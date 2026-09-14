#ifndef OCTARYN_LIGHTING_SAMPLING
#define OCTARYN_LIGHTING_SAMPLING
// Integer avalanche hash and uniform [0,1) variates for probe ray integration.
uint lighting_hash(uint x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);
}
float lighting_random(inout uint state) {
    state=lighting_hash(state);return float(state>>8)*(1.0/16777216.0);
}
#endif
