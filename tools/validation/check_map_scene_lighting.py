"""Exercise production scene environment and transformed glTF point lights without a GPU."""
import argparse
import copy
import hashlib
import json
import re
from pathlib import Path
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/build'))
import vsenv


def fixtures(root):
    root.mkdir()
    data=struct.pack('<9f',0,0,0,1,0,0,0,0,1)
    (root/'triangle.bin').write_bytes(data)
    base={'asset':{'version':'2.0'},'buffers':[{'uri':'triangle.bin','byteLength':36}],
          'bufferViews':[{'buffer':0,'byteLength':36}],
          'accessors':[{'bufferView':0,'componentType':5126,'count':3,'type':'VEC3','min':[0,0,0],'max':[1,0,1]}],
          'meshes':[{'primitives':[{'attributes':{'POSITION':0}}]}],
          'nodes':[{'mesh':0}], 'scenes':[{'nodes':[0]}], 'scene':0}
    (root/'plain.gltf').write_text(json.dumps(base))
    environment={'version':1,'sky_enabled':False,'ambient':[.1,.2,.3],
                 'directional_color':[.4,.5,.6],'directional_direction':[0,-1,0],'background':[.01,.02,.03]}
    valid=copy.deepcopy(base)
    valid['extensionsUsed']=['KHR_lights_punctual']
    valid['extensions']={'KHR_lights_punctual':{'lights':[{'type':'point','range':3+i,'intensity':4+i,'color':[.2,.3,.4]} for i in range(8)]}}
    valid['nodes'] += [{'translation':[10,20,30],'rotation':[0,2**-.5,0,2**-.5],'scale':[2,3,4],'children':list(range(2,10))}]
    valid['nodes'] += [{'translation':[i,0,0],'extensions':{'KHR_lights_punctual':{'light':i}}} for i in range(8)]
    valid['scenes'][0]['nodes']=[0,1]
    valid['scenes'][0]['extras']={'octaryn_environment':environment}
    for name in ['valid','version','length','numeric','negative','huge','direction','zero','sky','unknown','missing','norange','rangezero','rangehuge','intensitynegative','intensityhuge','spot','colornegative','positionhuge','overflow','nonfinite','alternate']:
        doc=copy.deepcopy(valid);env=doc['scenes'][0]['extras']['octaryn_environment'];light=doc['extensions']['KHR_lights_punctual']['lights'][0]
        if name=='version':env['version']=2
        elif name=='length':env['ambient']=[1,2]
        elif name=='numeric':env['ambient'][0]='invalid'
        elif name=='negative':env['ambient'][0]=-1
        elif name=='huge':env['background'][0]=100001
        elif name=='direction':env['directional_direction']=[0,2,0]
        elif name=='zero':env['directional_direction']=[0,0,0]
        elif name=='sky':env['sky_enabled']='false'
        elif name=='unknown':env['unknown']=1
        elif name=='missing':del env['ambient']
        elif name=='norange':del light['range']
        elif name=='rangezero':light['range']=0
        elif name=='rangehuge':light['range']=100001
        elif name=='intensitynegative':light['intensity']=-1
        elif name=='intensityhuge':light['intensity']=1e9
        elif name=='spot':light.update(type='spot',spot={'innerConeAngle':0,'outerConeAngle':.7})
        elif name=='colornegative':light['color'][0]=-1
        elif name=='positionhuge':doc['nodes'][1]['translation'][0]=100001
        elif name=='overflow':
            doc['nodes']=[{'mesh':0}]+[{'extensions':{'KHR_lights_punctual':{'light':0}}} for _ in range(1025)]
            doc['scenes'][0]['nodes']=list(range(1026))
        elif name=='nonfinite':env['ambient'][0]=float('inf')
        elif name=='alternate':
            doc['scenes'].append({'nodes':[0]});doc['scene']=1
        (root/(name+'.gltf')).write_text(json.dumps(doc))


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();out=args.out.resolve()
    if out.exists():raise ValueError('Use a new output directory to preserve earlier evidence')
    out.mkdir(parents=True);fixtures(out/'fixtures')
    vs=vsenv.find_vs_root();vsenv.import_vs_environment(vs,'x64');vsenv.prepend_tool_dirs(ROOT,vs,'x64')
    registry=(ROOT/'cmake/Dependencies/DependencyRegistry.cmake').read_text()
    version=re.search(r'set\(OCTARYN_DEP_slang_sdk_version "([^\"]+)"\)',registry).group(1)
    include_paths=['build/dependencies/src/fastgltf/include','build/dependencies/src/fastgltf/deps/simdjson',
                   'build/dependencies/src/glaze/include','build/dependencies/slang-rhi/include',
                   'build/dependencies/slang-rhi-windows-x64-Release/include',
                   'build/dependencies/slang-'+version+'/include','octaryn-client/Source/MapWorld',
                   'octaryn-shared/Source/Libraries/Gltf','octaryn-shared/Source/Libraries/Content']
    cmd=[vsenv.resolve_tool('clang-cl'),'/nologo','/std:c++latest','/EHsc','/MD','/O2','/W4','/WX',
         '/D_CRT_SECURE_NO_WARNINGS']+['/I'+str(ROOT/p) for p in include_paths]
    cmd += [str(ROOT/'tools/Source/MapSceneLightingProbe/main.cpp')]
    sources=[ROOT/'octaryn-client/Source/MapWorld'/name for name in ('MapModel.cpp','MapMaterials.cpp','MapSource.cpp')]
    libraries=['build/release-windows/shared/native/lib/octaryn_gltf_buffers.lib',
               'build/release-windows/deps/build/meshoptimizer/meshoptimizer.lib',
               'build/release-windows/deps/build/fastgltf/fastgltf.lib']
    cmd += [str(p) for p in sources]+[str(ROOT/p) for p in libraries]
    cmd += ['/Fe'+str(out/'MapSceneLightingProbe.exe'),'/Fo'+str(out)+'\\']
    receipt={'status':'failed','gpuUsed':False,'cpuCommand':cmd,'sourceSha256':{}}
    with (out/'cpu.log').open('w') as log:
        result=subprocess.run(cmd,cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=120);receipt['compileExit']=result.returncode
        if result.returncode==0:
            result=subprocess.run([str(out/'MapSceneLightingProbe.exe'),str(out/'fixtures')],cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=30);receipt['testExit']=result.returncode
    for path in sources+[ROOT/'tools/Source/MapSceneLightingProbe/main.cpp',ROOT/'octaryn-client/Source/MapWorld/MapLayerImport.h',ROOT/'octaryn-client/Source/MapWorld/MapSceneLightingImport.h']:
        receipt['sourceSha256'][str(path.relative_to(ROOT))]=hashlib.sha256(path.read_bytes()).hexdigest()
    if receipt.get('testExit')==0:receipt['status']='passed'
    (out/'result.json').write_text(json.dumps(receipt,indent=2));print((out/'cpu.log').read_text());print(receipt['status'])
    return 0 if receipt['status']=='passed' else 1


if __name__=='__main__':raise SystemExit(main())
