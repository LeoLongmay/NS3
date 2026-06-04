# Reproducing the overhead tables

Both tables are produced by `analyze-overhead.py` (stdlib-only) from simulation dumps.

## LPCC buffer / flow-table memory vs flows (appended to `overhead.md`)
    examples/PowerTCP/run-overhead-buffer.sh            # default COUNTS=64,256,1024,4096,16384
    COUNTS=64,256,1024 examples/PowerTCP/run-overhead-buffer.sh   # quick subset

Monitors switch node 74 for throughput, RNG pinned (`--RngRun=1`), trace disabled.
The buffer table uses max-over-all-switches (node 74 is a spine with ~0 buffer; real
buffer pressure is at the leaf switches). Output dirs (`dump_burst_overhead/`,
`sweep_flows/`) are gitignored.

## Maintenance Goodput/P99 (-> `overhead-maint.md`)
    examples/PowerTCP/script-ab-maintenance.sh                    # default COUNTS=64,256,1024,4096,16384
    COUNTS=64 examples/PowerTCP/script-ab-maintenance.sh          # quick subset

Goodput = mean bottleneck-link throughput at node 74 over active samples. The historical
`overhead.md` recipe was not committed, so regenerated numbers match in magnitude/trend
but are not bit-identical.

## Tests
    cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py -v
