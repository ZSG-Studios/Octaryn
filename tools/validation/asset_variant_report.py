"""Qualify explicit texture-codec differences without relaxing geometry or render identity."""
import json
import re
from pathlib import Path

from asset_evidence import compare_recorded_asset_identities
from capture_map_quality import signature, compare_signatures
from loading_report import parse_loading
from performance_summary import distribution
from summarize_performance_matrix import (read_case, require_same_camera, matched_frames,
                                          run_statistics, aggregate)


def read_json(path):
    return json.loads(path.read_text())


def verify_asset(result, expected):
    asset = result['map']
    cooked = asset.get('cooked_identity', {})
    receipts = cooked.get('texture_receipts', {})
    if asset['sha256'] != expected['source_sha256']:
        raise ValueError('Captured geometry hash differs from the prepared asset variant')
    if receipts != expected['captured_texture_receipts'] or receipts.get('count') != expected['variants']:
        raise ValueError('Captured DDS receipt identity differs from the explicit codec variant')
    if cooked.get('texture_manifest_sha256') != expected['texture_manifest_sha256']:
        raise ValueError('Captured cooker metadata differs from the explicit codec variant')
    if cooked.get('lod_receipts', {}).get('count') != 1:
        raise ValueError('Expected authenticated, unchanged geometry LOD receipt')


def validate_pair(cases, variants):
    identities = []
    for case in cases:
        result = case['result']
        verify_asset(result, variants[case['entry']['variant']])
        settings = {key: result.get(key) for key in (
            'backend', 'dimensions', 'render_dimensions', 'reflection_dimensions', 'upscaler_mode',
            'draw_mode', 'lod_pixels', 'ray_tracing', 'rt_reference', 'rt_sparse', 'rt_history_search',
            'reflection_quality', 'shadow_quality', 'temporal_reflections', 'reflection_distance',
            'camera_motion', 'camera_origin', 'fixed_lighting', 'process_priority', 'fixed_sampling',
            'uncapped_fps', 'warmup_frames', 'rhi_validation', 'ray_diagnostics', 'debug')}
        if not result['rt_reference'] or result['draw_mode'] != 'direct' or result['lod_pixels'] != 0:
            raise ValueError('Codec comparison must retain the full-detail RT/raster reference')
        build = read_json(Path(case['entry']['case']) / 'client-build.json')
        hashes = build['sha256']
        if 'Octaryn.Client.exe' not in hashes or not any(key.endswith('.slang') for key in hashes):
            raise ValueError('Executable or shader evidence is missing')
        if not build.get('host', {}).get('graphics_adapters'):
            raise ValueError('Windows GPU/driver identity is missing')
        asset = result['map']
        identities.append(dict(settings=settings, build=hashes, host=build['host'],
            geometry=asset['sha256'], manifest=asset['manifest'],
            lod=asset['cooked_identity']['lod_receipts']))
    if any(identity != identities[0] for identity in identities[1:]):
        raise ValueError('Geometry, LOD, manifest, executable, shader, driver or rendering settings differ')
    return identities[0]


def texture_upload(case, expected):
    text = (case / 'client.log').read_text(errors='replace')
    lines = re.findall(r'^map_textures variants=\d+ .*$', text, re.MULTILINE)
    if len(lines) != 1:
        raise ValueError('Expected one monolithic texture upload evidence record')
    fields = {key: int(value) for key, value in re.findall(r'(\w+)=(\d+)', lines[0])}
    if fields.get('uncooked_rgba') != 0 or fields.get('gpu_bytes') != expected['texture_payload_bytes']:
        raise ValueError('Unexpected source decodes or uploaded texture payload bytes')
    if (fields.get('cached_bc7', 0) > 0) != (expected['codec'] == 'bc7-opaque-color-uber4'):
        raise ValueError('Executed texture codec differs from the requested variant')
    return fields


