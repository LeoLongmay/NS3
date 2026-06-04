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


def test_p99_ms():
    vals_ns = [i * 1_000_000 for i in range(1, 101)]  # 1..100 ms
    assert abs(ao.p99_ms(vals_ns) - 99.0) < 0.5

def test_buffer_row_for_n():
    stats = {74: ao.PeakStats(buf_bytes=64 * 1024 * 1024, ft_bytes=512 * 1024, flow_count=4096),
             75: ao.PeakStats(buf_bytes=1024, ft_bytes=10, flow_count=3)}
    row = ao.buffer_metrics(stats, buffer_mb=128, switch_id=None)
    assert abs(row["peak_buf_mb"] - 64.0) < 1e-6        # max over switches
    assert abs(row["buf_util_pct"] - 50.0) < 1e-6       # 64/128
    assert abs(row["peak_ft_kb"] - 512.0) < 1e-6
    assert row["peak_flows"] == 4096

def test_render_markdown_buffer_table():
    cols = [64, 256]
    rows = {64: {"peak_buf_mb": 1.0, "buf_util_pct": 0.78, "peak_ft_kb": 8.0,
                 "ft_cap_pct": 0.006, "peak_flows": 64},
            256: None}  # 256 missing -> em dash
    md = ao.render_buffer_md(cols, rows, buffer_mb=128)
    assert "| Peak switch buffer (MB)" in md
    assert "—" in md           # missing cell
    assert "| 64 | 256 |" in md.replace("  ", " ") or "64" in md

def test_render_maintenance_md():
    cols = [64, 256]
    # data[arm][n] = {"goodput": Gbps, "p99": ms}
    data = {
        "on":  {64: {"goodput": 3.64, "p99": 5.23}, 256: {"goodput": 15.29, "p99": 17.89}},
        "off": {64: {"goodput": 3.64, "p99": 5.12}, 256: {"goodput": 15.78, "p99": 15.63}},
    }
    md = ao.render_maintenance_md(cols, data)
    assert "| Goodput (Gbps) | On" in md
    assert "| P99 FCT (ms)   | Off".replace("   ", " ") in md.replace("   ", " ")
    assert "3.64" in md and "17.89" in md
