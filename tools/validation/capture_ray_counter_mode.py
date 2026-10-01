"""Select and prove compiled ray counters independently of runtime collection."""
import re
import sys

KEY = 'OCTARYN_CLIENT_RAY_COUNTERS'
MARKER = re.compile(r'^ray_diagnostic_mode requested=(auto|0|1) resolved=([01]) '
                    r'actual_compiled=([01]) collecting=([01]) cache_variant=(raycounters[01]) frozen=device$')


def add_counter_option(parser):
    parser.add_argument('--ray-counter-shader', choices=('auto', '0', '1'), default='auto',
                        help='Auto removes counters only on qualified Windows DX12; collection always requires 1')


def resolve_counter_mode(requested, collecting, backend='dx12', platform=None):
    if requested not in ('auto', '0', '1'):
        raise ValueError('Ray counter shader requires auto, 0 or 1')
    qualified = (platform or sys.platform) == 'win32' and backend == 'dx12'
    compiled = bool(collecting) or not qualified if requested == 'auto' else requested == '1'
    if collecting and not compiled:
        raise ValueError('Ray diagnostic collection requires counters compiled in')
    return dict(ray_counter_shader_requested=requested, ray_counter_shader_resolved=compiled,
                ray_counter_shader_actual=None, ray_counter_collection_requested=bool(collecting))


def apply_counter_mode(requested, collecting, env, backend='dx12'):
    mode = resolve_counter_mode(requested, collecting, backend)
    env.pop(KEY, None)
    if requested != 'auto':
        env[KEY] = requested
    return mode


def inspect_counter_mode(log, mode):
    lines = [line for line in log.splitlines() if 'ray_diagnostic_mode' in line]
    if not lines:
        raise ValueError('Executable did not report compiled ray-counter mode')
    expected = mode['ray_counter_shader_resolved']
    for line in lines:
        match = MARKER.fullmatch(line)
        if not match:
            raise ValueError('Damaged or unsupported ray-counter mode marker')
        requested, resolved, actual, collecting, namespace = match.groups()
        if (requested != mode['ray_counter_shader_requested'] or (resolved == '1') != expected or
                (actual == '1') != expected or (collecting == '1') != mode['ray_counter_collection_requested'] or
                namespace != 'raycounters' + str(int(expected))):
            raise ValueError('Compiled ray-counter activation differs from capture request')
    return dict(mode, ray_counter_shader_actual=expected)
