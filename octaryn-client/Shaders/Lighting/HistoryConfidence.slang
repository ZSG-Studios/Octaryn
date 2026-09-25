#ifndef OCTARYN_HISTORY_CONFIDENCE
#define OCTARYN_HISTORY_CONFIDENCE
// Compare short histories over matching surfaces, allowing sampling variation.
float lighting_history_confidence(float3 mean,float3 previous,float3 variance,float samples) {
    float3 deviation=sqrt(max(variance,0)/max(samples,1));
    float3 scale=max(max(mean,previous),.04);
    float3 excess=max(abs(mean-previous)-2*deviation-.02*scale-.002,0);
    float3 change=saturate(excess/(.12*scale+.004));
    return 1-max(change.x,max(change.y,change.z));
}
#endif
