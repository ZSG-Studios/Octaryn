# Reproducible remote qualification

Tracked runner: `tools/validation/network/qualify_remote.py` (Python 3.12).
It starts the canonical dedicated server and either native client probe against
fresh, isolated world/cache directories. It does not build or package artifacts.

From the repository root on Windows:

```powershell
python -B tools/validation/network/qualify_remote.py --probe jump --max-takeoff-ms 50
python -B tools/validation/network/qualify_remote.py --probe jump --max-takeoff-ms 50 --simulate-min-delay-ms 30 --simulate-max-delay-ms 60 --simulate-loss-percent 2 --simulate-seed 20260917
python -B tools/validation/network/qualify_remote.py --probe blocks --simulate-min-delay-ms 30 --simulate-max-delay-ms 60 --simulate-loss-percent 2 --simulate-seed 20260917
```

Defaults use `build/release-windows/{client,server}/bundle` and
`build/release-windows/tools/native/bin`. Other hosts default to `release-linux`;
`--preset`, `--client-bundle`, `--server-bundle`, and `--probe-executable` allow
explicit artifact selection. Python path handling and process launch are portable;
this does not establish engine runtime qualification on other operating systems.
Relative paths resolve against the repository root even from another working directory.

Ports default to ephemeral allocation. To select ports explicitly, append
`--server-port 17651 --proxy-port 17652`. The proxy port is used when impairment
is enabled or when an explicit proxy port is supplied. Existing processes are
never stopped to free a port. The short server-port reservation/rebind race
causes startup failure if another process acquires it, not attachment to that process.

Each run creates a unique directory under
`build/<preset>/tools/validation/remote-<probe>-<timestamp>-<id>`.
`--work-dir <path>` instead requires a nonexistent directory. Both server world
and client cache are run-owned children; no existing save is reused or deleted.
Inherited `OCTARYN_*` overrides are cleared before assigning isolated paths.

Outputs:
- `logs/client/<run>.log` and `logs/client/<run>-session/`: native probe/session evidence.
- `logs/server/<run>.log`: dedicated-server evidence.
- `logs/tools/<run>.json`: commands, world paths, actual child exit codes, probe
  counters, cleanup actions, errors and final runner exit code.
- `logs/tools/<run>-proxy.json`: per-direction forwarding, random/bound drops,
  send errors, shutdown discards, queue peaks and scheduling lateness.

Timeouts are configurable through `--timeout-seconds` (180),
`--startup-timeout-seconds` (60), and `--shutdown-timeout-seconds` (15).
Cleanup writes explicit isolated shutdown markers, waits, and only then may
terminate/kill the exact child handles created by this run. Forced cleanup fails
qualification. Probe nonzero exits are preserved; timeout is 124, interruption
130, and setup/counter/cleanup failure 1. A zero probe exit without complete
success counters is a failure. Queue overflow/send errors also fail qualification.

The proxy bounds delay to 0..2000ms, loss to 0..25%, and queued traffic to 2048
packets / 8 MiB. Receive/send work is budgeted per iteration. A seed reproduces
loss/delay decisions for the same arrival order; operating-system scheduling and
network arrival order are not claimed deterministic.

Focused proxy checks (ephemeral local sockets only; no engine processes):

```powershell
python -B tools/validation/network/test_udp_impairment.py -v
python -B tools/validation/network/test_processes.py -v
```

These exercise packet and byte saturation, drop/discard accounting, seeded loss,
delay bounds, real bidirectional UDP forwarding, peer pinning, and shutdown with
queued traffic. Full jump/block qualification remains an explicit separate run.
The process check starts two disposable Python fixtures under the validation
build directory, verifies graceful marker shutdown and forced timeout cleanup,
and checks that stopping one child leaves the other running. It launches no engine.
All five focused checks passed on Python 3.12.10 on 2026-09-17; evidence is
`logs/tools/network-qualification-unit.log`.

## Provenance

Promoted from the ignored qualification harnesses `logs/tools/qualify_remote_jump.py`
and `logs/tools/udp_impairment.py` used by the 2026-09-17 protocol-4 qualification.
The original files and evidence remain intact. The tracked version adds generic
probe selection, ephemeral/configurable ports, fresh-directory enforcement,
portable launch paths, explicit child cleanup, and machine-readable results.
Earlier packaged impaired jump passes include
`logs/client/remote-jump-20260917-035931.log` (30–60ms, 2% loss) and
`logs/client/remote-jump-20260917-040045.log` (100–150ms, 5% loss). Those are provenance,
not executions of this promoted runner.
