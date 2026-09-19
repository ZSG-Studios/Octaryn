"""Summarize lighting.csv GPU pass timestamps without any new GPU readback."""
import csv
import statistics

DDGI_PASSES = ('ddgi_trace_ms', 'ddgi_update_ms')


def read_rows(path):
    with open(path, newline='') as source:
        rows = list(csv.DictReader(source))
    if not rows:
        raise RuntimeError(f'Empty lighting profile: {path}')
    for row in rows:
        row['frame'] = int(row['frame'])
        for field, value in row.items():
            if field != 'frame':
                row[field] = float(value)
    return rows


def steady(rows):
    """Second half of the completed rows; matches the existing profile convention."""
    return rows[len(rows) // 2:]


def summarize(rows, fields):
    result = {}
    for field in fields:
        if any(field not in row for row in rows):
            raise RuntimeError(f'Missing lighting GPU timestamp column: {field}')
        stable = [row[field] for row in steady(rows)]
        result[field] = dict(steady_median=statistics.median(stable), steady_maximum=max(stable),
                             all_median=statistics.median(row[field] for row in rows),
                             all_maximum=max(row[field] for row in rows))
    return result


def total_per_row(rows, fields):
    return [sum(row[field] for field in fields) for row in rows]
