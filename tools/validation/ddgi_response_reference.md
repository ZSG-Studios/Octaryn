# DDGI response regression fixtures

Run `python tools/validation/validate_ddgi_response.py` on the native Windows
toolchain. Generated C++ and shader binaries stay under
`build/release-windows/tools/ddgi-response`.

## Reference inspected before repair

NVIDIA RTXGI-DDGI commit `f33e496ca31b3f0eec1c4e2cbaa8bb620e337fa6`:

- `rtxgi-sdk/shaders/ddgi/Irradiance.hlsl`: distance lookup encodes
  **negative** `biasedPosToAdjProbe`, i.e. probe-to-receiver direction.
- `rtxgi-sdk/shaders/ddgi/ProbeBlendingCS.hlsl`: excludes fixed classification
  rays from irradiance; cosine normalization stores half radiance and the
  sampling path multiplies by `2*pi` after decoding. Octaryn stores physical
  irradiance directly (`pi*weightedMean`) and diffuse feedback divides by `pi`.
  These conventions agree for a constant environment.
- RTXGI includes a brightness limiter and changed-light hysteresis in its
  encoded-domain filter. Octaryn's linear-domain positive-only limiter was not
  an unbiased stationary estimator. It was removed rather than copying the
   reference's encoded-domain tuning into different storage/sampling conventions.
- RTXGI takes `volume.probeHysteresis` once per executed blend. It does not raise
  that retention to elapsed display frames. The online source was inspected at
  [this pinned revision](https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/f33e496ca31b3f0eec1c4e2cbaa8bb620e337fa6/rtxgi-sdk/shaders/ddgi/ProbeBlendingCS.hlsl).
  Octaryn now retains mature history per observation, with sample-count warmup,
  exact explicit edit/reset rejection and at most 0.5 reactive retention.
  Unobserved time does not supply additional independent ray samples.

No reference implementation code was copied.

## What the fixtures exercise

- Production Slang integration/history functions compiled to C++: 12 constant
  environment/ray-tier cases; fixed geometry rays carrying deliberately huge
  radiance must not contribute; stationary sparse light samples retain their
  analytical mean; 15 diffuse-enclosure add/remove sequences agree with the
  analytical recurrence `E_next = h*E + (1-h)*(pi*S + rho*E)` and return to the
  original cold fixed point after removal. Explicit source-change history resets.
- Full production `ddgi_sample` compiled to C++: mirrored asymmetric occluders
  reject a bright probe blocked toward the receiver despite its opposite
  hemisphere being open. Coverage remains one. This tests actual octahedral
  storage, interpolation, visibility weighting and the hierarchy gather.
- Reinstating the former positive limiter or reversed distance direction in
   generated C++ must fail the same fixtures.
- Stationary Bernoulli sparse-light observations at 0.016, 0.5 and 1 second
  intervals retain the same mean and variance. The independently derived EMA
  variance is `rawVariance*(1-h)/(1+h)` for mature retention `h=0.94`.
  All three intervals measured mean 1.0816169 (ensemble expectation 1.0995575)
  and variance 0.1285831 (ensemble expectation 0.1335449), across 64,000 samples
  after 2,000 warmup observations. The deterministic periodic sparse-light test
  also retains its mean 1.0995575 within 0.0002.
- Warmup and reactive retention are checked across all three intervals. Both
  explicit edit and reset discard history exactly for dark/bright histories and
  incoming values from zero to 200. Four reactive observations reach 93.75% of a
  constant step before mature retention resumes. Production metadata countdown
  and native invalidation are outside this function-level fixture.
- Reinstating elapsed-time history decay in generated production C++ must fail
  specifically on sparse-observation variance, rather than merely failing to
  compile. At half-second spacing the old `pow(0.94,30)` replaces about 84% of
  history with one noisy observation, instead of the intended 6%.
- Production trace/update/seed compile to SPIR-V, DXIL and Metal source. Expected
  compiler diagnostics are entry-point renaming and SPIR-V ray-query capability
  promotion. Generated Metal is not macOS runtime qualification.

The CPU target cannot execute group synchronization; the update pass therefore
shares its integration/history functions with the CPU fixture. GPU barriers,
classification/relocation, resource bindings and actual scene convergence still
require the serial GPU qualification. In particular, the recorded fine-16 cold
fixture has tunnel probes that were active but had not traced since frames
100–120 at capture frames 665–975; a visually stable cold image is not evidence
that those probes reached steady state.
