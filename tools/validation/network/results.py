"""Read actual probe counters; a process exit alone is insufficient qualification."""
import re
import math


def fields(line):
    result = {}
    for name, value in re.findall(r'(\w+)=([^\s]+)', line):
        try:
            result[name] = int(value)
        except ValueError:
            try:
                number = float(value)
                result[name] = number if math.isfinite(number) else value
            except ValueError:
                result[name] = value
    return result


def probe_result(probe, text, exit_code, max_takeoff_ms=None):
    lines = text.splitlines()
    if probe == 'jump':
        cases = [fields(line) for line in lines if re.match(r'^jump_\d+ ', line)]
        summary = next((fields(line) for line in reversed(lines) if line.startswith('jump_cases=')), {})
        playback = next((fields(line) for line in reversed(lines) if line.startswith('jump_playback ')), {})
        takeoffs = [case['onset_to_takeoff'] * 1000 for case in cases
                    if isinstance(case.get('onset_to_takeoff'), (int, float))]
        passed = (exit_code == 0 and 'jump_session=passed' in lines and
                  summary.get('failures') == 0 and summary.get('jump_cases', 0) > 0 and
                  len(cases) == summary.get('jump_cases') and len(takeoffs) == len(cases))
        if max_takeoff_ms is not None:
            passed = passed and all(value <= max_takeoff_ms for value in takeoffs)
        return dict(passed=passed, counters=summary, cases=cases, playback=playback,
                    mean_takeoff_ms=sum(takeoffs) / len(takeoffs) if takeoffs else None,
                    maximum_takeoff_ms=max(takeoffs) if takeoffs else None)
    counters = next((fields(line) for line in reversed(lines)
                     if line.startswith('client_block_actions_checks ')), {})
    passed = (exit_code == 0 and any(line.startswith('client_block_actions PASS ') for line in lines)
              and counters.get('receipts', 0) > 0 and counters.get('movement_gated', 0) > 0
              and counters.get('feedback_failures') == 0
              and all(counters.get(key) == 1 for key in
                      ('exact_rollback', 'unchanged_revision', 'ack_retired', 'originals_restored')))
    return dict(passed=passed, counters=counters)


def proxy_passed(stats):
    return (not stats.get('error') and stats['unexpected_source'] == 0 and
            stats['remaining_packets'] == 0 and stats['remaining_bytes'] == 0 and
            stats['peak_packets'] <= stats['packet_limit'] and stats['peak_bytes'] <= stats['byte_limit'] and all(
        stats[direction]['bound_dropped'] == 0 and stats[direction]['send_errors'] == 0
        for direction in ('client_to_server', 'server_to_client')))
