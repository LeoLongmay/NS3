import numpy as np
import matplotlib.pyplot as plt
import os
from RTT_process import filter_and_sort_rtt_file
from deduplicate import deduplicate_sorted_rtt_file

# Define the list of algorithms to compare (you can add/remove algorithms here)
algorithms = ['DCQCN', 'Timely', 'IE-LPCC', "2P-LPCC", "beta-LPCC", 'LPCC']
# algorithms = ['DCQCN', 'Timely']

plots_dir = "./rtt_data/"
plots_output_dir = "./plot_workload/"

# Color list for each algorithm's curve - YOU CAN MODIFY THESE COLORS AS NEEDED
# Common color options: 'blue', 'red', 'green', 'orange', 'purple', 'black', etc.
curve_colors = {}
curve_colors["DCQCN"] = 'xkcd:grass green'
curve_colors["IE-LPCC"] = 'xkcd:blue'
curve_colors["Timely"] = 'xkcd:purple'
curve_colors["2P-LPCC"] = 'xkcd:grey'
curve_colors["beta-LPCC"] = 'xkcd:brown'
curve_colors["LPCC"] = 'xkcd:orange'

# Set global font size to 18 (applies to all text elements)
plt.rcParams.update({'font.size': 18})

# Initialize a list to store processed data for plotting
plotting_data = []

# Iterate through each algorithm to read data and calculate CDF
for algo_name in algorithms:
    # Construct the full file path
    file_path = f"{plots_dir}{algo_name}_rtt.out"

    # Check if the file exists to avoid errors
    if not os.path.exists(file_path):
        print(f"Warning: File {file_path} does not exist, skipping this algorithm...")
        continue

    filter_and_sort_rtt_file(file_path)
    deduplicate_sorted_rtt_file(file_path)

    # Read and process RTT data from the file
    try:
        with open(file_path, 'r') as file:
            # Read all lines, remove empty lines, convert to float
            rtt_data = [
                float(line.strip()) / 5000
                for line in file
                if line.strip() and not line.strip().startswith('#')  # Skip comments/empty lines
            ]
    except Exception as e:
        print(f"Error reading {file_path}: {str(e)}, skipping...")
        continue

    # Calculate CDF (Cumulative Distribution Function)
    sorted_rtt = np.sort(rtt_data)  # Sort RTT values in ascending order
    cdf_values = np.arange(1, len(sorted_rtt) + 1) / len(sorted_rtt)  # Cumulative probability

    # Store algorithm name, sorted RTT, and CDF values
    plotting_data.append((algo_name, sorted_rtt, cdf_values))

# Create the plot
fig, ax = plt.subplots()  # Set figure size (width, height) in inches

linewidth = 2

# Plot CDF curve for each algorithm
for idx, (name, rtt, cdf) in enumerate(plotting_data):
    if name == "IE-LPCC":
        ax.plot(rtt, cdf, label="I/E-LPCC", color=curve_colors[name], linewidth=linewidth)
    elif name == "beta-LPCC":
        ax.plot(rtt, cdf, label=r"$\delta$-LPCC", color=curve_colors[name], linewidth=linewidth)
    else:
        ax.plot(rtt, cdf, label=name, color=curve_colors[name], linewidth=linewidth)

all_rtt_values = []
for _, rtt, _ in plotting_data:
    all_rtt_values.extend(rtt)
min_rtt = np.min(all_rtt_values)
max_rtt = np.max(all_rtt_values)
# Set left to 90% of min RTT (avoid 0) and right to 110% of max RTT (add small margin)
ax.set_xlim(left=1.6, right=45)
ax.set_ylim(0, 1.0)

ax.set_xticklabels(['10', '15', '20', '25', '30'])

# Configure plot labels and legend
ax.set_xlabel('RTT (ms)')
ax.set_ylabel('CDF')
ax.legend(loc='lower right', frameon=False)  # Legend position (adjustable: 'upper left', 'center', etc.)

# Ensure no elements are cut off
plt.tight_layout()

# Save the plot (uncomment the line below to save, adjust dpi for resolution)
plt.savefig(f'{plots_output_dir}rtt_cdf_comparison.pdf', dpi=300, bbox_inches='tight')

# Display the plot
# plt.show()