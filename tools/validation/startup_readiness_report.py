"""Separate an explicit app-start readiness guard from full-map request latency."""
import math
import re

MARKERS = ('world_initial_playable_candidate', 'world_initial_playable')
INTEGERS = {'schema', 'renderer_frame', 'session', 'authority_ack', 'requested_generation',
            'requested_set_hash', 'requested_tiles', 'resident_tiles', 'total_tiles',
            'visible_tiles', 'visible_missing', 'collision_ready', 'requested_ready',
            'ray_required', 'ray_ready', 'ray_guard_complete'}
FLOATS = {'elapsed_ms', 'actor_x', 'actor_y', 'actor_z', 'camera_x', 'camera_y', 'camera_z',
          'yaw', 'pitch', 'fov'}
STRINGS = {'clock_origin', 'scope'}
FLAGS = {'collision_ready', 'requested_ready', 'ray_required', 'ray_ready', 'ray_guard_complete'}
TOKEN = re.compile(r'world_initial_playable(?:_candidate)?')


def parse_event(line, line_number):
    markers = list(TOKEN.finditer(line))
    if not markers:
        return None
    marker = markers[0].group()
    prefix = f'Initial readiness line {line_number}'
    if len(markers) != 1 or not line.startswith(marker + ' '):
        raise ValueError(f'{prefix}: embedded or interleaved marker')
    values = {}
    for token in line[len(marker)+1:].split():
        if '=' not in token:
            raise ValueError(f'{prefix}: damaged field')
        key, value = token.split('=', 1)
        if key in values or key not in INTEGERS | FLOATS | STRINGS:
            raise ValueError(f'{prefix}: duplicate or unknown field {key}')
        if key in INTEGERS:
            if not re.fullmatch(r'\d+', value) or int(value) >= 1 << 64:
                raise ValueError(f'{prefix}: invalid integer {key}')
            value = int(value)
        elif key in FLOATS:
            try:
                value = float(value)
            except ValueError as error:
                raise ValueError(f'{prefix}: invalid number {key}') from error
            if not math.isfinite(value):
                raise ValueError(f'{prefix}: nonfinite {key}')
        values[key] = value
    if values.keys() != INTEGERS | FLOATS | STRINGS:
        raise ValueError(f'{prefix}: missing required fields')
    if values['schema'] != 1 or values['clock_origin'] != 'main_entry':
        raise ValueError(f'{prefix}: unsupported schema or clock origin')
    if values['elapsed_ms'] < 0 or values['fov'] <= 0 or any(values[key] not in (0, 1) for key in FLAGS):
        raise ValueError(f'{prefix}: invalid timing, camera or flag')
    if (values['visible_missing'] != 0 or values['collision_ready'] != 1 or values['requested_ready'] != 1 or
            values['requested_tiles'] > values['resident_tiles'] or
            values['resident_tiles'] > values['total_tiles'] or values['visible_tiles'] > values['total_tiles']):
        raise ValueError(f'{prefix}: incomplete or inconsistent readiness predicates')
    scope = 'all_manifest_rt_guard' if values['ray_required'] else 'visible_requested_region'
    if values['scope'] != scope:
        raise ValueError(f'{prefix}: scope does not match RT policy')
    if marker == 'world_initial_playable':
        if values['ray_ready'] != 1 or values['ray_guard_complete'] != 1:
            raise ValueError(f'{prefix}: final marker lacks RT readiness')
        if values['ray_required'] and values['resident_tiles'] != values['total_tiles']:
            raise ValueError(f'{prefix}: full-manifest RT guard incomplete')
    return dict(marker=marker, **values)


def readiness_activation(log, requested=None):
    expected = ('startup_readiness_profile enabled=1 clock_origin=main_entry '
                'cadence=every_successful_world_present cpu_scope=frame_total')
    lines = [line for line in log.splitlines() if 'startup_readiness_profile' in line]
    if any(line != expected for line in lines):
        raise ValueError('Damaged or unsupported startup readiness activation')
    if requested is True and not lines:
        raise ValueError('Requested startup readiness diagnostic did not activate')
    if requested is False and lines:
        raise ValueError('Startup readiness diagnostic activated despite being disabled')
    return dict(enabled=True if lines else False if requested is False else None,
                status='enabled' if lines else 'disabled' if requested is False else 'not_observed',
                requested=requested, timing_scope='Diagnostic full-manifest/visible scans count in frame CPU; '
                    'these runs are not interchangeable with disabled timing runs.',
                clock_scope='Native main entry excludes OS process creation and loader time.')


def initial_playable_report(log):
    events = [event for number, line in enumerate(log.splitlines(), 1)
              if (event := parse_event(line, number)) is not None]
    unique = {}
    for event in events:
        key = event['session'], event['marker']
        if key in unique:
            raise ValueError('Duplicate initial readiness event in one session')
        unique[key] = event
    sessions = sorted({event['session'] for event in events})
    for session in sessions:
        candidate = unique.get((session, 'world_initial_playable_candidate'))
        final = unique.get((session, 'world_initial_playable'))
        if candidate and final and (final['elapsed_ms'] < candidate['elapsed_ms'] or
                                    final['renderer_frame'] < candidate['renderer_frame']):
            raise ValueError('Final initial-readiness clock precedes candidate')
    initial = unique.get((sessions[0], 'world_initial_playable')) if sessions else None
    elapsed = initial['elapsed_ms'] if initial else None
    budgets = {str(ms): True if elapsed is not None and elapsed <= ms else None for ms in (5000, 8000)}
    disposition = ('passed_app_main_5s' if budgets['5000'] else 'passed_app_main_8s' if budgets['8000'] else
                   'conservative_readiness_gate_exceeded_initial_playable_unqualified' if initial else 'unqualified')
    activation = readiness_activation(log)
    if events and not activation['enabled']:
        raise ValueError('Readiness event lacks explicit diagnostic activation')
    return dict(instrumentation=activation, required_visible_collision_ready_observed=initial is not None,
                clock_origin='main_entry' if initial else None, app_main_to_conservative_gate_ms=elapsed,
                process_launch_to_ready_ms=None, startup_budget_disposition=disposition,
                app_main_budget_pass=budgets, first_observed_session=sessions[0] if sessions else None,
                initial_event=initial, events=events,
                note='Only the explicit post-present authority/collision/visible/requested/RT guard is used. '
                     'A timely conservative guard proves its app-main-start bound. A late superset does not '
                     'prove earliest required playability missed a budget; OS process-launch time remains unknown. '
                     'Candidate and full-map request durations never qualify this initial gate.')
