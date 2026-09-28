"""Explicit virtual geometry capture settings and production-path evidence."""
import hashlib
import math
from pathlib import Path
import re


def add_virtual_geometry_options(parser):
    parser.add_argument('--virtual-geometry', type=Path, help='Explicit cooked virtual geometry cache; monolithic qualification only')
    parser.add_argument('--geometry-pool-mib', type=int, default=384, help='Virtual geometry residency pool in MiB')
    parser.add_argument('--geometry-pixels', type=float, default=1, help='Virtual geometry screen error budget in pixels')


def _digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def prepare_virtual_geometry(args, manifest, env):
    if not args.virtual_geometry:
        if args.geometry_pool_mib != 384 or args.geometry_pixels != 1:
            raise ValueError('Geometry settings require --virtual-geometry')
        return dict(enabled=False)
    if args.performance_profile != 'custom' or manifest.get('tiles') or manifest.get('tile_files'):
        raise ValueError('Virtual geometry qualification requires a custom monolithic map capture')
    if args.draw_mode != 'direct' or args.lod_pixels or args.map_occlusion:
        raise ValueError('Virtual geometry cannot be combined with legacy map LOD, meshlets or occlusion')
    if not 8 <= args.geometry_pool_mib <= 1024:
        raise ValueError('--geometry-pool-mib must be 8..1024')
    if not math.isfinite(args.geometry_pixels) or not 0 <= args.geometry_pixels <= 4:
        raise ValueError('--geometry-pixels must be finite and 0..4')
    path = args.virtual_geometry.resolve(strict=True)
    if not path.is_file() or not path.stat().st_size:
        raise ValueError('Virtual geometry cache must be a nonempty file')
    env.update(OCTARYN_CLIENT_VIRTUAL_GEOMETRY=str(path),
               OCTARYN_CLIENT_VIRTUAL_GEOMETRY_POOL_MIB=str(args.geometry_pool_mib),
               OCTARYN_CLIENT_VIRTUAL_GEOMETRY_PIXELS=str(args.geometry_pixels),
               OCTARYN_CLIENT_VIRTUAL_GEOMETRY_TIMING='1')
    return dict(enabled=True, cache_path=str(path), cache_sha256=_digest(path),
                cache_bytes=path.stat().st_size, pool_mib=args.geometry_pool_mib,
                pixels=args.geometry_pixels, mode='opt_in_monolithic',
                transparency='existing_forward', ray_geometry='existing_full_detail')


def inspect_virtual_geometry(log, requested, verify_asset=False):
    requested = requested or dict(enabled=False)
    if not requested['enabled']:
        if 'world_geometry_ready ' in log:
            raise RuntimeError('Virtual geometry activated without an explicit capture request')
        return requested
    ready = re.findall(r'^world_geometry_ready mode=opt_in_monolithic clusters=(\d+) pages=(\d+) '
                       r'root_pages=(\d+) slots=(\d+) pixels=([\d.eE+-]+) root_ms=([\d.eE+-]+) '
                       r'transparency=existing_forward rt=existing_full_detail$', log, re.MULTILINE)
    if len(ready) != 1:
        raise RuntimeError('Missing unique virtual geometry startup evidence')
    clusters, pages, roots, slots = map(int, ready[0][:4])
    pixels, root_ms = map(float, ready[0][4:])
    if min(clusters, pages, roots) <= 0 or roots > pages or roots > slots:
        raise RuntimeError('Invalid virtual geometry root/page readiness evidence')
    if slots != requested['pool_mib'] * 16 or not math.isfinite(pixels) or abs(pixels-requested['pixels']) > .000501:
        raise RuntimeError('Virtual geometry runtime settings differ from the capture request')
    if not math.isfinite(root_ms) or root_ms < 0:
        raise RuntimeError('Invalid virtual geometry startup duration')
    streams = re.findall(r'^world_geometry_stream frame=(\d+) selected=(\d+) resident_pages=(\d+) '
                         r'pending_pages=(\d+) gpu_bytes=(\d+) uploaded_bytes=(\d+) feedback_overflow=(\d+)$',
                         log, re.MULTILINE)
    records = [tuple(map(int, row)) for row in streams]
    if not records or not any(row[1] > 0 for row in records):
        raise RuntimeError('No actual virtual geometry opaque selection was recorded')
    if any(row[1] > clusters or row[2] < roots or row[2]+row[3] > slots or row[4] <= 0 for row in records):
        raise RuntimeError('Invalid virtual geometry streaming bounds')
    if 'world_geometry_failed ' in log:
        raise RuntimeError('Virtual geometry runtime reported failure')
    if verify_asset:
        path = Path(requested['cache_path'])
        if not path.is_file() or path.stat().st_size != requested['cache_bytes'] or _digest(path) != requested['cache_sha256']:
            raise RuntimeError('Virtual geometry cache changed during the capture')
    return dict(requested, observed=dict(clusters=clusters, pages=pages, root_pages=roots,
                slots=slots, pixels=pixels, root_ms=root_ms, streaming_samples=len(records),
                selected_max=max(row[1] for row in records), resident_pages_last=records[-1][2],
                uploaded_bytes_last=records[-1][5], feedback_overflow_last=records[-1][6]))


def inspect_opaque_submissions(log, virtual_geometry=None):
    if virtual_geometry and virtual_geometry.get('enabled'):
        inspect_virtual_geometry(log, virtual_geometry)
        return
    inspect_virtual_geometry(log, dict(enabled=False))
    draws = re.findall(r'map_draw forward=0 submitted=(\d+) culled=(\d+)', log)
    indirect = re.findall(r'map_draw forward=0 indirect=1 command_slots=(\d+) cpu_submissions=1', log)
    meshlets = re.findall(r'map_draw forward=0 meshlet=1 meshlets=(\d+) cpu_submissions=1', log)
    if not any(int(submitted) > 0 for submitted, _ in draws) and not any(int(slots) > 0 for slots in indirect+meshlets):
        raise RuntimeError('No opaque map draw submissions recorded')
