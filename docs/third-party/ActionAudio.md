# ActionAudio

The client action-audio runtime (`octaryn-client/Source/Audio/ActionAudio`) is
first-party code. It links two third-party libraries, both staged with their
licenses in `Licenses/`:

- **OpenAL Soft** (static) — spatial audio device/context and voice playback.
- **miniaudio** (header-only) — helper decode/streaming.

Procedural sound synthesis (`Synthesis.cpp`) is first-party and dependency-free.
