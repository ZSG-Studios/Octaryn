import re


COMMAND_PATTERN = re.compile(
    r"^live_client_command_enqueue kind=1 request=(?P<request>\d+) target=0 "
    r"edit=(?P<edit>break|place) "
    r"block=\((?P<x>-?\d+),(?P<y>-?\d+),(?P<z>-?\d+),(?P<block>\d+)\) "
    r"flags=3$"
)


def read_log_lines(log_file):
    return [
        line.strip()
        for line in log_file.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]


def parse_positive_count(lines, prefix):
    values = []
    for line in lines:
        if not line.startswith(prefix):
            continue
        try:
            values.append(int(line.removeprefix(prefix)))
        except ValueError:
            return []
    return values


def parse_named_float(line, name):
    match = re.search(rf"{name}=(-?\d+\.\d+)", line)
    if not match:
        return None
    return float(match.group(1))


def parse_named_int(line, name):
    match = re.search(rf"{name}=(-?\d+)", line)
    if not match:
        return None
    return int(match.group(1))


def close_to(value, expected, tolerance):
    return value is not None and abs(value - expected) <= tolerance


def parse_camera_tuple(line):
    match = re.search(
        r"camera=\((-?\d+\.\d+),(-?\d+\.\d+),(-?\d+\.\d+),(-?\d+\.\d+),(-?\d+\.\d+)\)",
        line,
    )
    if not match:
        return None
    return tuple(float(value) for value in match.groups())
