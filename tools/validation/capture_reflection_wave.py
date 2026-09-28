"""Strict opt-in fused DX12 reflection wave control; observed width requires diagnostics."""
import re

KEY = 'OCTARYN_CLIENT_REFLECTION_WAVE_SIZE'
MODE = re.compile(r'^reflection_wave_mode requested=(0|32|64) forced=(0|32|64) telemetry=([01]) '
                  r'device_min=(\d+) device_max=(\d+) actual_observed=unknown frozen=device scope=fused_map_temporal$')
PATH = re.compile(r'^reflection_wave_path requested=(0|32|64) map_only=([01]) temporal=1 queued=0 applied=(0|32|64)$')


def add_wave_option(parser):
    parser.add_argument('--reflection-wave-size', type=int, choices=(0, 32, 64), default=0,
                        help='DX12 fused map-temporal reflection width; 0 leaves driver selection unchanged')


def resolve_wave_mode(width, backend, collecting):
    if width not in (0, 32, 64) or (width and backend != 'dx12'):
        raise ValueError('Forced reflection wave size requires DX12 and width 32 or 64')
    return dict(reflection_wave_requested=width, reflection_wave_actual=None,
                reflection_wave_telemetry=bool(collecting))


def apply_wave_mode(width, env):
    # Explicitly isolate inherited qualification controls, including the default.
    env[KEY] = str(width)


def inspect_wave_mode(log, mode):
    markers = [line for line in log.splitlines() if 'reflection_wave_mode' in line]
    if not markers:
        raise ValueError('Executable did not report reflection wave configuration')
    requested = mode['reflection_wave_requested']
    for line in markers:
        match = MODE.fullmatch(line)
        if not match:
            raise ValueError('Damaged reflection wave configuration')
        width, forced, telemetry, minimum, maximum = map(int, match.groups())
        if width != requested or forced != requested or bool(telemetry) != mode['reflection_wave_telemetry']:
            raise ValueError('Reflection wave activation differs from capture request')
        if requested and not (minimum <= requested <= maximum):
            raise ValueError('Requested reflection wave outside device capability')
    paths = [line for line in log.splitlines() if 'reflection_wave_path' in line]
    if requested and not paths:
        raise ValueError('Requested reflection wave did not reach fused reflection pipeline')
    for line in paths:
        match = PATH.fullmatch(line)
        if not match or int(match[1]) != requested or int(match[3]) != requested:
            raise ValueError('Invalid reflection wave pipeline activation')
        if requested and match[2] != '1':
            raise ValueError('Forced reflection wave reached wrong pipeline')
    return mode
