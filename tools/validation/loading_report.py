"""Report measured loading stages from completed matrix cases without assuming cache warmth."""
import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import re

from performance_summary import distribution
from asset_evidence import compare_recorded_asset_identities


def stable_hash(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def numbers(line):
    result = {key: float(value) for key, value in re.findall(r'\b(\w+)=(-?[\d.]+)(?=\s|$)', line)}
    if any(not math.isfinite(value) for value in result.values()):
        raise ValueError('Invalid loading measurement')
    return result


def parse_loading(text):
    cumulative = defaultdict(list)
    stages = defaultdict(list)
    for name, value in re.findall(r'^client_boot stage=(.*?) elapsed_ms=([\d.]+)$', text, re.MULTILINE):
        cumulative[name].append(float(value))
    for name, value in re.findall(r'^map_startup stage=(\w+) elapsed_ms=([\d.]+)$', text, re.MULTILINE):
        stages[name].append(float(value))
    measured = {}
    # Repeated map transitions cannot be silently treated as one launch stage.
    for name, values in stages.items():
        if len(values) == 1:
            measured['map_' + name + '_ms'] = values[0]
    if len(cumulative['renderer_ready']) == 1:
        measured['renderer_ready_elapsed_ms'] = cumulative['renderer_ready'][0]
    boot = {}
    for prefix in ('client_boot', 'map_boot'):
        matches = re.findall(r'^' + prefix + r' responsiveness=.*$', text, re.MULTILINE)
        boot[prefix] = [numbers(line) for line in matches]
        if len(matches) == 1:
            for key in ('worker_elapsed_ms', 'elapsed_ms', 'max_event_gap_ms', 'event_pumps'):
                if key in boot[prefix][0]:
                    measured[prefix + '_' + key] = boot[prefix][0][key]
    for marker, field in (('map_model_loaded', 'map_renderer_total_ms'),
                          ('client_collision_ready', 'collision_prepare_ms'),
                          ('map_boot_temporal', 'temporal_setup_ms'),
                          ('map_ray_startup_end', 'map_ray_startup_ms')):
        matches = re.findall(r'^' + marker + r' .*$', text, re.MULTILINE)
        if len(matches) == 1:
            values = numbers(matches[0])
            if 'ms' in values or 'elapsed_ms' in values:
                measured[field] = values.get('ms', values.get('elapsed_ms'))
    ready = re.findall(r'^authoritative_player_ready(?: .*)?$', text, re.MULTILINE)
    if len(ready) == 1:
        ready_values = numbers(ready[0])
        for key in ('elapsed_ms', 'sdl_uptime_ms', 'session_elapsed_ms'):
            if key in ready_values:
                measured['authoritative_ready_' + key] = ready_values[key]
    if any(not math.isfinite(value) or value < 0 for value in measured.values()):
        raise ValueError('Nonfinite or negative loading measurement')
    return dict(measured=measured, map_stage_records_ms=dict(stages),
                boot_milestones_elapsed_ms=dict(cumulative), responsiveness_records=boot,
                authoritative_ready_observed=bool(ready),
                authoritative_ready_latency_available=any('authoritative_ready_' + key in measured for key in ('elapsed_ms', 'session_elapsed_ms')),
                readiness_note='Authority/input readiness does not prove required visible tiles and collision are ready. '
                    'Untimed ready markers prove observation only. SDL uptime and session elapsed have separate origins; '
                    'neither is automatically process-launch latency.')


def case_identity(result, build):
    settings = {key: result.get(key) for key in ('backend', 'dimensions', 'render_dimensions', 'upscaler_mode',
        'draw_mode', 'lod_pixels', 'ray_tracing', 'rt_reference', 'rt_sparse', 'reflection_quality',
        'shadow_quality', 'temporal_reflections', 'reflection_distance', 'camera_motion', 'camera_origin',
        'fixed_lighting', 'process_priority', 'rhi_validation', 'ray_diagnostics', 'debug')}
    asset = result.get('map', {})
    cooked = asset.get('cooked_identity')
    if cooked:
        cooked = {key: value for key, value in cooked.items() if key != 'texture_cache'}
    identity = dict(settings=settings, host=build.get('host'),
                    recorded_build_hashes=build.get('sha256', {}),
                    assets={key: asset.get(key) for key in ('sha256', 'manifest', 'manifest_sha256', 'cooked_metadata')},
                    cooked_identity=cooked)
    gaps = []
    if not identity['host']:
        gaps.append('host/driver identity absent')
    if not identity['recorded_build_hashes']:
        gaps.append('build/shader hashes absent')
    if not asset.get('sha256'):
        gaps.append('map payload hash absent')
    if not cooked:
        gaps.append('aggregate cooked texture receipts absent in historical capture')
    return identity, gaps


def read_case(matrix, entry):
    case = Path(entry['case'])
    if not case.is_absolute() and not case.exists():
        case = matrix.parent / case
    case = case.resolve()
    result = json.loads((case / 'result.json').read_text())
    if result.get('exit_code') != 0 or result.get('status') not in ('captured', 'measured'):
        raise ValueError(f'Case is not completed successfully: {case}')
    build = json.loads((case / 'client-build.json').read_text())
    identity, gaps = case_identity(result, build)
    parsed = parse_loading((case / 'client.log').read_text(errors='replace'))
    core_identity = dict(identity, assets={key: result.get('map', {}).get(key) for key in ('sha256', 'manifest')})
    core_identity.pop('cooked_identity', None)
    return dict(case=str(case), repeat=entry['repeat'], process_lifecycle='fresh_process',
                sequence_label='first_observed_repetition' if entry['repeat'] == 0 else 'repeat_repetition',
                os_cache_state='unknown', identity_sha256=stable_hash(core_identity), identity=identity,
                recorded_identity_sha256=stable_hash(identity), recorded_assets=result.get('map', {}),
                identity_gaps=gaps, **parsed)


def report(matrices):
    groups = defaultdict(list)
    failures = []
    for matrix in matrices:
        matrix = matrix.resolve()
        for entry in json.loads(matrix.read_text()):
            try:
                case = read_case(matrix, entry)
                key = (str(matrix), entry['size'], entry['mode'], entry['workload'], entry['variant'], case['identity_sha256'])
                groups[key].append(case)
            except (OSError, ValueError, KeyError) as error:
                failures.append(dict(matrix=str(matrix), case=entry.get('case'), error=str(error)))
    output = []
    for key, cases in sorted(groups.items()):
        cases.sort(key=lambda case: case['repeat'])
        repeats = [case['repeat'] for case in cases]
        qualified = repeats == [0, 1, 2]
        try:
            coverage = compare_recorded_asset_identities([case['recorded_assets'] for case in cases])
        except ValueError as error:
            qualified = False
            coverage = dict(error=str(error))
        common = set.intersection(*(set(case['measured']) for case in cases))
        aggregates = {field: distribution([case['measured'][field] for case in cases]) for field in sorted(common)} if qualified else {}
        output.append(dict(matrix=key[0], size=key[1], mode=key[2], workload=key[3], variant=key[4],
                           identity_sha256=key[5], matched_three_repetitions=qualified, repeats=repeats,
                           asset_identity_coverage=coverage, aggregate=aggregates, cases=cases))
    return dict(schema_version=1, groups=output, rejected_cases=failures,
                cache_policy='Every matrix entry starts a fresh process; OS file/shader caches are unknown. First-observed does not mean cold.',
                aggregation_policy='Exactly three repeats (0,1,2) require identical settings/build/host/GLB/parsed manifest; additional recorded asset identities must agree and their coverage is reported.',
                timing_policy='Map stages are individual wall-clock durations; boot milestones are cumulative. Overlapping/nested durations are never summed.',
                identity_policy='Reports only identities captured at run time; missing historical fields are not reconstructed from current files.',
                percentile_note='With three launches, p95/p99 are descriptive interpolations, not reliable startup-tail estimates.')


def markdown(data):
    text = ['# Loading measurements', '', data['cache_policy'], '',
            '| Workload / variant | Repeats | Renderer ready ms | Map renderer ms | Images ms | Collision ms | Map boot ms | Max map event gap ms |',
            '|---|---:|---:|---:|---:|---:|---:|---:|']
    fields = ('renderer_ready_elapsed_ms', 'map_renderer_total_ms', 'map_images_ms', 'collision_prepare_ms',
              'map_boot_worker_elapsed_ms', 'map_boot_max_event_gap_ms')
    for group in data['groups']:
        values = [f"{group['aggregate'][key]['worst' if key.endswith('max_event_gap_ms') else 'median']:.2f}"
                  if key in group['aggregate'] else 'unavailable' for key in fields]
        label = f"{group['size']} mode {group['mode']} {group['workload']} / {group['variant']} ({group['identity_sha256'][:12]})"
        text.append('| ' + ' | '.join([label, str(len(group['cases']))] + values) + ' |')
    text += ['', 'Durations are medians of three matched launches; event gap is the worst observed across the three. Unavailable values are not zero.', '',
             data['timing_policy'], '', data['percentile_note'], '',
             'Authority-ready latency remains unavailable when its marker has no elapsed timestamp. This milestone does not qualify visible-world/collision startup. See JSON for identity gaps, exact run values and event-pump counts.', '']
    return '\n'.join(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('matrix', nargs='+', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    data = report(args.matrix)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, indent=2) + '\n')
    args.output.with_suffix('.md').write_text(markdown(data))
    print(f"loading_report groups={len(data['groups'])} matched_three={sum(group['matched_three_repetitions'] for group in data['groups'])} rejected={len(data['rejected_cases'])} output={args.output}")


if __name__ == '__main__':
    main()
