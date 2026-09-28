"""Verify isolated engine-cache pairs without asserting OS/driver/filesystem coldness."""
import hashlib
import json
from pathlib import Path
import re

from loading_report import case_identity, parse_loading, stable_hash
from performance_summary import distribution


def cache_snapshot(directory):
    if not directory.is_dir() or directory.is_symlink() or directory.is_junction():
        raise ValueError('Isolated shader cache must be a real directory owned by this pair')
    files = []
    for path in sorted(directory.rglob('*')):
        if path.is_symlink() or path.is_junction():
            raise ValueError('Isolated shader cache contains a redirected path')
        if not path.is_file():
            continue
        with path.open('rb') as source:
            digest = hashlib.file_digest(source, 'sha256').hexdigest()
        files.append(dict(path=path.relative_to(directory).as_posix(), bytes=path.stat().st_size, sha256=digest))
    return dict(files=len(files), bytes=sum(row['bytes'] for row in files),
                contents_sha256=stable_hash(files), entries=files)


def cache_counters(text):
    if 'shader_cache explicit_directory=1' not in text:
        raise ValueError('Requested isolated engine shader cache was not activated')
    configurations = re.findall(r'^shader_cache sdk_keys=1 .*$', text, re.MULTILINE)
    if len(configurations) != 1 or not re.search(r'\bshaders=1 pipelines=1\b', configurations[0]):
        raise ValueError('Both engine shader and pipeline caches must be active')
    namespace = re.search(r'\bnamespace=(\S+)', configurations[0])
    if not namespace:
        raise ValueError('Missing engine cache namespace identity')
    result = {}
    for kind, values in re.findall(r'^shader_cache_summary kind=(\w+) (.*)$', text, re.MULTILINE):
        if kind in result:
            raise ValueError('Multiple shader-cache lifetimes in one startup measurement')
        fields = {key: int(value) for key,value in re.findall(r'(\w+)=(\d+)', values)}
        required = {'hits','misses','writes','rejected','bytes','entries','evictions','evicted_bytes',
                    'write_refused','io_errors','lock_unavailable','entries_fresh'}
        if not required.issubset(fields):
            raise ValueError('Incomplete shader cache counters')
        if any(fields[key] for key in ('rejected','write_refused','io_errors','lock_unavailable')) or fields['entries_fresh'] != 1:
            raise ValueError('Cache error, corruption, lock contention or incomplete accounting invalidates startup pairing')
        result[kind] = fields
    if set(result) != {'shaders','pipelines'}:
        raise ValueError('Missing shader/pipeline shutdown cache summaries')
    return dict(namespace=namespace[1], counters=result)


def load_case(entry):
    path = Path(entry['case'])
    result = json.loads((path / 'result.json').read_text())
    build = json.loads((path / 'client-build.json').read_text())
    if result.get('exit_code') != 0 or result.get('status') not in ('measured','captured'):
        raise ValueError('Startup case did not complete successfully')
    identity, gaps = case_identity(result, build)
    if gaps:
        raise ValueError('Missing run identities: ' + '; '.join(gaps))
    hashes = identity['recorded_build_hashes']
    if 'Octaryn.Client.exe' not in hashes or not any(key.endswith('.slang') for key in hashes) or not any(key.replace('\\','/').startswith('server/') for key in hashes):
        raise ValueError('Executable, shader and authority payload hashes are required')
    if not build['host'].get('graphics_adapters'):
        raise ValueError('Adapter/driver identity is required')
    identity['settings'].update({key: result.get(key) for key in ('rt_history_search','fixed_sampling',
        'frame_cpu_trace','reflection_dimensions','uncapped_fps','warmup_frames')})
    text = (path / 'client.log').read_text(errors='replace')
    loading = parse_loading(text)
    if not loading['authoritative_ready_observed']:
        raise ValueError('Authority readiness was not observed')
    return dict(entry=entry, identity=identity, cache=result.get('shader_cache', {}),
                loading=loading, **cache_counters(text))


