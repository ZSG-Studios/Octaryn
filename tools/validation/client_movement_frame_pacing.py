import re


NAMED_FLOAT_PATTERN = re.compile(r" (?P<name>[a-zA-Z0-9_]+)=(-?\d+(?:\.\d+)?)")


def parse_named_float(line, name):
    for match in NAMED_FLOAT_PATTERN.finditer(line):
        if match.group("name") == name:
            return float(match.group(2))
    return None


def validate_frame_pacing(log_file, lines, errors):
    frame_lines = [line for line in lines if line.startswith("live_frame_profile ")]
    frame_ms = [
        value
        for value in (parse_named_float(line, "frame_ms") for line in frame_lines)
        if value is not None
    ]
    avg_ms = [
        value
        for value in (parse_named_float(line, "avg_ms") for line in frame_lines)
        if value is not None
    ]
    actual_fps = [
        value
        for value in (parse_named_float(line, "actual_fps") for line in frame_lines)
        if value is not None
    ]
    if len(frame_ms) < 120:
        errors.append(f"{log_file}: expected sustained frame profiling during movement, actual samples={len(frame_ms)}")
        return
    if len(actual_fps) != len(frame_ms):
        errors.append(f"{log_file}: expected every frame profile to report measured actual_fps, actual {len(actual_fps)}/{len(frame_ms)}")

    mismatched_fps = []
    for line in frame_lines:
        measured_ms = parse_named_float(line, "frame_ms")
        measured_fps = parse_named_float(line, "actual_fps")
        if measured_ms is None or measured_fps is None:
            continue
        reciprocal_ms = 1000.0 / measured_fps if measured_fps > 0.0 else 0.0
        if abs(measured_ms - reciprocal_ms) > 0.05:
            mismatched_fps.append((measured_ms, measured_fps, reciprocal_ms))
            if len(mismatched_fps) >= 5:
                break
    if mismatched_fps:
        errors.append(f"{log_file}: actual_fps must be reciprocal of measured frame_ms, examples={mismatched_fps}")
    if max(frame_ms) >= 1000.0:
        errors.append(f"{log_file}: movement probe hit second-scale frame stall, worst_frame_ms={max(frame_ms):.3f}")
    if max(frame_ms) >= 50.0:
        errors.append(f"{log_file}: movement probe hit sub-20fps measured frame, worst_frame_ms={max(frame_ms):.3f}")
    if avg_ms and avg_ms[-1] >= 100.0:
        errors.append(f"{log_file}: movement probe average frame time is too slow, final_avg_ms={avg_ms[-1]:.3f}")
