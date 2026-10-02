"""Compile production material parsing/ABI checks and affected Slang entries; no GPU."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/build'))
import vsenv

SHADERS=[('Map/WorldMap.slang','fragment_main','fragment'),('Map/WorldMap.slang','forward_main','fragment'),
 ('Map/WorldMapRT.slang','forward_main','fragment'),('VirtualGeometry/MaterialResolve.slang','fragment_main','fragment'),
 ('VirtualGeometry/SceneMaterialResolve.slang','fragment_main','fragment'),('Hdr/Composite.slang','main','compute'),
 ('Hdr/CompositeRT.slang','main','compute'),('Lighting/LocalDirect.slang','main','compute'),
 ('Lighting/LocalDirectRT.slang','main','compute'),('Hdr/MapReflectionTemporal.slang','main','compute'),
 ('Hdr/MapReflectionTemporalMap.slang','main','compute'),('Hdr/MapReflectionTemporalDeferred.slang','main','compute'),
 ('Hdr/MapReflectionQueue.slang','classify_main','compute'),('Hdr/MapReflectionQueue.slang','intersect_main','compute'),
 ('Hdr/MapReflectionQueueScreen.slang','screen_main','compute'),('Hdr/MapReflectionQueueScreen.slang','shade_main','compute'),
 ('Hdr/MapReflectionQueueScreenCoherent.slang','shade_main','compute'),
 ('Hdr/MapReflectionQueue.slang','shade_main','compute'),('Hdr/MapReflectionQueue.slang','recovery_main','compute'),
 ('Hdr/MapReflectionQueueCoherent.slang','classify_main','compute'),('Hdr/MapReflectionQueueCoherent.slang','intersect_main','compute'),
 ('Hdr/MapReflectionQueueCoherent.slang','shade_main','compute'),('Hdr/MapReflectionQueueCoherent.slang','recovery_main','compute')]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();out=args.out.resolve()
    if out.exists():raise ValueError('Preserve earlier evidence; use new --out')
    out.mkdir(parents=True)
    registry=(ROOT/'cmake/Dependencies/DependencyRegistry.cmake').read_text()
    version=re.search(r'set\(OCTARYN_DEP_slang_sdk_version "([^"]+)"\)',registry).group(1)
    slang=ROOT/'build/dependencies'/('slang-'+version)/'bin/slangc.exe'
    fastgltf=ROOT/'build/dependencies/src/fastgltf'
    library=ROOT/'build/release-windows/deps/build/fastgltf/fastgltf.lib'
    vs=vsenv.find_vs_root();vsenv.import_vs_environment(vs,'x64');vsenv.prepend_tool_dirs(ROOT,vs,'x64')
    exe=out/'MapUnlitProbe.exe'
    command=[vsenv.resolve_tool('clang-cl'),'/nologo','/std:c++latest','/EHsc','/MD','/O2','/W4','/WX',
        '/D_CRT_SECURE_NO_WARNINGS','/I'+str(fastgltf/'include'),'/I'+str(fastgltf/'deps/simdjson'),'/I'+str(ROOT/'build/dependencies/src/glaze/include'),'/I'+str(ROOT/'build/dependencies/slang-rhi/include'),'/I'+str(ROOT/'build/dependencies/slang-rhi-windows-x64-Release/include'),
        '/I'+str(ROOT/'build/dependencies'/('slang-'+version)/'include'),str(ROOT/'tools/Source/MapUnlitProbe/main.cpp'),
        str(ROOT/'octaryn-client/Source/MapWorld/MapMaterials.cpp'),str(library),'/Fe'+str(exe),'/Fo'+str(out)+'\\']
    encoding=[]
    for name in ('Map/WorldMap.slang','VirtualGeometry/MaterialResolve.slang'):
        text=(ROOT/'octaryn-client/Shaders'/name).read_text()
        if not re.search(r'map_material_unlit\([^;]+\)\?1:0',text):raise ValueError('UNORM unlit marker must be 1 or 0')
        encoding.append(name)
    if 'lighting_is_map(voxel) && material.a>.5' not in (ROOT/'octaryn-client/Shaders/Lighting/Surface.slang').read_text():
        raise ValueError('Map-only unlit discriminator mismatches UNORM marker')
    receipt={'unormEncodingChecked':encoding,'status':'failed','gpu':False,'sourceSha256':{},'shaderCases':[],'cpuCommand':command}
    for path in [slang,library,ROOT/'tools/Source/MapUnlitProbe/main.cpp',ROOT/'octaryn-client/Source/MapWorld/MapMaterials.cpp',
                 ROOT/'octaryn-client/Source/MapWorld/MapMaterialRecord.h',ROOT/'octaryn-client/Source/VirtualGeometry/SceneMaterialJson.h',ROOT/'octaryn-client/Source/Rendering/Hdr/WorldHdr.h']:
        receipt['sourceSha256'][str(path.relative_to(ROOT))]=hashlib.sha256(path.read_bytes()).hexdigest()
    with (out/'cpu.log').open('w') as log:
        compiled=subprocess.run(command,cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=120)
        receipt['cpuCompileExit']=compiled.returncode
        if compiled.returncode==0:
            tested=subprocess.run([str(exe)],cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=20)
            receipt['cpuChecksExit']=tested.returncode
    print((out/'cpu.log').read_text(),flush=True)
    env=os.environ.copy()
    dxc=ROOT/'build/dependencies/slang-rhi-windows-x64-Release'
    env['PATH']=str(dxc)+os.pathsep+env['PATH']
    for source,entry,stage in SHADERS:
        path=ROOT/'octaryn-client/Shaders'/source
        for target in ('dxil','spirv'):
            name=source.replace('/','-').replace('.slang','')+'-'+entry+'-'+target
            cmd=[str(slang),str(path),'-entry',entry,'-stage',stage,'-target',target,
                 '-profile','sm_6_6','-o',str(out/(name+'.bin'))]
            checked=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=60)
            (out/(name+'.log')).write_text(checked.stdout+checked.stderr)
            receipt['shaderCases'].append({'source':source,'entry':entry,'target':target,'exit':checked.returncode,'command':cmd})
            print('unlit_shader',name,'exit',checked.returncode,flush=True)
    for folder in ('octaryn-client/Shaders/Map','octaryn-client/Shaders/VirtualGeometry','octaryn-client/Shaders/Hdr',
                   'octaryn-client/Shaders/Lighting','octaryn-client/Shaders/RayTracing'):
        for path in (ROOT/folder).glob('*.slang'):
            receipt['sourceSha256'][str(path.relative_to(ROOT))]=hashlib.sha256(path.read_bytes()).hexdigest()
    if receipt.get('cpuChecksExit')==0 and all(c['exit']==0 for c in receipt['shaderCases']):receipt['status']='passed'
    (out/'result.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
    print(json.dumps({'status':receipt['status'],'receipt':str(out/'result.json')}),flush=True)
    return 0 if receipt['status']=='passed' else 1


if __name__=='__main__':raise SystemExit(main())
