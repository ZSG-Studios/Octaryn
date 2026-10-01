"""Validate bounded renderer retirement traces without inferring scheduler or GPU causes."""
import csv
import math
from pathlib import Path

UNKNOWN = 2**64-1
FIELDS = ('schema_version,sequence,record,renderer_frame,scope,stage,thread_id,start_ns,end_ns,wall_ms,'
          'thread_cpu_ns,slot,fence_value,source_frame,completed_before,completed_after,wait_called,'
          'success,result,interval_count').split(',')


def require(value, reason):
    if not value:
        raise ValueError('Frame retirement trace: ' + reason)


def read_trace(path: Path, log=None, gpu_csv=None, include_frames=False):
    if log is not None:
        require('profile_writer_failed' not in log, 'writer failure invalidates capture')
        require('frame_cpu_profile enabled=1 schema=1 ' in log, 'requested recorder activation missing')
    with path.open(newline='', encoding='utf-8') as stream:
        reader = csv.DictReader(stream)
        require(reader.fieldnames == FIELDS, 'unsupported or damaged header')
        raw = list(reader)
    require(raw, 'no completed records')
    completed, pending, stages = [], [], {}
    for number, raw_row in enumerate(raw):
        require(None not in raw_row and all(v is not None for v in raw_row.values()), 'damaged row')
        row = {k: v if k in ('record','scope','stage') else float(v) if k == 'wall_ms' else int(v)
               for k, v in raw_row.items()}
        require(row['schema_version'] == 1 and row['sequence'] == number, 'schema or sequence discontinuity')
        require(row['scope'] in ('world','menu') and row['stage'], 'invalid scope/stage')
        require(0 <= row['start_ns'] <= row['end_ns'] and row['thread_cpu_ns'] >= -1, 'invalid interval')
        wall = (row['end_ns']-row['start_ns'])/1e6
        require(math.isfinite(row['wall_ms']) and abs(row['wall_ms']-wall) <= max(1e-6,wall*1e-8), 'wall/clock mismatch')
        require(row['success'] in (0,1) and row['wait_called'] in (0,1), 'invalid status')
        if row['record'] != 'frame':
            require(row['record'] in ('interval','fence'), 'unknown record kind')
            pending.append(row)
            continue
        require(row['stage'] in ('complete','skipped','abandoned'), 'failed or unwound frame')
        require(row['success'] == (row['stage']=='complete'), 'completion status mismatch')
        intervals = [r for r in pending if r['record']=='interval']
        fences = [r for r in pending if r['record']=='fence']
        require(len(intervals)==row['interval_count'] and len(intervals)<=128 and len(fences)<=12,
                'missing or overflowing bounded frame records')
        for r in pending:
            require(all(r[k]==row[k] for k in ('renderer_frame','scope','thread_id')), 'frame ownership mismatch')
            require(row['start_ns']<=r['start_ns']<=r['end_ns']<=row['end_ns'], 'record outside enclosing frame')
        require(all(a['end_ns']<=b['start_ns'] for a,b in zip(intervals,intervals[1:])), 'primary intervals overlap')
        require(len(fences)%3==0, 'incomplete fence state machine')
        waits = []
        for index in range(0,len(fences),3):
            group=fences[index:index+3]
            require([r['stage'] for r in group]==['fence_precheck','fence_wait_call','fence_postcheck'], 'fence phase order')
            identity=('slot','fence_value','source_frame','completed_before','completed_after','wait_called','success','result')
            require(all(all(r[k]==group[0][k] for k in identity) for r in group), 'fence identity changed mid-wait')
            require(all(a['end_ns']==b['start_ns'] for a,b in zip(group,group[1:])), 'fence clock gap')
            first=group[0]
            require(first['slot'] in (0,1) and first['success']==1 and first['result']>=0, 'failed fence retirement')
            if first['fence_value']:
                require(first['completed_before']!=UNKNOWN, 'missing precheck completion')
                require(first['completed_after']!=UNKNOWN and first['completed_after']>=first['fence_value'], 'unsafe retirement')
                require(bool(first['wait_called']) == (first['completed_before']<first['fence_value']), 'incorrect wait classification')
            else:
                require(not first['wait_called'] and first['source_frame']==UNKNOWN, 'empty slot fabricated a source')
            if not first['wait_called']:
                require(group[1]['wall_ms']==0, 'skipped wait has duration')
            waits.append({k:first[k] for k in identity} | dict(
                precheck_ms=group[0]['wall_ms'], wait_call_ms=group[1]['wall_ms'], postcheck_ms=group[2]['wall_ms']))
        primary=sum((r['end_ns']-r['start_ns'])/1e6 for r in intervals)
        require(primary <= wall+1e-6, 'primary spans exceed frame; nested fence spans must not be added')
        for r in intervals:
            stages.setdefault(r['stage'],[]).append(r['wall_ms'])
        completed.append(dict(frame=row['renderer_frame'],scope=row['scope'],status=row['stage'],
                              wall_ms=wall,recorded_primary_ms=primary,unattributed_ms=max(0,wall-primary),
                              fence_waits=waits,intervals=[{k:r[k] for k in ('stage','wall_ms','thread_cpu_ns')} for r in intervals]))
        pending=[]
    require(not pending, 'missing terminal frame record')
    world=[r for r in completed if r['scope']=='world' and r['status']=='complete']
    require(len({r['frame'] for r in world})==len(world), 'duplicate completed world frame')
    if gpu_csv is not None:
        with Path(gpu_csv).open(newline='') as stream:
            frames=[int(r['frame']) for r in csv.DictReader(stream)]
        require(len(set(frames))==len(frames) and set(frames)=={r['frame'] for r in world}, 'complete world/GPU frame join differs')
    report = dict(status='validated',schema_version=1,records=len(raw),frames=len(completed),world_frames=len(world),
                incomplete_frames=0,skipped_frames=sum(r['status']=='skipped' for r in completed),
                abandoned_frames=sum(r['status']=='abandoned' for r in completed),
                cpu_scope='Wall intervals; GetThreadTimes samples are coarse, not precise active execution.',
                wait_scope='Explicit fence calls retire recorded older submissions; no OS/driver cause inferred.',
                gpu_timing_qualification=False,
                stage_max_ms={k:max(v) for k,v in stages.items()},
                worst_world_frames=sorted(world,key=lambda r:r['wall_ms'],reverse=True)[:12])
    if include_frames:
        report['frame_records'] = completed
    return report
