import torch
import numpy as np
from torch import Tensor, nn
from diffusion import DoubleCritic, Diffusion, MLP
from stable_baselines3.common.type_aliases import PyTorchObs
from stable_baselines3.common.policies import ActorCriticPolicy


class DiffusionNetwork(nn.Module):
    """
    Custom network for policy and value function.
    It receives as input the features extracted by the features extractor.

    :param feature_dim: dimension of the features extracted with the features_extractor (e.g. features from a CNN)
    :param last_layer_dim_pi: (int) number of units for the last layer of the policy network
    :param last_layer_dim_vf: (int) number of units for the last layer of the value network
    """

    def __init__(
            self,
            feature_dim: int,
            n_timesteps: int = 5,
            hidden_sizes: int = 256,
            beta_schedule: str = "vp",
            last_layer_dim_pi: int = 64,
            last_layer_dim_vf: int = 64,
    ) -> None:
        super().__init__()
        device = "cuda" if torch.cuda.is_available() else "cpu"

        model = MLP(
            state_dim=feature_dim,
            action_dim=last_layer_dim_pi,
            hidden_dim=hidden_sizes
        ).to(device)
        self.policy_net = Diffusion(
            model=model,
            max_action=1.0,
            action_dim=last_layer_dim_pi,
            beta_schedule=beta_schedule,
            n_timesteps=n_timesteps
        ).to(device)

        self.value_net = DoubleCritic(
            state_dim=feature_dim,
            action_dim=last_layer_dim_vf,
            hidden_dim=hidden_sizes
        ).to(device)

        self.latent_dim_pi = last_layer_dim_pi
        self.latent_dim_vf = last_layer_dim_vf

    def forward(self, features: Tensor):
        return self.forward_actor(features), self.forward_critic(features)

    def forward_actor(self, features: Tensor) -> Tensor:
        return self.policy_net(features)

    def forward_critic(self, features: Tensor) -> Tensor:
        return self.value_net.q_min(features)


class DiffusionActorCriticPolicy(ActorCriticPolicy):
    def __init__(self, *args, **kwargs) -> None:
        self.beta_schedule = kwargs['beta_schedule']
        self.last_layer_dim_pi = kwargs['last_layer_dim_pi']
        self.last_layer_dim_vf = kwargs['last_layer_dim_vf']
        self.hidden_sizes = kwargs['hidden_sizes']
        self.n_timesteps = kwargs['n_timesteps']
        del kwargs['beta_schedule']
        del kwargs['last_layer_dim_pi']
        del kwargs['last_layer_dim_vf']
        del kwargs['hidden_sizes']
        del kwargs['n_timesteps']
        super().__init__(*args, **kwargs)

    def _build_mlp_extractor(self) -> None:
        assert isinstance(self.features_dim, int), \
            "To use the custom actor-critic network, you need to pass features_dim to the constructor"
        self.mlp_extractor = DiffusionNetwork(self.features_dim, self.n_timesteps, self.hidden_sizes,
                                              self.beta_schedule, self.last_layer_dim_pi, self.last_layer_dim_vf)


class DiffusionActorCriticPolicyV2(ActorCriticPolicy):
    def _build_mlp_extractor(self) -> None:
        assert isinstance(self.features_dim, int), \
            "To use the custom actor-critic network, you need to pass features_dim to the constructor"
        action_dim = int(np.prod(self.action_space.shape))
        self.mlp_extractor = DiffusionNetwork(self.features_dim, action_dim, action_dim)

    def forward(self, obs: Tensor, deterministic: bool = False):
        """
        Forward pass in all the networks (actor and critic)

        :param obs: Observation
        :param deterministic: Whether to sample or use deterministic actions
        :return: action, value and log probability of the action
        """
        features = self.extract_features(obs)
        if self.share_features_extractor:
            latent_pi, latent_vf = self.mlp_extractor(features)
        else:
            pi_features, vf_features = features
            latent_pi = self.mlp_extractor.forward_actor(pi_features)
            latent_vf = self.mlp_extractor.forward_critic(vf_features)
        # Evaluate the values for the given observations
        values = self.value_net(latent_vf)
        distribution = torch.distributions.Dirichlet(latent_pi)
        actions = distribution.sample() if not deterministic else distribution.mean
        log_prob = distribution.log_prob(actions)
        actions = actions.reshape((-1, *self.action_space.shape))
        return actions, values, log_prob

    def evaluate_actions(self, obs: PyTorchObs, actions: Tensor):
        """
        Evaluate actions according to the current policy,
        given the observations.

        :param obs: Observation
        :param actions: Actions
        :return: estimated value, log likelihood of taking those actions
            and entropy of the action distribution.
        """
        # Preprocess the observation if needed
        features = self.extract_features(obs)
        if self.share_features_extractor:
            latent_pi, latent_vf = self.mlp_extractor(features)
        else:
            pi_features, vf_features = features
            latent_pi = self.mlp_extractor.forward_actor(pi_features)
            latent_vf = self.mlp_extractor.forward_critic(vf_features)
        distribution = torch.distributions.Dirichlet(latent_pi)
        log_prob = distribution.log_prob(actions)
        values = self.value_net(latent_vf)
        entropy = distribution.entropy()
        return values, log_prob, entropy

    def get_distribution(self, obs: PyTorchObs) -> torch.distributions.Distribution:
        """
        Get the current policy distribution given the observations.

        :param obs:
        :return: the action distribution.
        """
        features = self.extract_features(obs)
        pi_features = features[0] if not self.share_features_extractor else features
        latent_pi = self.mlp_extractor.forward_actor(pi_features)
        return torch.distributions.Dirichlet(latent_pi)

    def _predict(self, observation: PyTorchObs, deterministic: bool = False) -> Tensor:
        """
        Get the action according to the policy for a given observation.

        :param observation:
        :param deterministic: Whether to use stochastic or deterministic actions
        :return: Taken action according to the policy
        """
        distribution = self.get_distribution(observation)
        actions = distribution.sample() if not deterministic else distribution.mean
        return actions


class DiffusionActorCriticPolicyDeterministic(DiffusionActorCriticPolicy):
    def forward(self, obs: Tensor, deterministic: bool = False):
        return super().forward(obs, True)


class DiffusionActorCriticPolicyV2Deterministic(DiffusionActorCriticPolicyV2):
    def forward(self, obs: Tensor, deterministic: bool = False):
        return super().forward(obs, True)
