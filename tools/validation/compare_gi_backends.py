#!/usr/bin/env python3
"""Run the same outdoor daylight fixture under DDGI and SRC and compare matched regions.

Both backends render the validate_lighting_architecture block-lights fixture at a
fixed pose and resolution. Each frame.bmp is converted to a PNG inspection copy
and reduced to matched-region luminance statistics (open stone, shaded pillar,
sky) like logs/client/lighting-response-daylight-calibration/comparison.json.
Values are measured and reported; no luminance equality between backends is
asserted because SRC and DDGI are different algorithms.
"""
import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from capture_regions import region_luminance, save_png
from lighting_profile_summary import DDGI_PASSES, SRC_PASSES, read_rows, summarize
from validate_lighting_architecture import environment, fixture, stop_case
from validate_rhi_client_diagnostic import inspect_result

DEFAULT_BOXES = dict(open_stone=(180, 350, 780, 470), shaded_pillar=(502, 188, 522, 247),
                     sky=(680, 120, 820, 200))
EXIT_PATTERN = re.compile(r'open_world_exit code=(\d+) frames=(\d+) columns=(\d+) quads=(\d+) gpu_bytes=(\d+)')


def backend_arguments(args, gi):
    return argparse.Namespace(
        width=args.width, height=args.height, quality=args.quality, backend=args.backend,
        vegetation_shadows=False, block_lights=True, upscaler='native', debug=0,
        captures=1, gi=gi, resize=False,
        no_rhi_validation=(gi == 'src' and not args.rhi_validation))


