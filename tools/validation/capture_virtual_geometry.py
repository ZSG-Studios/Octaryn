"""Required paged map rendering settings and executed-path evidence."""
import hashlib
import math
import json
from pathlib import Path
import re


def add_virtual_geometry_options(parser):
    parser.add_argument('--virtual-geometry', type=Path, help='Retired cache override; use the dedicated cook and stream probes')
    parser.add_argument('--geometry-pool-mib', type=int, default=384, help='Maximum map geometry residency pool in MiB')
    parser.add_argument('--geometry-pixels', type=float, default=1, help='Geometry screen error budget in pixels')
    parser.add_argument('--geometry-occlusion', choices=('on', 'off'), default='off', help='Experimental history occlusion')
    parser.add_argument('--scene-stream', action='store_true', help='Exercise spatial residency for a cooked scene catalog')
    parser.add_argument('--scene-continuity', action='store_true', help='Record every scene publication and ray continuity frame')


def _digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def prepare_virtual_geometry(args, manifest, env):
    tiled = bool(manifest.get('tiles') or manifest.get('tile_files'))
    scene_stream = bool(getattr(args, 'scene_stream', False))
    continuity = bool(getattr(args, 'scene_continuity', False))
    if continuity and not scene_stream:
        raise ValueError('--scene-continuity requires --scene-stream')
    if scene_stream and not manifest.get('scene_catalog'):
        raise ValueError('--scene-stream requires a scene catalog manifest')
    if args.virtual_geometry:
        raise ValueError('Map geometry is prepared automatically; qualify explicit caches with the cook and stream probes')
    if args.draw_mode != 'direct' or args.lod_pixels or args.map_occlusion:
        raise ValueError('Required virtual geometry cannot use legacy map draw, LOD or occlusion modes')
    if not 8 <= args.geometry_pool_mib <= 1024:
        raise ValueError('--geometry-pool-mib must be 8..1024')
    if not math.isfinite(args.geometry_pixels) or not 0 <= args.geometry_pixels <= 4:
        raise ValueError('--geometry-pixels must be finite and 0..4')
    if tiled and args.geometry_pool_mib != 384:
        raise ValueError('Tiled geometry uses admitted per-tile pools and automatically cooked caches')
    env.update(OCTARYN_CLIENT_VIRTUAL_GEOMETRY_POOL_MIB=str(args.geometry_pool_mib),
               OCTARYN_CLIENT_VIRTUAL_GEOMETRY_PIXELS=str(args.geometry_pixels),
               OCTARYN_CLIENT_VIRTUAL_GEOMETRY_TIMING='1',
               OCTARYN_CLIENT_VIRTUAL_GEOMETRY_OCCLUSION='1' if args.geometry_occlusion == 'on' else '0')
    result = dict(enabled=True, pool_mib=args.geometry_pool_mib, pixels=args.geometry_pixels,
                  mode='required', transparency='sorted_forward', ray_geometry='virtual_geometry', scene_stream=scene_stream)
    if scene_stream:
        env['OCTARYN_CLIENT_SCENE_STREAM'] = '1'
    if continuity:
        env['OCTARYN_CLIENT_SCENE_CONTINUITY'] = '1'
        result['scene_continuity'] = True
    if manifest.get('scene_catalog'):
        manifest_path = args.manifest.resolve() if args.manifest else Path(args.client_bundle_root).resolve()/'Client/Assets/Maps/map.json'
        catalog_reference = Path(manifest['scene_catalog'])
        if not catalog_reference.is_absolute() and '..' in catalog_reference.parts:
            raise ValueError('Scene catalog cannot escape through a relative path')
        path = (manifest_path.parent/catalog_reference).resolve(strict=True)
        if (not catalog_reference.is_absolute() and not path.is_relative_to(manifest_path.parent)) or path.stat().st_size > 32*1024**2:
            raise ValueError('Invalid or oversized scene catalog path')
        scene = json.loads(path.read_text(encoding='utf-8'))
        result['scene_catalog'] = dict(path=str(path), sha256=_digest(path), bytes=path.stat().st_size,
            version=scene['version'], source_hash=scene['source_hash'], parts=len(scene['parts']),
            prepared_parts=sum(bool(part['geometry']) for part in scene['parts']), instances=len(scene['instances']),
            unique_triangles=scene['unique_triangles'], instanced_triangles=scene['instanced_triangles'])
    return result


def _initialized(log, requested):
    rows = re.findall(r'^world_geometry_initialized mode=required asset=([a-f0-9]{64}) '
                      r'clusters=(\d+) pages=(\d+) root_pages=(\d+) slots=(\d+) '
                      r'pixels=([\d.eE+-]+) root_ms=([\d.eE+-]+) '
                      r'transparency=sorted_forward rt=virtual_geometry$', log, re.MULTILINE)
    assets = {}
    for key, clusters, pages, roots, slots, pixels, root_ms in rows:
        clusters, pages, roots, slots = map(int, (clusters, pages, roots, slots))
        pixels, root_ms = float(pixels), float(root_ms)
        if min(clusters, pages, roots, slots) <= 0 or roots > min(pages, slots):
            raise RuntimeError('Invalid virtual geometry root/page allocation evidence')
        if slots != min(pages, requested['pool_mib'] * 16) or not math.isfinite(pixels) or abs(pixels-requested['pixels']) > .000501:
            raise RuntimeError('Virtual geometry runtime settings differ from the capture request')
        if not math.isfinite(root_ms) or root_ms < 0:
            raise RuntimeError('Invalid virtual geometry startup duration')
        record = dict(clusters=clusters, pages=pages, root_pages=roots, slots=slots, pixels=pixels, root_ms=root_ms)
        if key in assets and any(assets[key][field] != record[field] for field in ('clusters', 'pages', 'root_pages', 'slots', 'pixels')):
            raise RuntimeError('Inconsistent allocations for the same geometry identity')
        assets[key] = record
    if not assets:
        raise RuntimeError('Missing required virtual geometry initialization evidence')
    return assets


