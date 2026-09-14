#ifndef OCTARYN_RESERVOIR_SLANG
#define OCTARYN_RESERVOIR_SLANG
#ifdef __cplusplus
#define RESTIR_REF(T) T&
#else
#define RESTIR_REF(T) inout T
#endif
// Bitterli et al. 2020: weight=pHat/q, W=sumWeights/(M*pHat(selected)).
struct Reservoir {
    uint light; float2 sample; float weightSum;
    float target; float M; uint age; uint reuse;
};
uint restir_hash(uint x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);
}
float restir_random(RESTIR_REF(uint) state) {state=restir_hash(state);return float(state>>8)*(1.0/16777216.0);}
void reservoir_add(RESTIR_REF(Reservoir) r,uint light,float2 sample,float target,float weight,float M,uint age,uint reuse,RESTIR_REF(uint) rng) {
    if(!isfinite(weight) || weight<0 || M<=0)return;
    r.M+=M;r.weightSum+=weight;
    if(weight>0 && restir_random(rng)*r.weightSum<weight) {
        r.light=light;r.sample=sample;r.target=target;r.age=age;r.reuse=reuse;
    }
}
float reservoir_weight(Reservoir r) {
    return r.M>0 && r.target>0?r.weightSum/(r.M*r.target):0;
}
void reservoir_merge(RESTIR_REF(Reservoir) r,Reservoir source,float destinationTarget,float countLimit,uint reuse,RESTIR_REF(uint) rng) {
    float count=min(source.M,countLimit);
    // The represented count and sum are reduced together; never clamp M alone.
    reservoir_add(r,source.light,source.sample,destinationTarget,
        destinationTarget*reservoir_weight(source)*count,count,source.age+1,reuse,rng);
}
#undef RESTIR_REF
#endif
