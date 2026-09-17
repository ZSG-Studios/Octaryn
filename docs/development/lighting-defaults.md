# Screenshot lighting defaults — 2026-09-17

Fresh lighting settings and omitted fields in version-1 lighting settings use:

| Setting | Default |
| --- | ---: |
| Ambient strength | 0.65 |
| Sun strength | 0.75 |
| Fog distance | 1024 |
| Sky ambient floor | 0.25 |

`Source/Settings/LightingSettings/LightingSettings.cpp` and
`Source/Ui/LightingPanel/LightingPanel.cpp` under `octaryn-client` define these
native and deserialization defaults. Explicit saved values still deserialize
normally and use the existing bounds. No settings file migration or rewrite is
performed. Sun fallback strength remains 1.

The basegame `Assets/Ui/game.rml` declares ranges but no separate initial/reset
values. Client `GameUiUpdate.cpp` populates the controls from `LightingPanel`.
`WorldSession.cpp` supplies the loaded fog distance to the renderer. The renderer's
`WorldSceneSettings` and initial `WorldRenderer` fog values also use 1024, so
direct renderer callers receive the same default. Derived player-light values
are not persisted defaults.

Both changed C++ translation units compiled into isolated validation objects:
`logs/build/lighting-defaults-compile.log`. The existing settings probe passed
68 checks under `logs/client/lighting-defaults-settings-hvw1ixi0/probe.log`;
those checks cover runtime settings persistence and bounds, not lighting-panel
deserialization or visual rendering. No new GPU qualification is claimed.
