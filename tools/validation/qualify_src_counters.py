#!/usr/bin/env python3
"""Qualify SRC pass activity and merge/resolve bounds from lighting.csv and stdout.

Readback-free: no renderer change is required. The runner parses the existing
src_* GPU timestamp columns of lighting.csv, the world_capture stdout lines and
the open_world_exit counters of one isolated production-client run selected with
OCTARYN_CLIENT_GI=src. Budget overruns are recorded (status=measured), never
asserted away, so the same runner is valid before and after the SRC Resolve
performance fix.
"""
import argparse
import json
import re
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path

from lighting_profile_summary import SRC_BUDGET_PASSES, SRC_PASSES, read_rows, steady, summarize, total_per_row
from validate_lighting_architecture import environment, fixture, stop_case
from validate_rhi_client_diagnostic import inspect_result

SRC_FAILURE_MARKERS = ('world_src_config_failed', 'world_src_create_failed', 'src_update_failed',
                       'src_pass_failed', 'stage=split_radiance_cascades')
CAPTURE_PATTERN = re.compile(r'world_capture frame=(\d+).*?path=(.+)')


def inspect_captures(captures, args):
    if len(captures) != args.captures:
        raise RuntimeError(f'Expected {args.captures} GPU captures, observed {len(captures)}')
    if {row['frame'] % 2 for row in captures} != {0, 1}:
        raise RuntimeError('GPU capture sequence did not exercise both frame-slot parities')
    previous = None
    for row in captures:
        if row['frame'] < args.min_frame:
            raise RuntimeError(f'Capture preceded MIN_FRAME: {row}')
        if previous is not None and row['frame'] - previous < args.stride:
            raise RuntimeError(f'Capture interval below stride {args.stride}: {row}')
        previous = row['frame']
        for path in (Path(row['path']), Path(row['path'] + '.lighting.json')):
            if not path.is_file():
                raise RuntimeError(f'Missing capture artifact: {path}')
    return dict(frames=[row['frame'] for row in captures],
                parities=sorted({row['frame'] % 2 for row in captures}),
                paths=[row['path'] for row in captures])


