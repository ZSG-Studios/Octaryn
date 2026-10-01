"""Matched camera-only RT quality sequences; captures never qualify engine FPS."""
import argparse
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from map_quality_views import VIEWS, views_for_map


def signature(case, result):
    with (case / 'camera-motion.csv').open() as source:
        rows = {int(row['frame']): row for row in csv.DictReader(source)}
    frames, sampling = [], []
    for capture in result['captures']:
        lighting = capture['lighting']
        row = rows.get(lighting['render_frame'])
        if row is None:
            raise RuntimeError('Captured GPU frame lacks matching actual camera evidence')
        frames.append(dict(ready_frame=int(row['ready_frame']), phase=row['phase'],
                           pose=[float(row[key]) for key in ('eye_x', 'eye_y', 'eye_z', 'yaw', 'pitch')],
                           sky_direction=lighting['sky_light_direction'], sky_time=lighting['sky_time']))
        observation=capture.get('observation', {})
        sampling.append(dict(ready_frame=int(row['ready_frame']), render_frame=lighting['render_frame'],
                             jitter=observation.get('jitter'), temporal_active=observation.get('temporal_active'),
                             temporal_reset=observation.get('reset'),
                             presentation_delta_ms=observation.get('delta_ms'), fixed_sampling=observation.get('fixed_sampling',False),
                             shadow_sequence_frame=observation.get('sampling_frame',lighting['render_frame']) % 4096,
                             reflection_sequence_frame=observation.get('reflection_sampling_frame')))
        if result.get('fixed_sampling') and (observation.get('sampling_frame') != int(row['ready_frame']) or
            observation.get('reflection_sampling_frame') != int(row['ready_frame']) % 4096):
            raise RuntimeError('Fixed sampling phases differ from the captured ready frame')
    phases = {frame['phase'] for frame in frames}
    if not {'motion', 'settle', 'cut', 'post_cut'}.issubset(phases):
        raise RuntimeError(f'Capture schedule missed a motion/cut phase: {sorted(phases)}')
    first = frames[0]
    origin = result['camera_origin']
    if any(abs(a-b) > 1e-5 for a, b in zip(first['pose'], origin)):
        raise RuntimeError('First captured camera does not match requested origin')
    for frame in frames:
        if frame['sky_direction'] != first['sky_direction'] or frame['sky_time'] != first['sky_time']:
            raise RuntimeError('Quality sequence lighting changed')
    return dict(map_sha256=result['map']['sha256'], dimensions=result['dimensions'], frames=frames, sampling=sampling)


def compare_signatures(reference, observed):
    if any(observed[key] != reference[key] for key in ('map_sha256','dimensions','frames')):
        raise RuntimeError('Compared paths differ in actual camera, lighting, map, or dimensions')
    pairs=list(zip(reference['sampling'],observed['sampling']))
    known=all(a['jitter'] is not None and b['jitter'] is not None for a,b in pairs)
    unequal=[b['ready_frame'] for a,b in pairs if a['jitter'] != b['jitter']]
    reflection_known=all(a.get('reflection_sequence_frame') is not None and
                         b.get('reflection_sequence_frame') is not None for a,b in pairs)
    reflection_equal=all(a['reflection_sequence_frame']==b['reflection_sequence_frame'] for a,b in pairs) if reflection_known else None
    shadow_equal=all(a['shadow_sequence_frame']==b['shadow_sequence_frame'] for a,b in pairs)
    matched=known and not unequal and reflection_equal and shadow_equal
    return dict(jitter_phase_equal=not unequal if known else None, jitter_mismatch_ready_frames=unequal,
                shadow_phase_equal=shadow_equal, reflection_phase_equal=reflection_equal,
                pixel_difference_limit='Sampling phases match; visual acceptance remains manual.' if matched else
                    'Missing or unmatched jitter/RT phases confound pixel differences; inspect the recorded phases.',
                performance_impact='This quality pairing check does not invalidate aggregate performance runs.')