def run_backend(bundle, evidence, args, gi, boxes):
    case = Path(tempfile.mkdtemp(prefix=f'gi-{gi}-', dir=evidence))
    settings = backend_arguments(args, gi)
    fixture(bundle, case, settings)
    (case / 'world/world_time.json').write_text(json.dumps(
        dict(version=1, day_index=0, seconds_of_day=args.start_hour * 3600)), encoding='utf-8')
    env = environment(case, settings)
    env['OCTARYN_SERVER_START_HOUR'] = str(args.start_hour)
    command = [str(bundle / 'Octaryn.Client.exe'), '--frames', str(args.frames),
               '--validate-ui', '--benchmark-hidden', '--validate-lighting-edits']
    (case / 'client-command.json').write_text(json.dumps(dict(command=command, env={
        key: value for key, value in sorted(env.items()) if key.startswith('OCTARYN_')}), indent=2),
        encoding='utf-8')
    print(f'gi_compare_started gi={gi} evidence={case}', flush=True)
    timed_out = False
    with (case / 'client.log').open('wb') as log:
        process = subprocess.Popen(command, cwd=case, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            stop_case(process, case)
    text = (case / 'client.log').read_text(errors='replace')
    result = dict(status='running', gi=gi, evidence=str(case), command=command,
                  rhi_validation=not settings.no_rhi_validation,
                  client_sha256=hashlib.sha256((bundle / 'Octaryn.Client.exe').read_bytes()).hexdigest())
    try:
        if timed_out:
            raise RuntimeError(f'{gi} run exceeded {args.timeout}s; first-launch SRC shader compilation or the open Resolve performance defect')
        counts = inspect_result(process.returncode, text,
                                'D3D12' if args.backend == 'dx12' else 'Vulkan',
                                minimum_frames=args.min_frames)
        capture = case / 'frame.bmp'
        if not capture.is_file():
            raise RuntimeError('Missing production GPU capture')
        png = save_png(capture, case / 'frame.png')
        rows = read_rows(case / 'lighting.csv')
        fields = (DDGI_PASSES if gi == 'ddgi' else SRC_PASSES) + ('composition_ms', 'local_shade_ms')
        result.update(status='captured', frames=counts[0], columns=counts[1], quads=counts[2],
                      capture_bmp=str(capture), capture_png=str(png),
                      luminance={name: region_luminance(capture, box) for name, box in boxes.items()},
                      timings=summarize(rows, fields))
        result['exit_line'] = EXIT_PATTERN.search(text).group(0)
    except (OSError, ValueError, RuntimeError) as error:
        result.update(status='failed', error=str(error))
        # Record completed GPU timestamps even on failure; a TDR mid-run still
        # carries measured Resolve evidence for the performance owner.
        try:
            fields = (DDGI_PASSES if gi == 'ddgi' else SRC_PASSES) + ('composition_ms',)
            result['partial_profile'] = summarize(read_rows(case / 'lighting.csv'), fields)
        except (OSError, ValueError, RuntimeError):
            pass
        print(f'gi_compare_failed gi={gi}: {error}', file=sys.stderr)
        print('\n'.join(text.splitlines()[-35:]), file=sys.stderr)
    (case / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


def compare(results, boxes):
    by_gi = {row['gi']: row for row in results if row['status'] == 'captured'}
    if 'ddgi' not in by_gi or 'src' not in by_gi:
        return dict(available=False, captured=sorted(by_gi))
    regions = {}
    for name in boxes:
        ddgi = by_gi['ddgi']['luminance'][name]['mean']
        src = by_gi['src']['luminance'][name]['mean']
        regions[name] = dict(ddgi=ddgi, src=src, absolute_delta=src - ddgi,
                             relative_delta=(src - ddgi) / ddgi if ddgi else None)
    return dict(available=True, regions=regions,
                scope=('Tone-mapped matched-region luminance from actual GPU captures of the same '
                       'daylight block-lights fixture at a fixed pose. Different GI algorithms are '
                       'expected to differ; this table feeds visual inspection, not an equality pass.'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--quality', choices=('low', 'medium', 'high', 'ultra'), default='high')
    parser.add_argument('--frames', type=int, default=600)
    parser.add_argument('--min-frames', type=int, default=300)
    parser.add_argument('--start-hour', type=int, default=12, help='Server world hour for the daylight fixture')
    parser.add_argument('--timeout', type=int, default=1500)
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=540)
    parser.add_argument('--boxes', type=Path, help='JSON object mapping region name to [x0, y0, x1, y1]')
    parser.add_argument('--rhi-validation', action='store_true',
                        help='Keep the RHI debug layer on for SRC; it adds second-scale per-frame overhead')
    args = parser.parse_args()
    if args.frames < 600:
        parser.error('--frames must be at least 600 for the qualification lighting-edits mode')
    boxes = {name: tuple(box) for name, box in DEFAULT_BOXES.items()}
    if args.boxes:
        boxes = {name: tuple(box) for name, box in json.loads(args.boxes.read_text(encoding='utf-8')).items()}
    bundle = args.client_bundle_root.resolve()
    evidence = args.evidence_root.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    results = [run_backend(bundle, evidence, args, gi, boxes) for gi in ('ddgi', 'src')]
    comparison = dict(scope='DDGI versus SRC on one outdoor daylight block-lights fixture; measured values only',
                      backend=args.backend, quality=args.quality, dimensions=[args.width, args.height],
                      start_hour=args.start_hour, boxes={name: list(box) for name, box in boxes.items()},
                      backends=results, comparison=compare(results, boxes), visual_inspection='required')
    (evidence / 'comparison.json').write_text(json.dumps(comparison, indent=2), encoding='utf-8')
    for row in comparison['comparison'].get('regions', {}).items():
        name, values = row
        print(f'gi_compare_region {name} ddgi={values["ddgi"]:.6f} src={values["src"]:.6f} '
              f'delta={values["absolute_delta"]:+.6f}', flush=True)
    failed = [row['gi'] for row in results if row['status'] != 'captured']
    print(f'gi_compare={"failed:" + ",".join(failed) if failed else "measured"} evidence={evidence}', flush=True)
    if failed:
        sys.exit(1)


if __name__ == '__main__':
    main()
