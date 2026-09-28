# Tile measurement evidence

The tile report schema is now version 3. `tools/validation/tile_metric_records.py`
validates the native owner records consumed by `tile_performance_report.py`:
uploads, resource setup/allocation, ray preparation and pumping, compaction,
publication, residency state, and readiness deadlines.

Recognized records must begin at a line boundary and contain their required
fields. Invalid numeric values, nonfinite or negative costs, fractional counters,
duplicate fields, embedded records, and inconsistent field sets within one
process log fail the report. Missing optional historical fields remain explicitly
unavailable. An absent maximum upload-call counter or request-duration counter
produces `null`, not a measured zero. Field and marker counts accompany the
distributions.

This validates emitted records, not complete event delivery. These unversioned
engine records have no sequence numbers or expected counts. A wholly deleted
record cannot be detected reliably; an unobserved marker does not prove that its
work never occurred. Older binaries may omit zero-byte material upload pumps.
The report therefore always leaves `complete_event_coverage` false and describes
these limits. Deadline records and successful publication durations also do not
measure cancelled or unfinished request latency. No process-launch-to-full-world
clock is inferred from the authority-ready marker.

## Separate capture streams

`capture_watchdog.run_capture` writes child stdout to `client.log` and stderr to
`client.stderr.log`. After the entire owned process tree has closed, it appends
the original stderr bytes to `client.log` between
`capture_stderr_append ordering=nonchronological` markers. Existing error scans
still see stderr diagnostics, and the original separate file remains available.
Appended diagnostics must not be assigned chronology from their final position.
If teardown itself fails, the separate stderr file is retained without reading
or appending a stream that may still have live writers.

This prevents native stdout metrics from competing with managed or native
stderr writers on the same file handle. It does not serialize competing writers
within stderr, and does not establish complete diagnostic-text delivery. The
process priority, ownership, timeout, frame stall, startup, and shutdown watchdog
policies are unchanged.

## Focused verification

- `python -m unittest discover -s tools/validation -p test_tile_performance_report.py`:
  11 tests passed. Coverage includes malformed/interleaved records, nonfinite and
  missing fields, inconsistent optional fields, real zero values, unavailable
  historical counters, paths with spaces, and existing frame/capture accounting.
- `python -m unittest discover -s tools/validation -p 'test_capture_*.py'`:
  35 tests passed. A real process concurrently writes both streams byte by byte;
  all 64 tile metric records and 64 error diagnostics survive exactly. Other cases
  preserve raw stderr after timeout and failed teardown, and retain the existing
  Windows priority, child-process cleanup, and watchdog checks.

Re-reading the completed build27 quality logs found one concrete damaged BC7
record in `logs/client/hq200-build27-tiled-codecs/quality/bc7/tiles-dx12-nwsinsgr/client.log`:
line 1564 contains `tile_readiness_deadline id=12remote_session_window...`, with
the remainder of tile ID 125 on the next line. The stricter parser rejects this
record. The lossless `tiles-dx12-jrikk_dp` metrics pass record validation. Original
results were not rewritten. This rejection concerns streaming metric integrity;
the separately joined capture metadata still passes the strengthened paired
image-identity audit in `quality/comparison/quality-identity-audit.json`.

These focused checks are neither rendering acceptance nor performance results.
New matched loading captures are required to exercise the separated streams in
the actual client.
