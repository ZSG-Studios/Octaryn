# Menu/session rejoin qualification

Updated 2026-09-17.

The client previously evicted the old world on disconnect by setting the renderer
center to `(4000000, 4000000)` and its radius to zero. The next session renders
loading frames before its authoritative player pose sets a new streaming center.
Those frames passed a zero maximum draw distance to `render_clouds`, which rejects
it. The frame failure exited the client. Keep the configured radius during the
off-world eviction so loading frames retain valid presentation parameters.

`--validate-session-rejoin` explicitly exercises three world sessions and two
returns through the real menu/session owners. Each session waits for an
authoritative pose, all requested columns, completed meshes, a dismissed loading
screen, and at least 360 rendered frames. It then requests the existing disconnect
action. A session that cannot become ready within 60 seconds fails qualification.
The flag requires an isolated `OCTARYN_CLIENT_WORLD_PATH`, plus `--play-world 1`
for an existing isolated world or `--connect host:port` for a dedicated server.
It is incompatible with other qualification/frame/benchmark modes.

Native Windows DX12 / RX 9070 XT evidence:

- `logs/client/session-rejoin-before.log`: session one completes; session two exits
  with `world_frame_failed` before receiving a player pose.
- `logs/client/session-rejoin-trace.log`: temporary stage tracing isolated the
  failure to the forward/cloud pass. Temporary tracing was removed after diagnosis.
- `logs/client/session-rejoin-after.log`: exit 0, all three local sessions reach
  81 resident columns; `session_rejoin=passed sessions=3 menu_returns=2`.
- `logs/client/session-rejoin.bmp` and `.png`: actual GPU capture inspected;
  terrain, foliage, hotbar, and player hands are present.
- `octaryn_client_app` focused native build passes.

Initial remote qualification exposed a separate publication-tracker problem:
the second connection received a pose but no terrain baseline. Accepted peer
attachment now resets the session publication tracker without clearing durable
edits. The fixed remote qualifier passed all three sessions and two menu returns;
both processes exited 0. Server trace records exactly three terrain baselines
for three accepted peers, while 462 authority/player updates continued.

- `logs/client/rejoin-fixed-20260917-024218.log`
- `logs/server/rejoin-fixed-20260917-024218.log`
- Final full release bundles: `logs/build/rejoin-final-bundles.log`.
- Final static validation: `logs/build/rejoin-final-static.log`.

No OS input injection or UI automation was used. The dedicated qualification
server was a newly created child process and only that owned process was stopped.
Linux and macOS runtime verification are outstanding.
