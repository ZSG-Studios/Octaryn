"""Fixed GPU fixture partitions and per-process evidence checks."""
import csv
import math
import re


# name: (prepared compute entries, exact RT setup, conditional masks, summary markers)
GROUPS = {
    'cache': (7, 0, 0, ('cache', 'select', 'eviction', 'admission_safety', 'contributor_budget', 'contributor_domain')),
    'admission': (3, 0, 1, ('admission', 'world_budget')),
    'room': (9, 1, 1, ('world_room',)),
    'raster': (1, 0, 0, ('raster', 'reconstruction', 'reconstruction_raster', 'leaves_raster')),
    'rays': (6, 1, 1, ('rt', 'leaves_rt')),
    'plants': (6, 1, 1, ('plant_keys', 'plant_masks', 'plants_rt', 'plants_raster', 'plant_shadows', 'plant_energy')),
    'numerical': (2, 0, 0, ('reflection', 'history', 'probe')),
    'sampling': (1, 0, 1, ('sampling',)),
    'convergence': (8, 1, 1, ('reference', 'convergence')),
    'dynamic': (3, 1, 1, ('dynamic', 'geometry', 'dynamic_owner', 'item_lighting')),
    'leaf': (6, 1, 1, ('leaf_origins', 'leaf_energy')),
    'actor': (8, 1, 1, ('actor_history', 'actor_bounds')),
    'local-area': (5, 1, 1, ('local_area',)),
    'boundary': (1, 1, 1, ('boundary',)),
}


def fields(line):
    return dict(re.findall(r'(\w+)=(\S+)', line))


def inspect_errors(text):
    failures = re.findall(r'^.*(?:rhi_validation severity=error|Validation Error|VUID-|'
                          r'D3D12 ERROR|D3D12 CORRUPTION|validation_errors=[1-9]\d*).*$',
                          text, re.MULTILINE | re.IGNORECASE)
    if failures:
        raise RuntimeError('\n'.join(failures[:8]))


def inspect_group(text, name, combined=False):
    pipelines, rays, masks, proofs = GROUPS[name]
    inspect_errors(text)
    observed = {}
    for marker, identity in (('setup', 'group'), ('pipeline_reuse', 'group'), ('group', 'name')):
        records = [fields(line) for line in re.findall(
            rf'^block_transport_{marker}=passed (.*)$', text, re.MULTILINE)]
        if combined:
            records = [record for record in records if record.get(identity) == name]
        if len(records) != 1 or records[0].get(identity) != name:
            raise RuntimeError(f'Missing or duplicate {name} {marker} proof')
        observed[marker] = records[0]
    expected = dict(pipelines=str(pipelines), ray_bounds=str(rays), plant_masks=str(masks), before_frames='1')
    if any(observed['setup'].get(key) != value for key, value in expected.items()):
        raise RuntimeError(f'Incorrect pre-frame pipeline inventory for {name}')
    reuse = observed['pipeline_reuse']
    if (reuse.get('pipelines') != str(pipelines) or reuse.get('runtime_compiles') != '0'
            or not reuse.get('uses', '').isdigit() or int(reuse['uses']) < pipelines):
        raise RuntimeError(f'Incomplete retained pipeline reuse for {name}')
    end = observed['group']
    if end.get('validation_errors') != '0' or not end.get('frames', '').isdigit() or int(end['frames']) <= 0:
        raise RuntimeError(f'Invalid completed-work group interval for {name}')
    for proof in proofs:
        if len(re.findall(rf'^block_transport_{proof}=passed(?: |$)', text, re.MULTILINE)) != 1:
            raise RuntimeError(f'Missing or duplicate {name} fixture proof: {proof}')
    return int(end['frames'])


def inspect_group_inventory(text):
    for marker, identity in (('setup', 'group'), ('pipeline_reuse', 'group'), ('group', 'name')):
        actual = [fields(line).get(identity) for line in re.findall(
            rf'^block_transport_{marker}=passed (.*)$', text, re.MULTILINE)]
        if actual != list(GROUPS):
            raise RuntimeError(f'GPU fixture groups are missing, duplicated, or reordered: {marker}')
    for name in GROUPS:
        inspect_group(text, name, combined=True)


def inspect_pacing(path, log, name=None):
    if name is None:
        ends = re.findall(r'^block_transport_group=passed (.*)$', log, re.MULTILINE)
        if len(ends) != 1:
            raise RuntimeError('Pacing requires one process group log')
        name = fields(ends[0]).get('name')
    if name not in GROUPS:
        raise RuntimeError('Unknown pacing group')
    count = inspect_group(log, name)
    with path.open(encoding='utf-8', newline='') as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != ['frame', 'total_ms']:
            raise RuntimeError('Invalid block-transport GPU pacing header')
        rows = list(reader)
    if len(rows) != count:
        raise RuntimeError(f'Incomplete {name} completed-work pacing records')
    for frame, row in enumerate(rows):
        milliseconds = float(row['total_ms'])
        if int(row['frame']) != frame or not math.isfinite(milliseconds):
            raise RuntimeError('Invalid block-transport GPU pacing record')
        if milliseconds < 1000 / 30:
            raise RuntimeError(f'GPU fixture exceeded 30 FPS cap: group={name} frame={frame} ms={milliseconds}')
    oracles = re.findall(r'^block_transport_probe=passed (.*)$', log, re.MULTILINE)
    if name == 'numerical':
        if len(oracles) != 1:
            raise RuntimeError('Missing final oracle frame interval')
        oracle = fields(oracles[0])
        if not oracle.get('first_frame', '').isdigit() or oracle.get('oracle_frames') != '56':
            raise RuntimeError('Incomplete block-transport GPU oracle interval')
        first = int(oracle['first_frame'])
        if first <= 0 or first + 56 != count:
            raise RuntimeError('Oracle interval does not match its group pacing records')
    elif oracles:
        raise RuntimeError('Oracle ran in the wrong fixture group')
    return count
