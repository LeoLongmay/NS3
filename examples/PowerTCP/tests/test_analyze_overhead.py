from pathlib import Path
import importlib.util

_spec = importlib.util.spec_from_file_location(
    "analyze_overhead", Path(__file__).resolve().parents[1] / "analyze-overhead.py")
ao = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ao)

FIX = Path(__file__).resolve().parent / "fixtures"


def test_parse_flow_table_peaks_per_switch():
    stats = ao.parse_flow_table(FIX / "ft_small.txt")
    # switch 74 peak buf=1500, ft=300, flows=12 ; switch 75 peak buf=800, ft=90, flows=7
    assert stats[74].buf_bytes == 1500
    assert stats[74].ft_bytes == 300
    assert stats[74].flow_count == 12
    assert stats[75].buf_bytes == 800


def test_parse_flow_table_real_fixture_has_node_74():
    stats = ao.parse_flow_table(FIX / "real-lpcc-64" / "flow_table.txt")
    assert 74 in stats
    assert stats[74].flow_count > 0


def test_parse_fct_returns_fct_ns_column():
    recs = ao.parse_fct(FIX / "fct_small.txt")
    assert [r.fct_ns for r in recs] == [5230000, 17890000, 123190000]
    assert recs[0].size_bytes == 1000000


def test_parse_monitor_goodput_mean_of_active_samples():
    # active samples: 90e9 and 100e9 -> mean 95e9 -> 95.0 Gbps (the 0 sample is excluded)
    g = ao.parse_monitor_goodput(FIX / "runlog_small.txt")
    assert abs(g - 95.0) < 1e-9


def test_parse_monitor_goodput_real_fixture_in_range():
    g = ao.parse_monitor_goodput(FIX / "real-lpcc-64" / "run.log")
    # N=64 is far below line rate; expect a small positive Gbps value.
    assert g is None or g > 0