def inspect_virtual_geometry(log, requested=None, verify_asset=False):
    requested = requested or dict(enabled=True, pool_mib=384, pixels=1, mode='required')
    if not requested.get('enabled'):
        raise RuntimeError('Map geometry must use the required virtual geometry path')
    if 'world_geometry_failed ' in log:
        raise RuntimeError('Virtual geometry runtime reported failure')
    if re.search(r'map_draw forward=0 ', log):
        raise RuntimeError('Legacy opaque map rendering was submitted')
    assets = _initialized(log, requested)
    ready = re.findall(r'^world_geometry_ready mode=required asset=([a-f0-9]{64}) clusters=(\d+) pages=(\d+)$', log, re.MULTILINE)
    identities = set()
    for key, clusters, pages in ready:
        if key not in assets or (int(clusters), int(pages)) != (assets[key]['clusters'], assets[key]['pages']):
            raise RuntimeError('Geometry readiness does not match its initialized asset')
        identities.add(key)
    if identities != set(assets):
        raise RuntimeError('Some geometry assets never reached coarse root readiness')
    streams = re.findall(r'^world_geometry_stream asset=([a-f0-9]{64}) frame=(\d+) selected=(\d+) resident_pages=(\d+) '
                         r'pending_pages=(\d+) gpu_bytes=(\d+) uploaded_bytes=(\d+) feedback_overflow=(\d+)$', log, re.MULTILINE)
    records = {key: [] for key in assets}
    for key, *values in streams:
        if key not in assets:
            raise RuntimeError('Streaming evidence has an unknown geometry identity')
        row = tuple(map(int, values)); asset = assets[key]
        if (row[1] > asset['clusters'] or row[2] < asset['root_pages'] or row[2]+row[3] > asset['slots']
                or row[4] <= 0 or row[6] != 0):
            raise RuntimeError('Invalid virtual geometry streaming bounds or feedback overflow')
        records[key].append(row)
    if not any(row[1] > 0 for rows in records.values() for row in rows):
        raise RuntimeError('No actual virtual geometry opaque selection was recorded')
    for key, rows in records.items():
        assets[key].update(streaming_samples=len(rows), selected_max=max((row[1] for row in rows), default=0),
                           resident_pages_last=rows[-1][2] if rows else None,
                           uploaded_bytes_last=rows[-1][5] if rows else None)
    if verify_asset and requested.get('cache_path'):
        path = Path(requested['cache_path'])
        if not path.is_file() or path.stat().st_size != requested['cache_bytes'] or _digest(path) != requested['cache_sha256']:
            raise RuntimeError('Virtual geometry cache changed during the capture')
    if verify_asset and requested.get('scene_catalog'):
        scene = requested['scene_catalog'];path = Path(scene['path'])
        if not path.is_file() or path.stat().st_size != scene['bytes'] or _digest(path) != scene['sha256']:
            raise RuntimeError('Scene catalog changed during the capture')
    return dict(requested, observed=dict(assets=assets, asset_count=len(assets), streaming_samples=len(streams),
                                        selected_max=max(item['selected_max'] for item in assets.values())))


def inspect_opaque_submissions(log, virtual_geometry=None):
    inspect_virtual_geometry(log, virtual_geometry)


def inspect_geometry_rays(log):
    assets = set(re.findall(r'^world_geometry_ready mode=required asset=([a-f0-9]{64}) ', log, re.MULTILINE))
    rows = re.findall(r'^world_geometry_ray_ready asset=([a-f0-9]{64}) generation=(\d+) clusters=(\d+) batches=(\d+) '
                      r'bytes=(\d+) budget=(\d+) offscreen=complete materials=authored error_pixels=([\d.eE+-]+) requested_pixels=([\d.eE+-]+) vertex_stride=(\d+)$', log, re.MULTILINE)
    ready = set(); snapshots = {}
    for key, *values, pixels, requested_pixels, stride in rows:
        generation, clusters, batches, size, budget = map(int, values)
        pixels = float(pixels); requested_pixels = float(requested_pixels); stride = int(stride)
        if (key not in assets or min(generation, clusters, batches, size, budget) <= 0 or size > budget or batches > clusters
                or not math.isfinite(pixels) or pixels < 0 or not math.isfinite(requested_pixels) or requested_pixels < 0 or stride not in (12, 104)):
            raise RuntimeError('Invalid paged map ray snapshot or memory budget evidence')
        ready.add(key)
        snapshots[key] = dict(generation=generation, clusters=clusters, batches=batches, bytes=size,
                              budget=budget, error_pixels=pixels, requested_pixels=requested_pixels, vertex_stride=stride, offscreen='complete', materials='authored')
    if not assets or ready != assets:
        raise RuntimeError('Ray tracing enabled but some virtual geometry assets lack published map BLAS')
    return dict(assets=snapshots, asset_count=len(snapshots))
