# Hardware profile evidence and analysis boundary

The pinned tools are installed under
`build/dependencies/tools/rdts/package/RadeonDeveloperToolSuite-2026-05-28-1806/`:
`RadeonDeveloperPanelCLI.exe` (RDP3.5), `RadeonDeveloperServiceCLI.exe`,
`RadeonGPUProfiler.exe` (RGP2.7), RGA, RRA and RMV. The central registry package
SHA256 is `c8ec76abfba6646d0a388f169ceb608f142915997986569ce0e449711ae0720d`.
This archive identity is not a recorded per-executable capture-time hash.

## Existing capture

`logs/client/rgp/hardware-mad7tbhz/frame.rgp` is221,753,393B. Its companion
`commands.json`, `profiler.log` and `result.json` record a successful Vulkan
instruction/counter capture, triggered after240 timing-CSV rows; runtime exit0.
The exact captured engine frame was not recorded by this trigger, so this is not
an exact-phase matched backend comparison. Counter-processing success does not
prove every individual counter is present or that the profile has been analyzed.

Runtime receipts are under
`logs/client/rgp/hardware-mad7tbhz/runtime/map-vulkan-on-0-54z0jvm0/`:

| Receipt field | Recorded value |
|---|---|
| API / adapter |Vulkan / AMD Radeon RX9070XT|
| Driver in `client-build.json` |32.0.31041.1004|
| Executable SHA256 |`50077a3324e749b8e1327e19c9d6e69f1caae97c4b487b6f74dacee4809d6e4f`|
| Map SHA256 |`55cbe3aea10abc2b3857184dcf6a7f63285e4445bb16cea146ee453efe9d8f74`|
| MapReflections.slang SHA256 |`133e0bca6420ab563793c9a40e3098e7ff3aacfa0721d926741580510f81bd12`|
| Output / reflection dimensions |2560×1440 /2560×1440|
| Render dimensions field |null in the historical receipt|
| Settings |Upscaler off, RT enabled, Ultra shadows/reflections, direct draws, LOD0|
| Workload |900 bounded hidden frames; no camera-motion request|
| Diagnostic watchdog / qualification |250ms / `timing_qualification=false`|

`client-build.json` preserves additional shader/DLL/server hashes, but uses
historical identity schema2 rather than current schema3 coverage. `settings.json`
and `lighting.json` preserve exact requested settings. `result.json` records
`uncapped_fps=false`; do not treat the nominal settings frame cap as proof of
uncapped execution. These identities describe that historical capture only,
not the current build33 engine.

The profiler log explicitly records temporary peak-clock selection followed by
restoration. The old helper docstring's no-clock-change claim was incorrect and
has been corrected without runtime changes. Its250ms watchdog is a diagnostic
capture setting, not the ordinary qualification threshold. No FPS or clock-neutral
performance result may be inferred from this instrumented run.

## Supported workflow and current blocker

AMD documents headless **capture** through RDP CLI, including instruction tracing,
hardware counters and frame/dispatch triggers. Start headless RDS before the panel
to avoid automatic startup of the tray service. A fresh coordinated capture must
use the hidden1440 harness, exact workload identities and readiness/phase records;
never overlap normal timing qualification. [RDP CLI](https://gpuopen.com/manuals/rdp_manual/radeon_developer_panel_cli/),
[headless service](https://gpuopen.com/manuals/rdp_manual/radeon_developer_service/).

The packaged RGP manual and AMD's official documentation identify RGP as a GUI
analysis tool. They document occupancy, cache counters, synchronization/barriers
and instruction analysis but no supported headless `.rgp`-to-CSV analysis command
was found. AMD's public tracker still has open requests for batch analysis
([#124](https://github.com/GPUOpen-Tools/radeon_gpu_profiler/issues/124)) and timing/
counter export ([#112](https://github.com/GPUOpen-Tools/radeon_gpu_profiler/issues/112)),
checked2026-09-27. These requests support the documented gap; absence of a found
command is not a claim about every undocumented binary switch.
[Official RGP overview](https://github.com/GPUOpen-Tools/radeon_gpu_profiler/blob/master/docs/source/index.rst).

Consequently the existing trace does not yet explain the DX12/Vulkan difference.
The repository prohibits UI automation, and no custom unsupported decoder is
being introduced. RGA's static/compiler statistics are not measured occupancy,
cache traffic or barrier stalls. Microsoft documents `pixtool` capture analysis
and event/counter CSV export, but PIX was not found in the targeted Program Files,
LocalAppData or PATH locations; it has not been installed. PIX playback itself
uses the GPU and would require a separate coordinated run.
[Official PIX CLI](https://devblogs.microsoft.com/pix/pixtool/).
