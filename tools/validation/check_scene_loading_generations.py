"""Isolated shared scene verifier generations and existing preparation checks; no GPU."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/build'))
import vsenv

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();out=args.out.resolve()
    if out.exists():raise ValueError('Preserve prior evidence; use a fresh output')
    out.mkdir(parents=True)
    vs=vsenv.find_vs_root();vsenv.import_vs_environment(vs,'x64');vsenv.prepend_tool_dirs(ROOT,vs,'x64')
    compiler=vsenv.resolve_tool('clang-cl');source=ROOT/'octaryn-shared/Source/Libraries/SceneLoading'
    files=[source/name for name in ('SceneLoading.cpp','ScenePreparation.cpp','SceneVerification.cpp','GltfMetadata.cpp','CatalogMetadata.cpp','SceneCookIdentity.cpp')]
    probe=ROOT/'tools/Source/SceneLoadingProbe/main.cpp'
    includes=[source,ROOT/'octaryn-shared/Source/Libraries/NativeJobs',ROOT/'octaryn-shared/Source/Libraries/Content',ROOT/'octaryn-shared/Source/Libraries/Gltf',ROOT/'build/dependencies/src/fastgltf/include',ROOT/'build/dependencies/src/glaze/include',ROOT/'build/dependencies/src/meshoptimizer/src']
    libs=[ROOT/'build/release-windows/shared/native/lib'/name for name in ('octaryn_native_jobs.lib','octaryn_content_digest.lib','octaryn_gltf_buffers.lib')]+[ROOT/'build/release-windows/deps/build/fastgltf/fastgltf.lib',ROOT/'build/release-windows/deps/build/meshoptimizer/meshoptimizer.lib']
    common=[compiler,'/nologo','/std:c++latest','/EHsc','/MD','/O1','/D_CRT_SECURE_NO_WARNINGS','/DOCTARYN_SCENE_LOADING_BUILD']+['/I'+str(i) for i in includes]
    exe=out/'SceneLoadingProbe.exe';cmd=common+[str(p) for p in files]+[str(probe)]+[str(p) for p in libs]+['/Fe'+str(exe),'/Fo'+str(out)+'\\']
    receipt={'status':'failed','gpu':False,'sourceSha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files+[probe]},'command':cmd}
    with (out/'compile.log').open('w') as log:checked=subprocess.run(cmd,cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=240)
    receipt['compileExit']=checked.returncode
    print((out/'compile.log').read_text(),flush=True)
    if checked.returncode==0:
        shutil.copy2(ROOT/'build/release-windows/shared/native/bin/octaryn_native_jobs.dll',out)
        with (out/'cpu.log').open('w') as log:checked=subprocess.run([str(exe)],cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=60)
        receipt['cpuExit']=checked.returncode;print((out/'cpu.log').read_text(),flush=True)
        stale=out/'stale';stale.mkdir();(stale/'catalog.json').write_text('{"version":2,"part_triangles":65536,"mesh_count":1}')
        with (out/'stale.log').open('w') as log:checked=subprocess.run([str(exe),'--scene',str(stale/'catalog.json')],cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=60)
        receipt['staleCatalogRejected']=checked.returncode!=0 and 'scene_loading_probe_failed' in (out/'stale.log').read_text()
        if receipt['cpuExit']==0 and receipt['staleCatalogRejected']:receipt['status']='passed'
    (out/'result.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'status':receipt['status'],'receipt':str(out/'result.json')}),flush=True)
    return 0 if receipt['status']=='passed' else 1
if __name__=='__main__':raise SystemExit(main())