def review_page(destination, cases):
    payload = []
    for entry in cases:
        case = Path(entry['case'])
        result = json.loads((case / 'result.json').read_text())
        payload.append(dict(view=entry['view'], mode=entry['mode'], frames=entry['signature']['frames'],
                            sampling=entry['signature']['sampling'], comparison=entry['sampling_comparison'],
                            images=[(case / item['path']).as_uri() for item in result['captures']]))
    data = json.dumps(payload).replace('</', '<\\/')
    destination.write_text('''<!doctype html><meta charset="utf-8"><title>RT quality review</title>
<style>body{background:#17191d;color:#eee;font:16px system-ui;margin:20px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(380px,1fr));gap:10px}
img{width:100%;height:auto}input{width:60%}pre{white-space:pre-wrap}</style>
<h1>Matched RT quality sequences</h1><p>Check foliage edges, contact shadows, glass, reflection
detail, disocclusion, persistent noise and ghosting. Material names do not establish visible coverage.
Moving items require a separate populated authority fixture.</p>
<p>Jitter equality is reported separately from pose/light matching. Different startup frame counts can
shift jitter and RT sampling phases. Unmatched phases limit pixel-difference conclusions; they do not
invalidate aggregate performance measurements. Older captures lack reflection sampling phase metadata.</p>
<select id="view"></select><input id="frame" type="range" min="0" value="0">
<button id="play">Play / pause</button><div class="grid" id="images"></div>
<script>const cases=''' + data + ''';
const view=document.querySelector('#view'),frame=document.querySelector('#frame');
for(const name of new Set(cases.map(x=>x.view)))view.add(new Option(name,name));
function draw(){const selected=cases.filter(x=>x.view===view.value);frame.max=selected[0].images.length-1;
let i=Math.min(Number(frame.value),Number(frame.max));frame.value=i;
document.querySelector('#images').replaceChildren(...selected.map(c=>{const box=document.createElement('div');
const title=document.createElement('p');title.textContent=c.mode+' / ready '+c.frames[i].ready_frame+' / '+c.frames[i].phase+
' / jitter '+JSON.stringify(c.sampling[i].jitter)+' / sequence jitter match '+c.comparison.jitter_phase_equal;
title.textContent+=' / reflection phase match '+c.comparison.reflection_phase_equal;
const image=document.createElement('img');image.src=c.images[i];box.append(title,image);return box;}));}
view.onchange=draw;frame.oninput=draw;let playing=false;document.querySelector('#play').onclick=()=>playing=!playing;
setInterval(()=>{if(playing){frame.value=(Number(frame.value)+1)%(Number(frame.max)+1);draw();}},250);draw();
</script>''', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-bundle-root', type=Path, required=True)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--evidence-root', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), default='dx12')
    parser.add_argument('--upscaler-mode', type=int, choices=(0, 1, 2, 6), default=0)
    parser.add_argument('--render-scale', type=float, default=2/3)
    parser.add_argument('--size', choices=('1440p', '4k'), default='1440p')
    parser.add_argument('--views', nargs='+', choices=VIEWS, default=['foliage', 'shop_glass', 'signs_metal'])
    parser.add_argument('--include-sparse', action='store_true')
    parser.add_argument('--include-queued', action='store_true')
    parser.add_argument('--rt-history-search', action='store_true', help='enable strict 2x2 reflection history search on compared adaptive paths')
    parser.add_argument('--fixed-sampling', action='store_true', help='align quality-only jitter/ray phases to ready frames')
    parser.add_argument('--dense', action='store_true', help='64 images per view at the selected output resolution instead of eight')
    parser.add_argument('--dry-run', action='store_true', help='write commands and bounds evidence without starting any process')
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--max-frame-ms', type=float, default=250)
    args = parser.parse_args()
    if args.timeout <= 0 or args.max_frame_ms <= 0:
        parser.error('Timeout and frame watchdog must be positive')
    bundle = args.client_bundle_root.resolve()
    manifest = args.manifest.resolve() if args.manifest else bundle / 'Client/Assets/Maps/map.json'
    map_path = manifest.parent / json.loads(manifest.read_text())['map']
    views = views_for_map(map_path, args.views)
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='quality-', dir=args.evidence_root.resolve()))
    modes = ['reference', 'adaptive'] + (['sparse'] if args.include_sparse else []) + (['queued'] if args.include_queued else [])
    dimensions = (2560, 1440) if args.size == '1440p' else (3840, 2160)
    captures, stride = (64, 3) if args.dense else (8, 24)
    width, height = dimensions
    plan = dict(backend=args.backend, upscaler_mode=args.upscaler_mode, views=views, modes=modes, captures_per_view=captures,
                rt_history_search=args.rt_history_search,
                fixed_sampling=args.fixed_sampling,
                captures_per_path=captures*len(views), dimensions=[width,height],
                visual_acceptance='pending image and sequence inspection', timing_qualification=False,
                moving_items='not assessed by this camera-only fixture', commands=[])
    for view, candidate in views.items():
        for mode in modes:
            out = root / view / mode
            command = [sys.executable, str(Path(__file__).with_name('capture_map_world.py')),
                       '--client-bundle-root', str(bundle), '--manifest', str(manifest),
                       '--evidence-root', str(out), '--backend', args.backend, '--camera-motion',
                       '--camera-origin', *map(str,candidate['origin']), '--frames', '400',
                       '--capture-min-frame', '180', '--captures', str(captures), '--stride', str(stride),
                       '--width', str(width), '--height', str(height), '--upscaler-mode', str(args.upscaler_mode),
                       '--render-scale', str(args.render_scale),
                       '--reflection-quality', 'ultra', '--shadow-quality', 'ultra',
                       '--timeout', str(args.timeout), '--max-frame-ms', str(args.max_frame_ms)]
            if mode == 'reference': command.append('--rt-reference')
            if mode == 'sparse': command.append('--rt-sparse')
            if mode == 'queued': command.append('--rt-queued')
            if args.rt_history_search and mode != 'reference': command.append('--rt-history-search')
            if args.fixed_sampling: command.append('--fixed-sampling')
            plan['commands'].append(dict(view=view, mode=mode, evidence=str(out), command=command))
    (root / 'plan.json').write_text(json.dumps(plan, indent=2))
    print(f'quality_suite_plan={root / "plan.json"}', flush=True)
    if args.dry_run:
        return
    cases = []
    try:
        for entry in plan['commands']:
            subprocess.run(entry['command'], check=True, timeout=args.timeout+60)
            results = list(Path(entry['evidence']).glob('*/result.json'))
            if len(results) != 1: raise RuntimeError('Expected one isolated case')
            case = results[0].parent
            result = json.loads(results[0].read_text())
            if result['status'] != 'captured': raise RuntimeError('Capture did not complete')
            observed = signature(case,result)
            reference = next((c for c in cases if c['view']==entry['view']), None)
            comparison=compare_signatures(reference['signature'] if reference else observed,observed)
            cases.append(dict(view=entry['view'],mode=entry['mode'],case=str(case),signature=observed,
                              sampling_comparison=comparison))
            review_page(root / 'review.html',cases)
    finally:
        (root / 'cases.json').write_text(json.dumps(cases,indent=2))
    print(f'quality_suite_review={root / "review.html"}',flush=True)


if __name__ == '__main__':
    main()