def timing_report(entries, variants):
    if len({str(Path(entry['case']).resolve()).casefold() for entry in entries}) != len(entries):
        raise ValueError('Repeated case paths are not independent launches')
    cases = []
    for entry in entries:
        # Existing validator checks the RT reference contract; persisted labels remain codecs.
        case = read_case(dict(entry, variant='reference'))
        case['entry'] = entry
        cases.append(case)
    identity = validate_pair(cases, variants)
    if any(case['identity'] != cases[0]['identity'] for case in cases[1:]):
        raise ValueError('Observed FOV or fixed-workload identity differs')
    require_same_camera([case['camera'] for case in cases])
    sunlight = cases[0]['sunlight']
    if not sunlight or any(case['sunlight'] != sunlight for case in cases[1:]):
        raise ValueError('Actual captured camera-ready frames or sunlight differ')
    selected, excluded, warmup = matched_frames(cases)
    output = {}
    for codec in ('lossless', 'bc7'):
        members = sorted((case for case in cases if case['entry']['variant'] == codec),
                         key=lambda case: case['entry']['repeat'])
        if [case['entry']['repeat'] for case in members] != [0, 1, 2]:
            raise ValueError('Exactly three matched fresh-process repetitions are required per codec')
        coverage = compare_recorded_asset_identities([case['result']['map'] for case in members])
        runs = [run_statistics(case, selected) for case in members]
        loading = [parse_loading((Path(case['entry']['case']) / 'client.log').read_text(errors='replace'))
                   for case in members]
        fields = set.intersection(*(set(item['measured']) for item in loading))
        output[codec] = dict(rendering=aggregate(runs), runs=runs,
            startup={field: distribution([item['measured'][field] for item in loading]) for field in sorted(fields)},
            startup_runs=loading, texture_upload_runs=[texture_upload(Path(case['entry']['case']), variants[codec]) for case in members],
            expected_texture_payload_bytes=variants[codec]['texture_payload_bytes'],
            asset_identity_coverage=coverage, cases=[case['entry']['case'] for case in members])
    base = output['lossless']['rendering']['gpu']['total_gpu_ms']
    candidate = output['bc7']['rendering']['gpu']['total_gpu_ms']
    paired = [a-b for a,b in zip(base['run_means'], candidate['run_means'])]
    reduction = base['mean'] - candidate['mean']
    noise = max(base['run_mean_range'], candidate['run_mean_range'])
    comparison = dict(baseline='lossless', candidate='bc7', gpu_mean_reduction_ms=reduction,
        paired_reduction_ms=paired, observed_run_spread_ms=noise,
        exceeds_observed_noise=reduction > noise and min(paired) > 0,
        limitation='Three-run spread check, not statistical significance or visual acceptance.')
    return dict(identity=identity, variants=output, comparison=comparison, measured_ready_frames=selected,
        excluded_capture_ready_frames=excluded, warmup=warmup,
        allowed_difference='Only authenticated cooked texture payloads and their cooker metadata differ.',
        timing_qualification=True, cache_state='Fresh process per run; OS/shader cache state unknown.',
        memory_note='Rendering.memory reports sampled process RSS/private bytes and GPU/texture payload counters. Missing fields are unavailable, never zero.',
        startup_note='Three launches provide descriptive startup distributions, not reliable tail estimates.',
        acceptance='Performance and image acceptance pending; texture payload savings alone are not FPS improvement.')


def quality_report(entries, variants, destination):
    from compare_map_images import compare
    comparisons = {}
    for view in sorted({entry['view'] for entry in entries}):
        members = sorted((entry for entry in entries if entry['view'] == view),
                         key=lambda entry: entry['variant'] != 'lossless')
        if [entry['variant'] for entry in members] != ['lossless', 'bc7']:
            raise ValueError('Each quality view requires one explicit pair of texture codecs')
        cases = [dict(entry=entry, result=read_json(Path(entry['case']) / 'result.json')) for entry in members]
        identity = validate_pair(cases, variants)
        for case in cases:
            result = case['result']
            if result.get('status') != 'captured' or result.get('exit_code') != 0 or not result.get('fixed_sampling') or result.get('timing_qualification'):
                raise ValueError('Quality sequence must complete with fixed sampling and no timing qualification')
            texture_upload(Path(case['entry']['case']), variants[case['entry']['variant']])
        observed = [signature(Path(case['entry']['case']), case['result']) for case in cases]
        sampling = compare_signatures(*observed)
        if not all(sampling[key] is True for key in ('jitter_phase_equal', 'shadow_phase_equal', 'reflection_phase_equal')):
            raise ValueError('Unmatched actual sampling phases invalidate the codec image comparison')
        for a, b in zip(observed[0]['sampling'], observed[1]['sampling']):
            for key in ('fixed_sampling', 'presentation_delta_ms', 'temporal_active', 'temporal_reset'):
                if a[key] != b[key]:
                    raise ValueError('Quality sequence presentation/history state differs')
            if not a['fixed_sampling'] or abs(a['presentation_delta_ms']-1000/60) > .001:
                raise ValueError('Fixed presentation sampling was not observed')
        images = compare(*(Path(entry['case']) for entry in members), destination / view)
        comparisons[view] = dict(identity=identity, sampling=sampling, images=images)
    return dict(timing_qualification=False, views=comparisons,
        acceptance='Inspect native images and motion: foliage coverage, glass opacity, glossy detail, ghosting and noise. Pixel metrics alone do not approve BC7.')
