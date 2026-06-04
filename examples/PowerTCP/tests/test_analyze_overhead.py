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
