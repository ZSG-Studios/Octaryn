"""Read fenced, exact-key, linear production GI snapshots without display filtering."""
import math
import statistics
import struct
from pathlib import Path

MAGIC = b'BTGI_CACHE_V1\0\0\0'


def load(path):
    data = Path(path).read_bytes()
    if len(data) < 48 or data[:16] != MAGIC:
        raise ValueError('Invalid block transport cache signature')
    version, capacity, geometry, radiance, low, high, surface_size, value_size = struct.unpack_from('<8I', data, 16)
    if version != 1 or not 0 < capacity <= 65536 or surface_size != 64 or value_size != 16:
        raise ValueError('Unsupported block transport cache layout')
    if len(data) != 48 + capacity * 112:
        raise ValueError('Truncated or oversized block transport cache')
    rows = {}
    for index in range(capacity):
        offset = 48 + index * 64
        key = struct.unpack_from('<3iI', data, offset)
        albedo = struct.unpack_from('<4f', data, offset + 16)
        state = struct.unpack_from('<4I', data, offset + 32)
        extra = struct.unpack_from('<4I', data, offset + 48)
        if state[0] != geometry:
            continue
        if key in rows:
            raise ValueError('Duplicate live exact GI key')
        values = {}
        for field, section in (('direct', 0), ('environment', 1), ('indirect', 2)):
            address = 48 + capacity * 64 + (section * capacity + index) * 16
            rgb = struct.unpack_from('<3f', data, address)
            epoch, = struct.unpack_from('<I', data, address + 12)
            if not all(math.isfinite(v) and v >= 0 for v in rgb):
                raise ValueError('Nonfinite or negative GI radiance')
            values[field] = list(rgb) if epoch == radiance else None
        rows[key] = dict(albedo=list(albedo), batch=state[1], frame=state[2], history=state[3],
                         generation=extra[0], **values)
    return dict(frame=low + (high << 32), geometry_epoch=geometry, radiance_epoch=radiance, rows=rows)


def summarize(path, capture):
    snapshot = load(path)
    if snapshot['frame'] != capture['render_frame'] or snapshot['geometry_epoch'] != capture['block_transport_epoch'] \
            or snapshot['radiance_epoch'] != capture['block_transport_radiance_epoch']:
        raise ValueError('GI dump and captured frame/epochs differ')
    rows = list(snapshot['rows'].values())
    if len(rows) != capture['block_transport_counters'][8]:
        raise ValueError('GI dump live rows differ from actual GPU occupancy')
    batches = [row['batch'] for row in rows]
    ready = sum(all(row[field] is not None for field in ('direct', 'environment', 'indirect')) for row in rows)
    if capture['block_transport_ready'] and ready != len(rows):
        raise ValueError('Ready GI capture contains stale radiance rows')
    return dict(frame=snapshot['frame'], rows=len(rows), current_radiance_rows=ready,
                batch_min=min(batches, default=0), batch_mean=statistics.mean(batches) if batches else 0,
                batch_max=max(batches, default=0),
                scope='Linear per-face irradiance divided by pi; no tone mapping, TAA or screen reconstruction.')


def compare(first, second):
    a, b = load(first), load(second)
    if a['geometry_epoch'] != b['geometry_epoch'] or a['radiance_epoch'] != b['radiance_epoch']:
        raise ValueError('GI epochs differ; steady-state comparison is invalid')
    common = a['rows'].keys() & b['rows'].keys()
    result = dict(first_frame=a['frame'], second_frame=b['frame'], common_rows=len(common),
                  missing_first=len(b['rows']) - len(common), missing_second=len(a['rows']) - len(common))
    for field in ('direct', 'environment', 'indirect'):
        pairs = [(a['rows'][key][field], b['rows'][key][field]) for key in common
                 if a['rows'][key][field] is not None and b['rows'][key][field] is not None]
        if not pairs:
            result[field] = dict(rows=0)
            continue
        errors = [sum((y[i] - x[i]) ** 2 for i in range(3)) / 3 for x, y in pairs]
        result[field] = dict(rows=len(pairs), rgb_rmse=math.sqrt(statistics.mean(errors)),
            mean_absolute_rgb=statistics.mean(sum(abs(y[i] - x[i]) for i in range(3)) / 3 for x, y in pairs),
            reference_mean_rgb=[statistics.mean(y[i] for _, y in pairs) for i in range(3)])
    result['scope'] = 'Temporal change between two production estimates; the later estimate is not radiometric ground truth.'
    return result
