"""Reject tiled codec quality captures with mismatched readiness or sampling."""
import csv
import math
import re


def quality_signature(case, evidence, records, ready_frame, origin, tile_count, log):
    if 'map_validation_sampling fixed=1 index=ready_frame' not in log:
        raise ValueError('Fixed tiled quality sampling was not activated')
    capture = evidence['capture']
    lighting, observation = capture['lighting'], capture['observation']
    frame = lighting['render_frame']
    if observation.get('frame') != frame or frame not in records:
        raise ValueError('Quality capture observation/timing frame join is incomplete')
    captures = re.findall(r'^world_capture frame=(\d+) nonclear_pixels=\d+ '
                          r'eye=([^ ]+) yaw=([^ ]+) pitch=([^ ]+) fov=([^ ]+) path=.+$', log, re.MULTILINE)
    views = [entry for entry in captures if int(entry[0]) == frame]
    if len(views) != 1:
        raise ValueError('Quality capture has no unique actual camera/FOV record')
    actual_pose = [float(value) for value in views[0][1].split(',')] + [float(value) for value in views[0][2:4]]
    fov = float(views[0][4])
    if (len(actual_pose) != 5 or any(not math.isfinite(a) or abs(a-b) > 1e-5 for a, b in zip(actual_pose, origin)) or
            not math.isfinite(fov) or not 0 < fov < math.pi):
        raise ValueError('Quality capture actual camera/FOV is invalid or differs')
    if (capture['width'], capture['height']) != (2560, 1440):
        raise ValueError('Quality output must be native 2560x1440')
    if not records or any((row.get('render_width'), row.get('render_height')) != (2560, 1440) or
                          row.get('upscaler_mode') != 0 or row.get('dynamic_resolution') != 0 for row in records.values()):
        raise ValueError('Quality run changed internal dimensions or reconstruction')
    resident = capture['residency']
    if (resident.get('resident') != tile_count or resident.get('wanted') != tile_count or
            resident.get('preparing') != 0 or resident.get('uploading') != 0):
        raise ValueError('Quality capture lacks the complete required tiled world')
    with (case / 'camera-motion.csv').open(newline='') as source:
        rows = {int(row['frame']): row for row in csv.DictReader(source)}
    row = rows.get(lighting['render_frame'])
    if not row or int(row['ready_frame']) != ready_frame or row['phase'] != 'static':
        raise ValueError('Tiled quality capture missed the exact common ready frame')
    pose = [float(row[name]) for name in ('eye_x', 'eye_y', 'eye_z', 'yaw', 'pitch')]
    if any(not math.isfinite(a) or abs(a-b) > 1e-5 for a, b in zip(pose, origin)):
        raise ValueError('Tiled quality camera differs from the requested view')
    delta = observation.get('delta_ms', 0)
    if (not observation.get('fixed_sampling') or not math.isfinite(delta) or abs(delta-1000/60) > .001 or
            observation.get('sampling_frame') != ready_frame or
            observation.get('reflection_sampling_frame') != ready_frame % 4096):
        raise ValueError('Tiled quality ray/presentation sampling differs from the fixed phase')
    if (not lighting.get('ray_enabled') or lighting.get('sky_time', [None])[-1] != 0 or
            not lighting.get('sky_light_direction')):
        raise ValueError('Tiled quality RT or fixed-light evidence is absent')
    setting_names = ('shadow_distance', 'reflection_distance', 'lighting_debug_view', 'local_light_count')
    settings = {name: lighting.get(name) for name in setting_names}
    if any(not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0
           for value in settings.values()):
        raise ValueError('Quality capture lighting settings evidence is absent or invalid')
    if (lighting.get('gi_mode') != 'direct' or not lighting.get('ray_coverage_complete') or
            lighting.get('ray_pending_columns') != 0 or lighting.get('ray_active_jobs') != 0):
        raise ValueError('Quality capture lighting coverage is incomplete')
    return dict(ready_frame=ready_frame, pose=pose, sky_time=lighting['sky_time'],
                vertical_fov=fov, lighting_settings=settings, gi_mode=lighting['gi_mode'],
                sky_direction=lighting['sky_light_direction'], jitter=observation.get('jitter'),
                presentation_delta_ms=delta, fixed_sampling=True,
                shadow_phase=ready_frame % 4096, reflection_phase=observation['reflection_sampling_frame'],
                full_resident_tiles=tile_count, native_dimensions=[2560, 1440], timing_qualification=False)
