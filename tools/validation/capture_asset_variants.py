"""Run explicit lossless/BC7 asset pairs; never cook or rebuild during measurement."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from asset_variant_report import read_json, timing_report, quality_report
from asset_evidence import cooked_asset_identity
from map_quality_views import VIEWS, views_for_map

ROOT = Path(__file__).resolve().parents[2]


def variant_inputs(path):
    variants = read_json(path)['variants']
    if set(variants) != {'lossless', 'bc7'} or variants['lossless']['codec'] != 'rgba8-lossless' or \
            variants['bc7']['codec'] != 'bc7-opaque-color-uber4':
        raise ValueError('Require the safe opaque-color BC7 candidate and lossless reference')
    if variants['lossless']['source_sha256'] != variants['bc7']['source_sha256']:
        raise ValueError('Prepared source geometry differs')
    manifests = []
    for variant in variants.values():
        manifest_path = Path(variant['manifest'])
        manifest = read_json(manifest_path)
        source = manifest_path.parent / manifest['map']
        cooked = cooked_asset_identity(source, manifest_path, manifest)
        cache = Path(cooked['texture_cache'])
        keys = read_json(cache / 'map-texture-cook.json')['files']
        legacy = hashlib.sha256()
        for key in keys:
            legacy.update((key + (cache / (key + '.dds.sha256')).read_text()).encode('ascii'))
        if legacy.hexdigest() != variant['texture_payload_identity'] or \
                cooked['texture_receipts']['count'] != variant['variants'] or \
                cooked.get('texture_manifest_sha256') != variant['texture_manifest_sha256']:
            raise ValueError('Prepared texture receipts/metadata have changed; recreate the variant descriptor')
        variant['captured_texture_receipts'] = cooked['texture_receipts']
        manifests.append((manifest, cooked['lod_receipts']))
    if manifests[0] != manifests[1]:
        raise ValueError('Manifest or cooked geometry differs between texture variants')
    return variants


def command(args, manifest, output, origin, quality):
    width, height = (2560, 1440) if args.size == '1440p' else (3840, 2160)
    result = [sys.executable, str(Path(__file__).with_name('capture_map_world.py')),
        '--client-bundle-root', str(args.client_bundle_root.resolve()), '--manifest', manifest,
        '--evidence-root', str(output), '--backend', args.backend,
        '--width', str(width), '--height', str(height), '--upscaler-mode', str(args.mode),
        '--frames', '400', '--warmup-frames', '120', '--ray-tracing', 'on',
        '--reflection-quality', 'ultra', '--shadow-quality', 'ultra', '--rt-reference',
        '--draw-mode', 'direct', '--lod-pixels', '0', '--camera-origin', *map(str, origin),
        '--timeout', str(args.timeout), '--max-frame-ms', str(args.max_frame_ms)]
    if quality:
        result += ['--camera-motion', '--fixed-sampling', '--captures', '8', '--capture-min-frame', '180', '--stride', '24']
    else:
        result += ['--uncapped-fps', '--captures', '1', '--capture-min-frame', '300']
        if args.workload == 'motion': result.append('--camera-motion')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--variants', type=Path, default=ROOT / 'build/release-windows/client/map-variants-opaque/variants.json')
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--size', choices=('1440p', '4k'), default='1440p')
    parser.add_argument('--mode', type=int, choices=(0, 1, 2), default=0)
    parser.add_argument('--workload', choices=('static', 'motion'), default='motion')
    parser.add_argument('--phase', choices=('timing', 'quality', 'both'), default='timing')
    parser.add_argument('--views', nargs='+', choices=VIEWS, default=list(VIEWS))
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--max-frame-ms', type=float, default=250)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    if args.timeout <= 0 or not 0 < args.max_frame_ms < float('inf'):
        parser.error('Positive, finite watchdog limits required')
    variants = variant_inputs(args.variants)
    manifest = Path(variants['lossless']['manifest'])
    views = views_for_map(manifest.parent / read_json(manifest)['map'], args.views)
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='asset-pair-', dir=args.evidence_root.resolve()))
    plan = dict(variants=variants, backend=args.backend, size=args.size, upscaler_mode=args.mode,
        geometry_path='full-detail direct raster and RT reference', views=views, commands=[],
        allowed_difference='Explicit cooked texture payload/metadata identities only; capture verifies geometry/build/settings.',
        cache_state='Fresh process per case; OS and shader cache state unknown. Never labeled cold.')
    if args.phase != 'quality':
        for repeat in range(3):
            for codec in (('lossless', 'bc7') if repeat % 2 == 0 else ('bc7', 'lossless')):
                output = root / 'timing' / f'{repeat}-{codec}'
                entry = dict(phase='timing', repeat=repeat, variant=codec, size=args.size, mode=args.mode,
                             workload=args.workload, evidence=str(output))
                entry['command'] = command(args, variants[codec]['manifest'], output, [0, 2, -20, .6, -.25], False)
                plan['commands'].append(entry)
    if args.phase != 'timing':
        for view, info in views.items():
            for codec in ('lossless', 'bc7'):
                output = root / 'quality' / view / codec
                entry = dict(phase='quality', view=view, variant=codec, evidence=str(output))
                entry['command'] = command(args, variants[codec]['manifest'], output, info['origin'], True)
                plan['commands'].append(entry)
    (root / 'plan.json').write_text(json.dumps(plan, indent=2))
    print(f'asset_variant_plan={root / "plan.json"}', flush=True)
    if args.dry_run: return
    completed = []
    try:
        for entry in plan['commands']:
            subprocess.run(entry['command'], check=True, timeout=args.timeout+60)
            results = list(Path(entry['evidence']).glob('*/result.json'))
            if len(results) != 1: raise RuntimeError('Expected one isolated capture result')
            completed.append(dict(entry, case=str(results[0].parent)))
            (root / 'cases.json').write_text(json.dumps(completed, indent=2))
        timing = [entry for entry in completed if entry['phase'] == 'timing']
        quality = [entry for entry in completed if entry['phase'] == 'quality']
        if timing:
            (root / 'timing.json').write_text(json.dumps(timing_report(timing, variants), indent=2))
        if quality:
            (root / 'quality.json').write_text(json.dumps(quality_report(quality, variants, root / 'comparisons'), indent=2))
    finally:
        (root / 'cases.json').write_text(json.dumps(completed, indent=2))
    print(f'asset_variant_completed={root}', flush=True)


if __name__ == '__main__':
    main()