def validate_pair(first, reused):
    if first['entry']['state'] != 'engine_cache_empty' or reused['entry']['state'] != 'engine_cache_reused':
        raise ValueError('Pair must run empty first, followed by reuse')
    if first['identity'] != reused['identity'] or first['namespace'] != reused['namespace']:
        raise ValueError('Build/shader/authority/asset/driver/settings/cache-namespace identities differ')
    before, populated = first['entry']['before'], first['entry']['after']
    if before['files'] or before['bytes'] or before['entries']:
        raise ValueError('First run did not start with an empty engine cache directory')
    if populated['files'] <= 0 or not any(row['path'].endswith('.bin') for row in populated['entries']):
        raise ValueError('First run did not populate persistent engine cache entries')
    if populated != reused['entry']['before']:
        raise ValueError('Cache contents changed between first-run completion and reuse')
    directory = str(Path(first['entry']['cache_directory']).resolve())
    if str(Path(reused['entry']['cache_directory']).resolve()) != directory:
        raise ValueError('Warm reuse must use the first run cache directory')
    for case in (first, reused):
        expected = case['entry']['before']
        if str(Path(case['cache'].get('directory', '')).resolve()) != directory or \
                case['cache'].get('initial_files') != expected['files'] or case['cache'].get('initial_bytes') != expected['bytes']:
            raise ValueError('Capture-side cache state differs from runner-side prelaunch proof')
    for kind in ('shaders','pipelines'):
        if first['counters'][kind]['misses'] <= 0 or first['counters'][kind]['writes'] <= 0:
            raise ValueError('Empty engine cache did not exercise misses and writes')
        if reused['counters'][kind]['hits'] <= 0:
            raise ValueError('Reused engine cache did not exercise hits')


def report(entries):
    if len(entries) != 6 or len({str(Path(entry['case']).resolve()).casefold() for entry in entries}) != 6:
        raise ValueError('Exactly six independent startup processes are required')
    cases = [load_case(entry) for entry in entries]
    pairs, directories = [], set()
    for repeat in range(3):
        pair = [case for case in cases if case['entry']['repeat'] == repeat]
        if len(pair) != 2:
            raise ValueError('Exactly three empty/reuse pairs with repetitions 0,1,2 are required')
        validate_pair(*pair)
        directory = str(Path(pair[0]['entry']['cache_directory']).resolve()).casefold()
        if directory in directories:
            raise ValueError('Each pair requires its own new engine cache directory')
        directories.add(directory)
        pairs.append(pair)
    if any(case['identity'] != cases[0]['identity'] or case['namespace'] != cases[0]['namespace'] for case in cases[1:]):
        raise ValueError('Identities differ across the three matched pairs')
    variants = {}
    for state in ('engine_cache_empty','engine_cache_reused'):
        members = [case for case in cases if case['entry']['state'] == state]
        fields = set.intersection(*(set(case['loading']['measured']) for case in members))
        variants[state] = dict(startup={field: distribution([case['loading']['measured'][field] for case in members]) for field in sorted(fields)},
            cache={kind:{field:distribution([case['counters'][kind][field] for case in members])
                         for field in members[0]['counters'][kind]} for kind in ('shaders','pipelines')})
    common = set(variants['engine_cache_empty']['startup']) & set(variants['engine_cache_reused']['startup'])
    reductions = {field: [pair[0]['loading']['measured'][field]-pair[1]['loading']['measured'][field]
                           for pair in pairs] for field in sorted(common)}
    return dict(schema_version=1, timing_qualification=False, identity_sha256=stable_hash(cases[0]['identity']),
        variants=variants, paired_empty_minus_reused=reductions, cases=cases,
        scope='Engine-owned persistent shader and pipeline cache only; first run starts empty, second reuses verified bytes.',
        unknown_cache_state='OS, driver and filesystem cache states are uncontrolled/unknown in every run.',
        limits='No engine FPS claim. Reused caches may still miss or evict. Three startup samples give descriptive p95/p99, not reliable tails. SDL uptime and session readiness have different origins; overlapping stages are not summed.')


def markdown(data):
    lines = ['# Isolated engine shader-cache startup comparison', '', data['scope'], '', data['unknown_cache_state'], '',
             '| Measured field | Empty median | Reused median | Paired empty minus reused |', '|---|---:|---:|---|']
    for field, deltas in data['paired_empty_minus_reused'].items():
        a = data['variants']['engine_cache_empty']['startup'][field]['median']
        b = data['variants']['engine_cache_reused']['startup'][field]['median']
        lines.append(f'| {field} | {a:.3f} | {b:.3f} | '+', '.join(f'{value:.3f}' for value in deltas)+' |')
    lines += ['', '| Cache | State | Median hits | Median misses | Median writes |', '|---|---|---:|---:|---:|']
    for state, value in data['variants'].items():
        for kind, fields in value['cache'].items():
            lines.append('| '+ ' | '.join([kind, state]+[f"{fields[field]['median']:.0f}" for field in ('hits','misses','writes')])+' |')
    return '\n'.join(lines + ['', data['limits'], ''])
