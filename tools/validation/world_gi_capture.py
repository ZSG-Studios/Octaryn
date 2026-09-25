"""Validate production world-surface GI evidence, independent of display images."""


def unsigned(value, maximum):
    return isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= maximum


def assert_world_gi(counters, require_ready=True):
    if counters.get('gi_mode') != 'block-transport' or counters.get('gi_admission') != 'world_geometry':
        raise RuntimeError('Capture did not execute world-geometry block transport')
    if require_ready:
        fields = ('render_frame', 'block_transport_statistics_frame', 'block_transport_admission_sweeps',
                  'block_transport_measured_admission_sweeps', 'block_transport_contributor_start_frame')
        if any(not unsigned(counters.get(field), 2**64-1) for field in fields):
            raise RuntimeError('World transport fence or sweep metadata is not an unsigned64 integer')
        fields = ('block_transport_pinned_rows', 'block_transport_ready_rows', 'block_transport_selection_occupied')
        if any(not unsigned(counters.get(field), 65536) for field in fields):
            raise RuntimeError('World transport occupancy metadata is not an unsigned bounded integer')
    if require_ready and (counters.get('block_transport_active') is not True or
                          counters.get('block_transport_coverage_valid') is not True or
                          counters.get('ray_coverage_complete') is not True or
                          counters.get('ray_pending_columns') != 0 or
                          counters.get('block_transport_ready') is not True or
                          counters.get('block_transport_admission_sweeps', 0) < 1):
        raise RuntimeError('Captured world transport has incomplete geometry or warmup')
    values = counters.get('block_transport_counters')
    if not isinstance(values, list) or len(values) != 12 or any(
            not unsigned(value, 0xffffffff) for value in values):
        raise RuntimeError('Capture omitted the twelve actual transport GPU counters')
    if require_ready and (values[0] == 0 or values[6] == 0 or values[8] == 0):
        raise RuntimeError('Capture contains no admitted surfaces or actual transport rays')
    if require_ready:
        measured = counters.get('block_transport_measured_admission_sweeps', 0)
        pinned = counters.get('block_transport_pinned_rows', 0)
        if (counters.get('block_transport_admission_clean') is not True or
                measured < 1 or measured > counters.get('block_transport_admission_sweeps', 0) or
                not 0 < pinned <= values[8] <= 65536):
            raise RuntimeError('World surface admission lacks a complete fenced clean sweep')
        start = counters.get('block_transport_contributor_start_frame', 2**64-1)
        if (start == 2**64-1 or not 0 <= start <= counters.get('render_frame', -1) or
                counters.get('block_transport_statistics_frame') != counters.get('render_frame') or
                values[9] != values[8] or counters.get('block_transport_ready_rows') != values[8] or
                counters.get('block_transport_selection_occupied') != values[8]):
            raise RuntimeError('World transport contains uninitialized or stale occupied rows')
    allocated = counters.get('block_transport_gpu_bytes', 0)
    total = counters.get('block_transport_total_gpu_bytes')
    if not unsigned(allocated, 2**64-1) or not unsigned(total, 2**64-1) or allocated <= 0 or total < allocated:
        raise RuntimeError('Capture has inconsistent world transport resource accounting')
    return counters

def assert_world_gi_sequence(rows):
    """Cross-check sampled ready records; native clean-sweep evidence is still required."""
    previous = None
    for row in rows:
        assert_world_gi(row)
        epoch = row.get('block_transport_epoch')
        if not unsigned(epoch, 0xfffffffd) or epoch == 0:
            raise RuntimeError('World transport sequence omitted its geometry epoch')
        if previous is not None:
            if row['render_frame'] <= previous['render_frame']:
                raise RuntimeError('World transport capture frames are not strictly increasing')
            if epoch == previous['block_transport_epoch']:
                sweeps = row['block_transport_measured_admission_sweeps']
                earlier = previous['block_transport_measured_admission_sweeps']
                if sweeps < earlier:
                    raise RuntimeError('World transport fenced sweep count regressed without geometry reset')
                if row['block_transport_contributor_start_frame'] != previous['block_transport_contributor_start_frame']:
                    raise RuntimeError('World transport first contributor frame changed without geometry reset')
                if (row['block_transport_counters'][11] != previous['block_transport_counters'][11]
                        and sweeps < earlier + 2):
                    raise RuntimeError('Mandatory admission failure lacks a subsequent complete clean sweep')
        previous = row
    return rows
