# World view default

2026-09-18: new settings, missing persisted `renderDistance` fields, and runtime
controls default to radius 4. At 32 blocks per column this is a 128-block radius
and an 81-column square. The view-distance UI displays blocks. Selectable radii
remain 4, 8, 12, 16, 20, 24 and 32; meshing retains full voxel detail.

Ordinary startup initializes radius 4, loads persisted settings, then applies an
explicit `--render-distance` override. The CLI's zero/default sentinel preserves
this precedence. Existing explicit saved radii, including 16 and 32, are retained.
`LocalSession` publishes the selected radius to the server's chunk-view intent;
the server uses that intent rather than an independent radius-16 default.

Preferences use `OCTARYN_CLIENT_SETTINGS_PATH` when set, otherwise SDL's
`ZSGStudios/Octaryn/client-settings.json` preference path. On this Windows machine
that is `%APPDATA%\ZSGStudios\Octaryn\client-settings.json`. This implementation
does not rewrite generated user preferences.

Focused Windows CPU settings validation passed 80 checks, including missing-field
default 4 and explicit 4/16/32 load/save preservation. Only the settings probe and
its CPU dependencies were built; no GPU or full-client execution was performed.
RT shadow/reflection defaults remain 1024 blocks and are separate controls.
