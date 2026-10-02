"""Validate opt-in source identity and live primary transform diagnostics."""
import argparse
import json
import math
from pathlib import Path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-log',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--minimum-frames',type=int,default=1)
    args=parser.parse_args()
    if args.out.exists():raise ValueError('Use a new receipt path to preserve earlier evidence')
    sources=[];frames=[];errors=[]
    for line in args.client_log.read_text(errors='replace').splitlines():
        for marker,destination in [('map_transform_source ',sources),('map_transform_frame ',frames)]:
            if marker in line:
                try:destination.append(json.loads(line.split(marker,1)[1]))
                except ValueError:errors.append('Malformed transform diagnostic JSON')
    if not sources:errors.append('No selected source transform snapshots')
    if len(frames)<args.minimum_frames:errors.append('Insufficient live primary frame records')
    for record in sources:
        identity=str((record.get('node_name'),record.get('mesh'),record.get('primitive')))
        for field in ['finite','orientation_valid','normal_valid','source_bounds_valid','source_transform_valid']:
            if not record.get(field):errors.append(identity+': '+field+' failed')
        for field,size in [('evaluated',16),('shader_world',12),('shader_normal',12),('source_bounds',6),('render_bounds',6),('shader_world_bounds',6),('expected_world_bounds',6)]:
            value=record.get(field,[])
            if len(value)!=size or not all(isinstance(x,(int,float)) and math.isfinite(x) for x in value):errors.append(identity+': invalid '+field)
        if record.get('route') not in ('flattened','instanced'):errors.append(identity+': invalid transform route')
    for record in frames:
        if len(record.get('camera',[]))!=5 or not all(math.isfinite(x) for x in record.get('camera',[])):errors.append('Invalid live camera')
        if record.get('selected_clusters',-1)<0:errors.append('Invalid GPU selection feedback')
    poses={tuple(record['camera']) for record in frames if len(record.get('camera',[]))==5}
    result={'passed':not errors,'clientLog':str(args.client_log.resolve()),'sourceRecords':len(sources),
            'liveFrameRecords':len(frames),'distinctCameraPoses':len(poses),'gpuVertexReadbackQualified':False,
            'originalGameAssemblyQualified':False,'errors':errors}
    args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(result,indent=2))
    print(json.dumps(result));return 0 if result['passed'] else 1


if __name__=='__main__':raise SystemExit(main())
