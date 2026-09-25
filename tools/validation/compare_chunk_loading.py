"""Compare completed chunk-loading evidence without launching the client."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re

from benchmark_chunk_loading import inspect_snapshot, read_rows, summarize
from benchmark_presentation import distribution, require

FRAME_FIELDS = ('total_ms', 'session_ms', 'stream_ms', 'render_ms', 'ui_ms', 'cap_sleep_ms')
GPU_FIELDS = ('total_gpu_ms', 'mesh_cpu_ms', 'prepare_cpu_ms', 'encode_cpu_ms',
              'submit_cpu_ms', 'wait_cpu_ms', 'mesh_decode_ms', 'mesh_allocation_ms',
              'mesh_upload_ms', 'mesh_encoding_ms', 'mesh_submission_ms',
              'mesh_readback_ms', 'mesh_fence_wait_ms', 'mesh_release_ms', 'mesh_publication_ms')
COUNTERS = ('mesh_jobs_started', 'mesh_count_submits', 'mesh_emit_submits',
            'halo_published', 'halo_discarded')


def column_delta(rows, index):
    return int(rows[index]['columns']) - (int(rows[index - 1]['columns']) if index else 0)


def phase_summary(rows, gpu, start, stop):
    selected = rows[start:stop]
    if not selected:
        return None
    seconds = sum(float(row['total_ms']) for row in selected) / 1000
    sleep = sum(float(row['cap_sleep_ms']) for row in selected) / 1000
    deltas = [column_delta(rows, index) for index in range(start, stop)]
    result = dict(frames=len(selected), world_seconds=seconds, cap_sleep_seconds=sleep,
                  nonsleep_wall_seconds=seconds - sleep, columns_added=sum(deltas),
                  columns_per_second=sum(deltas) / seconds if seconds else None,
                  column_delta_histogram=dict(Counter(deltas)),
                  frame_ms={field: distribution([float(row[field]) for row in selected])
                            for field in FRAME_FIELDS if field in selected[0]})
    if gpu is not None:
        selected_gpu = gpu[start:stop]
        result.update(gpu_profile_ms={field: distribution([float(row[field]) for row in selected_gpu])
                                      for field in GPU_FIELDS if field in selected_gpu[0]},
                      mesh_counts={field: sum(int(row[field]) for row in selected_gpu)
                                   for field in COUNTERS if field in selected_gpu[0]},
                      mesh_starts_histogram=dict(Counter(int(row['mesh_jobs_started']) for row in selected_gpu)))
    return result


def ray_summary(log):
    records = [dict((key, float(value) if '.' in value else int(value))
                    for key, value in re.findall(
                        r'(ready|pending|jobs|blas_builds|tlas_builds|discarded|blas_gpu_ms|tlas_gpu_ms)=(\S+)', line))
               for line in log.splitlines() if line.startswith('world_ray ready=')]
    budget = re.search(r'world_ray mode=inline_query[^\r\n]+', log)
    result = dict(configuration=budget[0] if budget else None, log_samples=len(records))
    if records:
        result.update(final=records[-1], maximum_sampled_pending=max(row['pending'] for row in records),
                      blas_build_increment_histogram=dict(Counter(
                          after['blas_builds'] - before['blas_builds']
                          for before, after in zip(records, records[1:]))))
    return result


def shutdown_summary(log):
    frames = [(float(ms), int(count)) for ms, count in re.findall(
        r'^client_shutdown_frame total_ms=(\S+) remaining=(\d+)\s*$', log, re.M)]
    stages = {stage: float(ms) for stage, ms in re.findall(
        r'^client_shutdown stage=(\S+) status=end elapsed_ms=(\S+)', log, re.M)}
    result = dict(stage_ms=stages, completed_frames=len(frames))
    if frames:
        total = sum(ms for ms, count in frames)
        result.update(frame_ms=distribution([ms for ms, count in frames]), total_frame_ms=total,
                      first_remaining=frames[0][1], last_remaining=frames[-1][1],
                      nonincreasing_remaining=all(after[1] <= before[1]
                                                  for before, after in zip(frames, frames[1:])),
                      frames_above_50_ms=sum(ms > 50 for ms, count in frames))
        if 'renderer' in stages:
            result['renderer_stage_residual_ms'] = stages['renderer'] - total
    return result


def analyze(case):
    result = json.loads((case / 'result.json').read_text(encoding='utf-8'))
    require(result.get('status') != 'running', f'Case is still running: {case}')
    measured = summarize(case, result['radius'])
    rows, gpu = (read_rows(case / name) for name in ('frame-timing.csv', 'gpu-profile.csv'))
    aligned = bool(rows and len(rows) == len(gpu) and all(
        first['columns'] == second['columns'] and int(first['frame']) == int(second['frame']) + 1
        for first, second in zip(rows, gpu)))
    expected = (2 * result['radius'] + 1) ** 2
    full = next((index for index, row in enumerate(rows) if int(row['columns']) == expected), None)
    settled = next((index for index, row in enumerate(rows) if int(row['columns']) == expected
                    and int(row['pending_meshes']) == 0 and int(row['ray_pending']) == 0
                    and int(row.get('gi_ready', -1)) == 1), None)
    phases = {}
    if full is None:
        phases['incomplete_population'] = phase_summary(rows, gpu if aligned else None, 0, len(rows))
    else:
        phases['population'] = phase_summary(rows, gpu if aligned else None, 0, full + 1)
        end = settled + 1 if settled is not None else len(rows)
        phases['drain' if settled is not None else 'incomplete_drain'] = phase_summary(
            rows, gpu if aligned else None, full + 1, end)
        phases['complete_loading' if settled is not None else 'incomplete_loading'] = phase_summary(
            rows, gpu if aligned else None, 0, end)
        if settled is not None:
            phases['steady'] = phase_summary(rows, gpu if aligned else None, settled + 1, len(rows))
    log = (case / 'client.log').read_text(encoding='utf-8', errors='replace')
    report = dict(evidence=str(case.resolve()), status=result['status'], exit_code=result.get('exit_code'),
                  error=result.get('error'), client_sha256=result['client_sha256'],
                  bundle_content=result['bundle_content'], fixture_sha256=result['initial_fixture']['sha256'],
                  renderer_ready_ms=measured.get('renderer_ready_ms'), milestones=measured['milestones'],
                  completion=measured.get('completion'), world_frame_ms=measured.get('frame_ms'),
                  failures=measured['failures'], aligned_gpu_rows=aligned, phases=phases,
                  lighting_readiness_recorded=bool(rows and 'gi_ready' in rows[0]),
                  ray=ray_summary(log), shutdown=shutdown_summary(log))
    if full is not None:
        report['first_full_backlog'] = {field: int(rows[full][field])
                                       for field in ('frame', 'pending_meshes', 'ray_pending')}
    if rows:
        report['last_camera'] = {field: rows[-1][field] for field in
                                 ('eye_x', 'eye_y', 'eye_z', 'yaw', 'pitch', 'fov') if field in rows[-1]}
    try:
        report['authoritative_snapshot'] = inspect_snapshot(case, result['radius'])
    except (OSError, RuntimeError) as error:
        report['authoritative_snapshot_error'] = str(error)
    return result, report


def compare(before, after):
    old, first = analyze(before)
    new, second = analyze(after)
    checks = {key: old.get(key) == new.get(key) for key in
              ('initial_fixture', 'settings', 'lighting', 'requested_backend', 'radius',
               'hidden', 'frame_cap_fps', 'watchdog_max_frame_ms', 'capture_requested')}
    checks['stationary_cases'] = all('--benchmark-streaming-speed' not in result['command'] for result in (old, new))
    checks['both_passed_exit_zero'] = all(result.get('status') == 'passed' and result.get('exit_code') == 0
                                         for result in (old, new))
    old_final, new_final = first.get('completion') or {}, second.get('completion') or {}
    checks['equal_final_geometry'] = bool(old_final and new_final and all(
        old_final.get(field) == new_final.get(field) for field in ('columns', 'quads')))
    checks['equal_authoritative_window'] = bool(first.get('authoritative_snapshot') and
                                                first.get('authoritative_snapshot') == second.get('authoritative_snapshot'))
    changes = {}
    for name, milestone in first['milestones'].items():
        if name in second['milestones']:
            start, end = milestone['seconds_bounds'][1], second['milestones'][name]['seconds_bounds'][1]
            changes[name] = dict(before_seconds_upper=start, after_seconds_upper=end,
                                 seconds_saved=start - end, ratio_before_over_after=start / end if end else None,
                                 percent_less_time=100 * (1 - end / start) if start else None)
    return dict(matched_completed_comparison=all(checks.values()), checks=checks,
                before=first, after=second, milestone_changes=changes,
                limits=[
                    'World-frame milestones exclude renderer initialization; process clean exit is separate.',
                    'Background CPU/GPU contention is not controlled by this comparison.',
                    'stream_ms includes cap-time private work in the new path; cap_sleep_ms records actual sleep only.',
                    'GPU mesh substage counters for cap-time work appear in the next GPU-profile row. Compare phase/whole-loading aggregates; a phase boundary may shift one frame of work.',
                    'Nested CPU stage timers cannot be added to encompassing stream/render timers.',
                    'Main-frame GPU query times exclude separately submitted mesh and BLAS work.',
                    'Renderer shutdown residual includes final destruction plus logging/stage overhead.',
                    'A mismatched or incomplete pair has reached milestones only, not an accepted performance comparison.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = compare(args.before, args.after)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(output=str(args.output.resolve()),
                         matched_completed_comparison=result['matched_completed_comparison'],
                         checks=result['checks'], milestone_changes=result['milestone_changes']), indent=2))


if __name__ == '__main__':
    main()
