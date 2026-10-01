#ifndef OCTARYN_RAY_DIAGNOSTICS
#define OCTARYN_RAY_DIAGNOSTICS
#if OCTARYN_RAY_COUNTERS
RWStructuredBuffer<uint> rayDiagnosticCounters;
uniform uint rayDiagnosticsEnabled;
void ray_diagnostic(uint index) {
    if(rayDiagnosticsEnabled!=0)InterlockedAdd(rayDiagnosticCounters[index],1);
}
void ray_diagnostic_max(uint index,uint value) {
    if(rayDiagnosticsEnabled!=0)InterlockedMax(rayDiagnosticCounters[index],value);
}
#else
void ray_diagnostic(uint index) {}
void ray_diagnostic_max(uint index,uint value) {}
#endif
#if OCTARYN_RAY_COUNTERS && OCTARYN_RAY_WAVE_TELEMETRY
void ray_diagnostic_wave() {
    if(rayDiagnosticsEnabled!=0 && WaveIsFirstLane()) {
        uint width=WaveGetLaneCount();
        InterlockedAdd(rayDiagnosticCounters[21],1);
        InterlockedMax(rayDiagnosticCounters[22],width);
        InterlockedMin(rayDiagnosticCounters[23],width);
    }
}
#else
void ray_diagnostic_wave() {}
#endif
#endif
