"""Compile and run production frame-cap deadline/timer and pending-call CPU checks."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/build'))
import vsenv


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,default=ROOT/'build/windows-x64/tools/frame-cap-wait-v1')
    args=parser.parse_args();out=args.out.resolve()
    if out.exists():raise ValueError('Preserve previous evidence; use a new output directory')
    out.mkdir(parents=True);vs=vsenv.find_vs_root();vsenv.import_vs_environment(vs,'x64');vsenv.prepend_tool_dirs(ROOT,vs,'x64')
    source=ROOT/'tools/Source/FrameCapWaitProbe/main.cpp';exe=out/'FrameCapWaitProbe.exe'
    command=[vsenv.resolve_tool('clang-cl'),'/nologo','/std:c++20','/EHsc','/MD','/O2','/W4','/WX',
             '/D_CRT_SECURE_NO_WARNINGS',str(source),'/Fe'+str(exe),'/Fo'+str(out/'main.obj')]
    receipt={'version':1,'kind':'production-helper-authored-cpu','status':'failed','gpu':False,'command':command,
             'visiblePerformanceQualified':False,'watchdogChanged':False,'sources':{}}
    for p in [source,ROOT/'octaryn-client/Source/App/OpenWorld/ResponsiveFrameWait.h',ROOT/'octaryn-client/Source/App/OpenWorld/PostRenderTrace.h']:
        receipt['sources'][str(p.relative_to(ROOT))]=hashlib.sha256(p.read_bytes()).hexdigest()
    with (out/'checks.log').open('w') as log:
        built=subprocess.run(command,cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=120);receipt['compileExitCode']=built.returncode
        if built.returncode==0:
            checked=subprocess.run([str(exe)],cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=20);receipt['checksExitCode']=checked.returncode
            if checked.returncode==0:receipt['status']='passed'
    (out/'result.json').write_text(json.dumps(receipt,indent=2)+'\n');print((out/'checks.log').read_text());print(json.dumps({'status':receipt['status'],'receipt':str(out/'result.json')}))
    return 0 if receipt['status']=='passed' else 1


if __name__=='__main__':raise SystemExit(main())
