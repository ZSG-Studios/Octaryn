"""Resolve qualified renderer defaults and verify the executable's selections."""
import re


def qualified_windows(backend, platform):
    return platform == 'nt' and backend in ('dx12', 'vulkan')


def map_only_traversal(requested, backend, queued, temporal, rays, platform):
    eligible = not queued and temporal == 'on' and rays == 'on'
    if requested and not eligible:
        raise ValueError('--rt-map-only requires ray tracing and temporal reflections without --rt-queued')
    return requested if requested is not None else qualified_windows(backend, platform) and eligible


def add_render_options(parser):
    options = (
        ('rt_map_only', '--rt-map-only', '--rt-generic', None, 'triangle-only reflection traversal'),
        ('cloud_occlusion', '--cloud-occlusion', '--cloud-original', None, 'exact opaque cloud rejection'),
        ('shadow_map_only', '--shadow-map-only', '--shadow-generic', None, 'triangle-only shadow traversal'),
        ('rt_deferred_material', '--rt-deferred-material', '--rt-eager-material', None,
         'direct-map material evaluation after visibility'),
    )
    for field, enabled, disabled, default, description in options:
        group = parser.add_mutually_exclusive_group()
        group.add_argument(enabled, dest=field, action='store_true', default=default, help=description)
        group.add_argument(disabled, dest=field, action='store_false', default=default,
                           help='explicit reference for ' + description)


def resolve_render_options(args, platform):
    fields = ('rt_map_only', 'cloud_occlusion', 'shadow_map_only', 'rt_deferred_material')
    args.requested_render_options = {key: getattr(args, key) for key in fields}
    args.rt_map_only = map_only_traversal(args.rt_map_only, args.backend, args.rt_queued,
                                        args.temporal_reflections, args.ray_tracing, platform)
    if args.cloud_occlusion is None:
        args.cloud_occlusion = qualified_windows(args.backend, platform)
    if args.shadow_map_only and args.ray_tracing != 'on':
        raise ValueError('--shadow-map-only requires ray tracing')
    if args.shadow_map_only is None:
        args.shadow_map_only = qualified_windows(args.backend, platform) and args.ray_tracing == 'on'
    deferred_eligible = args.rt_map_only and getattr(args, 'reflection_distance', 1024) > 0
    if args.rt_deferred_material and not deferred_eligible:
        raise ValueError('--rt-deferred-material requires active map-only temporal reflections without queues')
    if args.rt_deferred_material is None:
        args.rt_deferred_material = qualified_windows(args.backend, platform) and deferred_eligible


def apply_render_options(args, env):
    for field, key in (('rt_map_only', 'RT_MAP_ONLY'), ('cloud_occlusion', 'CLOUD_OCCLUSION'),
                       ('shadow_map_only', 'SHADOW_MAP_ONLY'), ('rt_deferred_material', 'RT_DEFERRED_MATERIAL')):
        env['OCTARYN_CLIENT_' + key] = str(int(getattr(args, field)))


def render_option_evidence(args):
    result = {}
    for field, requested in args.requested_render_options.items():
        result[field] = getattr(args, field)
        result[field + '_requested'] = requested
        result[field + '_active'] = None
    return result


def inspect_render_options(args, log, result):
    options = [('cloud_occlusion', r'clouds occlusion=([01]) samples_unchanged=1')]
    if args.ray_tracing == 'on':
        options.append(('shadow_map_only', r'rt_shadows map_only=([01]) sampling_unchanged=1'))
        if args.temporal_reflections == 'on':
            options += [('rt_map_only', r'map_reflections map_only=([01]) sampling_unchanged=1'),
                        ('rt_deferred_material', r'map_reflections deferred_material=([01]) direct_gi=[01] (?:candidate=1|qualified_default=[01])')]
    for field, pattern in options:
        modes = re.findall(pattern, log)
        if not modes or any((mode == '1') != getattr(args, field) for mode in modes):
            raise RuntimeError('Executable ' + field + ' differs from the resolved request')
        result[field + '_active'] = modes[-1] == '1'
