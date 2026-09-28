"""Validate emitted tile metrics; absent historical counters remain unavailable."""
from collections import Counter, defaultdict
import math
import re


# These are the stable fields of each owner record. Later additions are optional
# only when absent from every record of that kind in the same process log.
REQUIRED = {
    'tile_upload': {'bytes', 'budget', 'cpu_ms'},
    'tile_resource_setup': {'id', 'cpu_ms'},
    'tile_ray_pump': {'id', 'cpu_ms', 'ready'},
    'tile_ray_schedule': {'frame', 'polls', 'wait_fences', 'wait_allocations', 'operations',
                         'submissions', 'inflight', 'capacity', 'published', 'cancelled',
                         'work_tile', 'work_step', 'work_ms', 'cpu_ms', 'soft_budget_ms',
                         'total_polls', 'total_wait_fences', 'total_wait_allocations',
                         'total_operations', 'total_submissions'},
    'map_resource_allocation': {'cpu_ms'},
    'map_ray_resources': {'cpu_ms', 'triangles'},
    'map_compact_resource': {'cpu_ms', 'bytes'},
    'tile_published': {'id'},
    'tile_stream': {'resident', 'wanted', 'preparing', 'uploading'},
    'tile_readiness_deadline': {'id', 'phase', 'deadline_ms', 'collision_ready'},
    'tile_prepare': {'id', 'asset_cpu_ms', 'collision_cpu_ms', 'triangles', 'cancelled'},
}
OPTIONAL = {
    'tile_upload': {'pending_bytes', 'maximum_call_ms', 'material_cpu_ms', 'material_records'},
    'tile_resource_setup': {'os_budget_available', 'reserved_bytes'},
    'map_resource_allocation': {'maximum_call_ms', 'resources', 'cancelled'},
    'tile_published': {'file', 'collision_ready', 'ray_ready', 'readiness_ms', 'deadline_missed'},
    'tile_stream': {'frame', 'resident_bytes', 'reserved_bytes', 'retired_bytes', 'texture_bytes',
                    'cancelled', 'evicted', 'generation', 'camera_x'},
    'tile_prepare': {'resident_texture_reuses', 'avoided_dds_bytes'},
}
TOKEN = re.compile('|'.join(map(re.escape, REQUIRED)))
FIELD = re.compile(r'(?<!\S)(\w+)=')
NUMBER = re.compile(r'[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?\Z')


class TileMetricRecords:
    def __init__(self):
        self.counts = Counter()
        self.fields = defaultdict(Counter)
        self.schemas = {}

    def parse(self, line, line_number):
        matches = list(TOKEN.finditer(line))
        if not matches:
            return None, {}
        marker = matches[0].group()
        prefix = f'Tile metric line {line_number} ({marker})'
        if len(matches) != 1 or matches[0].start() != 0 or not line.startswith(marker + ' '):
            raise ValueError(f'{prefix}: embedded, damaged or interleaved marker')
        body = line[len(marker)+1:]
        keys = list(FIELD.finditer(body))
        if not keys or keys[0].start() != 0:
            raise ValueError(f'{prefix}: damaged field prefix')
        values = {}
        for index, match in enumerate(keys):
            name = match[1]
            value = body[match.end():keys[index+1].start() if index+1 < len(keys) else len(body)].strip()
            if name in values:
                raise ValueError(f'{prefix}: duplicate {name}')
            if marker == 'tile_published' and name == 'file':
                if not value or '\ufffd' in value or 'rhi_validation' in value:
                    raise ValueError(f'{prefix}: damaged file field')
                values[name] = value
                continue
            if not NUMBER.fullmatch(value):
                raise ValueError(f'{prefix}: invalid numeric {name}={value}')
            number = float(value)
            if not math.isfinite(number) or (number < 0 and name != 'camera_x'):
                raise ValueError(f'{prefix}: nonfinite or negative {name}')
            if not name.endswith('_ms') and name != 'camera_x' and not number.is_integer():
                raise ValueError(f'{prefix}: fractional counter {name}')
            values[name] = number
        missing = REQUIRED[marker] - values.keys()
        if missing:
            raise ValueError(f'{prefix}: missing required fields {sorted(missing)}')
        if marker == 'tile_ray_schedule':
            if (values['capacity'] not in (1,4) or values['inflight'] > values['capacity'] or values['polls'] > values['capacity'] or
                    values['operations'] > 1 or values['submissions'] > values['operations'] or
                    values['wait_fences'] + values['wait_allocations'] > values['polls']):
                raise ValueError(f'{prefix}: lifecycle scheduling limit exceeded')
            if values['operations']:
                expected_submission = int(values['work_step'] in (0, 4))
                if values['work_step'] not in (0, 2, 4) or values['submissions'] != expected_submission:
                    raise ValueError(f'{prefix}: incorrect submission classification')
            elif values['work_tile'] != 4294967295 or values['work_step'] != 6:
                raise ValueError(f'{prefix}: idle operation has a work identity')
        schema = set(values)
        if marker in self.schemas and schema != self.schemas[marker]:
            raise ValueError(f'{prefix}: inconsistent fields within one process log')
        self.schemas[marker] = schema
        self.counts[marker] += 1
        self.fields[marker].update(values.keys())
        return marker, {key: value for key, value in values.items() if isinstance(value, (int, float))}

    def coverage(self):
        return dict(all_recognized_records_valid=True,
                    marker_records=dict(self.counts),
                    field_records={key: dict(value) for key, value in self.fields.items()},
                    unobserved_marker_types=sorted(REQUIRED.keys() - self.counts.keys()),
                    unavailable_optional_fields={key: sorted(fields - self.schemas.get(key, set()))
                        for key, fields in OPTIONAL.items() if fields - self.schemas.get(key, set())},
                    complete_event_coverage=False,
                    limitation='Unversioned logs have no expected record counts or sequence numbers. '
                        'Validation covers recognized emitted records, not wholly deleted records, '
                        'unobserved work or zero-byte pumps omitted by older binaries. '
                        'Absent optional fields/counters are unavailable, never measured zero.')
