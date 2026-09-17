# Server closure during receipt publication — 2026-09-17

## Recorded failure

Windows Application events 9736 (.NET Runtime 1026) and 9737 (Application Error
1000) identify `Octaryn.Server.exe`, PID 23176 (`0x5a88`), terminating at
04:51:12 EDT / 08:51:12 UTC on September 17. The executable was
`build/release-windows/client/bundle/server/Octaryn.Server.exe`; its recorded
creation time was 04:46:08.606 EDT. This is a later process than the previously
reported server PID 14224; do not attribute this event to that earlier PID.

The exception was `System.UnauthorizedAccessException`, native exception code
`0xe0434352`, from `BlockReceiptLedger.WriteAtomic` at the original line 114:
`File.Move(temporary, path, overwrite: true)`. The stack continues through
`SaveAndPublish`, `ModuleActivator.Tick`, the process tick bridge and the live
server host. `logs/server/local-session.log` records the same stack immediately
after accepted break command 111 at block `(101,55,-102)` in `open-world-v3`.

Evidence copied under `logs/server/closure-20260917-045112/`:

- `application-events.xml`: events 1000, 1001 and 1026 for this interval.
- `local-session.log`: server log including the fatal exception.
- `client-csv-tail.txt`: final samples; original CSV last-write 04:51:13 EDT.

`saves/dedicated/logs/server.log` last changed at 04:42 and ends in a peer timeout;
it does not describe this later local-session crash. No Octaryn crash markers
were found in the Local Temp root or its approved `opencode` directory. No
System warning/error events were returned for 04:45–04:52. No client crash event
was found in the inspected interval. The client deliberately exits its world
loop with result 1 when `session.running()` becomes false
(`WorldSession.cpp:176–179`), consistent with the server crash and CSV ending
one second later. The exact exited client PID/exit code was not captured.

## Fix and qualification

Receipt publication previously allowed a mailbox replacement failure to escape
the server tick. It now catches I/O/access failures **only around mailbox
publication**, retains staged receipts and dirty retirement state, and retries
on later ticks. It logs one deferral and one recovery per outage. The world-save
callback remains outside that catch: failed authoritative saves cannot publish
acceptance. Unpublished sequences remain unacknowledgeable, command deduplication
remains intact and the 256-entry admission limit still applies.

Windows qualification holds the destination open without delete sharing and
reproduces `UnauthorizedAccessException`, HRESULT `0x80070005`, at replacement.
It also covers a read-only destination, repeated failures, post-unlock ordered
publication, and acknowledgement retirement retry without new commands. The
existing save-failure barrier, bounds, reconnect and acknowledgement checks pass.

Command (exit 0):

```text
dotnet run --project tools/validation/BlockReceiptQualification/BlockReceiptQualification.csproj -p:OctarynBuildPresetName=release-windows -- logs/tools/block-receipt-closure-20260917
```

Evidence: `logs/tools/block-receipt-closure-20260917.log`.

The original denying handle/security actor is unknown. The native client reader
already specifies `FILE_SHARE_DELETE`, so the reproduction does not establish
that this reader caused the original denial. This fixes the proven unhandled
publication-failure path, not an inferred GPU/lighting fault. No saves were
modified by this investigation. The focused tool compiles the actual ledger
source. Production server-process qualification is recorded below; graphical
gameplay validation remains the coordinating session's responsibility.

## Production server-process contention proof

After the coordinating session built the server target, staged a private server
payload under `build/release-windows/tools/receipt-contention-server-20260917/`:
copied the existing server package, then overlaid the freshly built server
managed files and server/shared native DLLs. No canonical package rebuild or
modification was performed. Staged and source `Octaryn.Server.dll` SHA-256:
`009B2EF5CCAABEE852517DDAB7E28F1F2D1FB1B6B07AD483DB16AF7AEC930F39`.

`tools/validation/validate_block_receipt_contention.py` launches that production
executable with the normal supervised process-file protocol in a newly created
fixture world. It uses the existing authored-world identity and block-intent
format, reads the real authoritative player pose, and submits an in-reach break
through admission, native command drain and world persistence. No graphical
client, input injection or normal user-process control is involved.

Command (exit 0):

```text
python tools/validation/validate_block_receipt_contention.py --server-bundle build/release-windows/tools/receipt-contention-server-20260917 --evidence-root logs/server/receipt-contention-20260917
```

Passing evidence directory:
`logs/server/receipt-contention-20260917/receipt-contention-iexww6re/`.
`result.json` and `server.log` establish:

- Production server PID 8628 validated and activated bundled basegame.
- While an intentionally non-delete-sharing handle held `block_results.json`,
  request 1 broke `(0,162,3)`, accepted and changed by the real authority.
- Reading the saved aggregate during the lock found exactly the expected 63
  surviving overrides. The removed above-terrain fixture block returns to
  generated air, so edit-only persistence correctly removes that override.
- Server tick advanced from 1 to 31 while the mailbox remained locked and its
  receipt list remained empty. The server logged the same Windows access-denied
  exception as the original crash, now as deferred publication.
- Unlocking produced exactly one accepted receipt, sequence 1 / command 1 /
  revision 1, with authoritative block value 0. A correlated acknowledgement
  retired it and the server logged publication recovery.
- A fixture-only `shutdown.request` produced `octaryn_server_shutdown=1` and
  process exit 0. The tool never kills processes.

The first development attempt (`receipt-contention-2xq0kw05`) timed out because
the tool incorrectly expected an explicit persisted air override. Its server
also survived contention and stopped cleanly; the assertion was corrected to
the actual edit-only persistence representation before the passing run. The
passing server log additionally records one recovered pose-publication
contention event. This qualification establishes receipt-path survival,
durability ordering and recovery on Windows, not general filesystem immunity
or GPU/lighting correctness.