def inspect_profile(rows, args):
    if len(rows) < args.min_profile_rows:
        raise RuntimeError(f'Lighting profile lacks {args.min_profile_rows} completed frames')
    checks = {}
    for field in SRC_PASSES:
        stable = [row[field] for row in steady(rows)]
        if max(row[field] for row in rows) <= 0 or statistics.median(stable) <= 0:
            raise RuntimeError(f'SRC pass lacks finite nonzero GPU execution while terrain resident: {field}')
    for field in ('ddgi_trace_ms', 'ddgi_update_ms'):
        if any(row[field] != 0 for row in rows):
            raise RuntimeError(f'DDGI pass executed while SRC was selected: {field}')
    totals = total_per_row(steady(rows), SRC_PASSES)
    exceeded = {field: summarize(rows, (field,))[field]
                for field in SRC_BUDGET_PASSES
                if summarize(rows, (field,))[field]['steady_maximum'] > args.budget_ms}
    checks['src_pass_activity'] = 'nonzero seed/trace/deposit/merge/contact/evaluate GPU time (probe and ray work implied)'
    checks['ddgi_replaced'] = 'ddgi_trace_ms and ddgi_update_ms are zero on every frame'
    checks['frame_parity_captures'] = 'both frame-slot parities captured'
    return dict(checks=checks, rows=len(rows),
                timings=summarize(rows, SRC_PASSES + ('composition_ms', 'local_shade_ms')),
                src_total_steady=dict(median=statistics.median(totals), maximum=max(totals)),
                budget=dict(threshold_ms=args.budget_ms,
                            steady_values={field: summarize(rows, (field,))[field] for field in SRC_BUDGET_PASSES},
                            exceeded=sorted(exceeded)),
                exceeded_recorded=exceeded)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--frames', type=int, default=600)
    parser.add_argument('--captures', type=int, default=8)
    parser.add_argument('--stride', type=int, default=31)
    parser.add_argument('--min-frame', type=int, default=300)
    parser.add_argument('--min-profile-rows', type=int, default=120)
    parser.add_argument('--budget-ms', type=float, default=50.0,
                        help='Steady-state merge/resolve budget in ms; overruns are recorded, not hidden')
    parser.add_argument('--timeout', type=int, default=1500,
                        help='First launches after shader edits spend minutes compiling the SRC pipeline set')
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=540)
    parser.add_argument('--quality', choices=('low', 'medium', 'high', 'ultra'), default='high')
    parser.add_argument('--block-lights', action='store_true')
    parser.add_argument('--rhi-validation', action='store_true',
                        help='Keep the RHI debug layer on; the D3D12 debug layer adds second-scale per-frame SRC overhead')
    parser.add_argument('--strict', action='store_true', help='Fail instead of record on budget overruns')
    args = parser.parse_args()
    if args.frames < 600:
        parser.error('--frames must be at least 600 for the qualification lighting-edits mode')
    if args.stride < 1 or args.stride > 119 or args.stride % 2 == 0:
        parser.error('--stride must be odd and within 1..119 for parity coverage')
    if not 120 <= args.min_frame <= 10000:
        parser.error('--min-frame must be within the native range 120..10000')
    if args.frames < args.min_frame + (args.captures - 1) * args.stride + 30:
        parser.error('--frames must cover min-frame plus every capture interval plus margin')
    args.gi = 'src'
    args.no_rhi_validation = not args.rhi_validation
    args.vegetation_shadows, args.resize, args.debug = False, False, 0
    args.upscaler = 'native'
    bundle = args.client_bundle_root.resolve()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    case = Path(tempfile.mkdtemp(prefix='src-counters-', dir=args.evidence_root.resolve()))
    fixture(bundle, case, args)
    env = environment(case, args)
    env.update(OCTARYN_CLIENT_CAPTURE_COUNT=str(args.captures),
               OCTARYN_CLIENT_CAPTURE_STRIDE=str(args.stride),
               OCTARYN_CLIENT_CAPTURE_MIN_FRAME=str(args.min_frame),
               OCTARYN_CLIENT_CAPTURE_STABLE_FRAMES='0')
    command = [str(bundle / 'Octaryn.Client.exe'), '--frames', str(args.frames),
               '--validate-ui', '--benchmark-hidden', '--validate-lighting-edits']
    (case / 'client-command.json').write_text(json.dumps(dict(command=command, env={
        key: value for key, value in sorted(env.items()) if key.startswith('OCTARYN_')}), indent=2), encoding='utf-8')
    print(f'src_counters_started evidence={case}', flush=True)
    timed_out = False
    with (case / 'client.log').open('wb') as log:
        process = subprocess.Popen(command, cwd=case, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            code = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            stop_case(process, case)
            code = process.returncode
    text = (case / 'client.log').read_text(errors='replace')
    result = dict(status='running', gi='src', evidence=str(case), command=command,
                  rhi_validation=args.rhi_validation)
    try:
        if timed_out:
            raise RuntimeError(f'Run exceeded {args.timeout}s; first-launch SRC shader compilation or the open Resolve performance defect')
        for marker in SRC_FAILURE_MARKERS:
            if marker in text:
                raise RuntimeError(f'SRC failure marker in client output: {marker}')
        captures = [dict(frame=int(match[1]), path=match[2].strip()) for match in CAPTURE_PATTERN.finditer(text)]
        result['captures'] = inspect_captures(captures, args)
        counts = inspect_result(code, text, 'D3D12' if args.backend == 'dx12' else 'Vulkan',
                                minimum_frames=captures[-1]['frame'])
        profile = inspect_profile(read_rows(case / 'lighting.csv'), args)
        exceeded = profile.pop('exceeded_recorded')
        if exceeded and args.strict:
            raise RuntimeError(f'Steady merge/resolve above {args.budget_ms} ms budget: {sorted(exceeded)}')
        result.update(status='measured' if exceeded else 'passed',
                      frames=counts[0], columns=counts[1], quads=counts[2],
                      lighting_csv=str(case / 'lighting.csv'), capture=str(case / 'frame.bmp'),
                      visual_inspection='required', **profile)
        if exceeded:
            result['budget']['note'] = ('Steady merge/resolve above budget on the current build; recorded, not asserted. '
                                        'Re-run after the SRC Resolve performance fix lands.')
    except (OSError, ValueError, RuntimeError) as error:
        result.update(status='failed', error=str(error))
        # Failed runs still record whatever GPU timestamps completed, so a
        # TDR or timeout leaves measured Resolve evidence behind.
        try:
            rows = read_rows(case / 'lighting.csv')
            result['partial_profile'] = dict(rows=len(rows),
                                             timings=summarize(rows, SRC_PASSES + ('ddgi_trace_ms', 'ddgi_update_ms')))
        except (OSError, ValueError, RuntimeError):
            pass
        (case / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        print(f'src_counters=failed: {error}', file=sys.stderr)
        print('\n'.join(text.splitlines()[-35:]), file=sys.stderr)
        sys.exit(1)
    (case / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(f'src_counters={result["status"]} evidence={case}', flush=True)


if __name__ == '__main__':
    main()
