"""Compare process and GPU memory at the same settled route phase each cycle."""
import csv
import statistics

from performance_summary import read_profile


def cycle_memory(case):
    profile = read_profile(case / 'frame-timing.csv')
    with (case / 'tile-route.csv').open(newline='') as stream:
        route = list(csv.DictReader(stream))
    cycles = {}
    held, previous = 0, None
    for row in route:
        key = (row['phase'], int(row['cycle']))
        held = held + 1 if key == previous else 1
        previous = key
        # Use recorded phases; initial warmup length is configurable.
        if row['phase'] != 'origin' or held <= 120:
            continue
        frame = int(row['frame'])
        if frame in profile:
            cycles.setdefault(int(row['cycle']), []).append(profile[frame])
    fields = ('process_resident_bytes', 'allocated_gpu_estimate_bytes', 'gpu_local_usage_bytes')
    samples = [dict(cycle=cycle, frames=len(rows), **{
        field: statistics.median(row[field] for row in rows) for field in fields})
        for cycle, rows in sorted(cycles.items()) if len(rows) >= 60]
    # Three full cycles warm driver/managed caches; five later cycles are required.
    tail = samples[3:]
    result = dict(samples=samples, warmup_cycles=3, qualified=len(tail) >= 5,
                  scope='Per-cycle origin medians; RSS and process GPU usage are distinct from tile accounting',
                  metrics={})
    for field in fields:
        if not tail:
            continue
        values = [row[field] for row in tail]
        x = [row['cycle'] for row in tail]
        xm, ym = statistics.mean(x), statistics.mean(values)
        denominator = sum((value-xm)**2 for value in x)
        slope = sum((a-xm)*(b-ym) for a,b in zip(x,values))/denominator if denominator else 0
        allowance = 128*1024**2 if field == 'process_resident_bytes' else 64*1024**2
        slope_limit = 2*1024**2 if field == 'process_resident_bytes' else 1024**2
        result['metrics'][field] = dict(minimum=min(values), maximum=max(values),
            range=max(values)-min(values), bytes_per_cycle=slope,
            range_limit=allowance, slope_limit=slope_limit,
            passed=max(values)-min(values) <= allowance and slope <= slope_limit)
    result['passed'] = result['qualified'] and all(row['passed'] for row in result['metrics'].values())
    return result
