"""Compile production weighted-layer parsing/ABI checks and affected Slang entries; no GPU."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import struct
import base64
import copy

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
    fixture=out/'fixtures';fixture.mkdir()
    fields=[('POSITION','VEC3',[(0,0,0),(1,0,0),(0,0,1)]),('NORMAL','VEC3',[(0,1,0)]*3),('TEXCOORD_0','VEC2',[(0,0),(1,0),(0,1)]),('COLOR_0','VEC4',[(1,1,1,.5)]*3),('_OCTARYN_BLEND0','VEC4',[(.25,.75,0,0)]*3),('_OCTARYN_BLEND1','VEC4',[(0,0,0,0)]*3)]
    binary=b'';accessors=[];views=[];attributes={}
    for name,kind,values in fields:
        attributes[name]=len(accessors);offset=len(binary)
        binary+=struct.pack('<'+'f'*sum(map(len,values)),*(v for row in values for v in row))
        views.append({'buffer':0,'byteOffset':offset,'byteLength':len(binary)-offset})
        accessor={'bufferView':len(views)-1,'componentType':5126,'count':3,'type':kind}
        if name=='POSITION':accessor.update(min=[0,0,0],max=[1,0,1])
        accessors.append(accessor)
    doc={'asset':{'version':'2.0'},'buffers':[{'uri':'valid.bin','byteLength':len(binary)}],'bufferViews':views,'accessors':accessors,'images':[{'uri':'test.png'}],'textures':[{'source':0}],'materials':[{'pbrMetallicRoughness':{'metallicFactor':0},'extras':{'octaryn_material_layers':{'version':1,'layers':[{'texture':0},{'texture':0}]}}}],'meshes':[{'primitives':[{'attributes':attributes,'material':0}]}],'nodes':[{'mesh':0}],'scenes':[{'nodes':[0]}],'scene':0}
    (fixture/'valid.bin').write_bytes(binary)
    (fixture/'test.png').write_bytes(base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg=='))
    for name in ('valid','missing','negative','unused','normalized','wrongtype'):
        case=copy.deepcopy(doc)
        if name=='missing':del case['meshes'][0]['primitives'][0]['attributes']['_OCTARYN_BLEND1']
        elif name=='normalized':case['accessors'][4]['normalized']=True
        elif name=='wrongtype':case['accessors'][4]['componentType']=5125
        elif name in ('negative','unused'):
            data=bytearray(binary);struct.pack_into('<f',data,views[4]['byteOffset']+(0 if name=='negative' else 12),-1 if name=='negative' else .1)
            (fixture/(name+'.bin')).write_bytes(data);case['buffers'][0]['uri']=name+'.bin'
        (fixture/(name+'.gltf')).write_text(json.dumps(case))
    registry=(ROOT/'cmake/Dependencies/DependencyRegistry.cmake').read_text()
    version=re.search(r'set\(OCTARYN_DEP_slang_sdk_version "([^"]+)"\)',registry).group(1)
    slang=ROOT/'build/dependencies'/('slang-'+version)/'bin/slangc.exe'
    fastgltf=ROOT/'build/dependencies/src/fastgltf'
    library=ROOT/'build/release-windows/deps/build/fastgltf/fastgltf.lib'
    vs=vsenv.find_vs_root();vsenv.import_vs_environment(vs,'x64');vsenv.prepend_tool_dirs(ROOT,vs,'x64')
    exe=out/'MapLayerProbe.exe'
    command=[vsenv.resolve_tool('clang-cl'),'/nologo','/std:c++latest','/EHsc','/MD','/O2','/W4','/WX',
        '/D_CRT_SECURE_NO_WARNINGS','/I'+str(fastgltf/'include'),'/I'+str(fastgltf/'deps/simdjson'),'/I'+str(ROOT/'build/dependencies/src/glaze/include'),'/I'+str(ROOT/'build/dependencies/slang-rhi/include'),'/I'+str(ROOT/'build/dependencies/slang-rhi-windows-x64-Release/include'),
        '/I'+str(ROOT/'build/dependencies'/('slang-'+version)/'include'),str(ROOT/'tools/Source/MapLayerProbe/main.cpp'),
        *[str(ROOT/'octaryn-client/Source/MapWorld'/name) for name in ('MapMaterials.cpp','MapModel.cpp','MapSource.cpp','MapSourceReader.cpp')],str(ROOT/'tools/Source/MapTileCook/Write.cpp'),str(ROOT/'tools/Source/MapTileCook/Materials.cpp'),'/I'+str(ROOT/'octaryn-client/Source/MapWorld'),'/I'+str(ROOT/'octaryn-shared/Source/Libraries/Gltf'),'/I'+str(ROOT/'octaryn-shared/Source/Libraries/Content'),str(ROOT/'build/release-windows/shared/native/lib/octaryn_gltf_buffers.lib'),str(ROOT/'build/release-windows/deps/build/meshoptimizer/meshoptimizer.lib'),str(library),'/Fe'+str(exe),'/Fo'+str(out)+'\\']
    encoding=[]
    for name in ('Map/WorldMap.slang','VirtualGeometry/MaterialResolve.slang'):
        text=(ROOT/'octaryn-client/Shaders'/name).read_text()
        if not re.search(r'map_material_unlit\([^;]+\)\?1:0',text):raise ValueError('UNORM unlit marker must be 1 or 0')
        encoding.append(name)
    if 'lighting_is_map(voxel) && material.a>.5' not in (ROOT/'octaryn-client/Shaders/Lighting/Surface.slang').read_text():
        raise ValueError('Map-only unlit discriminator mismatches UNORM marker')
    receipt={'unormEncodingChecked':encoding,'status':'failed','gpu':False,'sourceSha256':{},'shaderCases':[],'cpuCommand':command}
    for path in [slang,library,ROOT/'tools/Source/MapLayerProbe/main.cpp',ROOT/'octaryn-client/Source/MapWorld/MapMaterials.cpp',
                 ROOT/'octaryn-client/Source/MapWorld/MapMaterialRecord.h',ROOT/'octaryn-client/Source/VirtualGeometry/SceneMaterialJson.h',ROOT/'octaryn-client/Source/Rendering/Hdr/WorldHdr.h']:
        receipt['sourceSha256'][str(path.relative_to(ROOT))]=hashlib.sha256(path.read_bytes()).hexdigest()
    with (out/'cpu.log').open('w') as log:
        compiled=subprocess.run(command,cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=120)
        receipt['cpuCompileExit']=compiled.returncode
        if compiled.returncode==0:
            tested=subprocess.run([str(exe),str(fixture)],cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=20)
            receipt['cpuChecksExit']=tested.returncode
    print((out/'cpu.log').read_text(),flush=True)
    env=os.environ.copy()
    dxc=ROOT/'build/dependencies/slang-rhi-windows-x64-Release'
    env['PATH']=str(dxc)+os.pathsep+env['PATH']
    for source,entry,stage in SHADERS+[('VirtualGeometry/RayExpand.slang','expandRayGeometry','compute'),('VirtualGeometry/SceneHybridRaster.slang','software_main','compute')]:
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
    receipt['syntaxCases']=[]
    import shlex
    ninja=(ROOT/'build/release-windows/cmake/build.ninja').read_text()
    block=ninja[ninja.index('build CMakeFiles/octaryn_client_render_backend.dir/octaryn-client/Source/MapWorld/MapModel.cpp.obj:'):]
    includes=re.search(r'  INCLUDES = (.*)',block).group(1)
    for source in ('MapWorld/MapModel.cpp','MapWorld/MapSourceReader.cpp','MapWorld/MapRendererMaterials.cpp','MapWorld/MapRendererTextures.cpp','MapWorld/MapAssetPrepare.cpp','VirtualGeometry/SceneCatalogImport.cpp','VirtualGeometry/GeometryCook.cpp','VirtualGeometry/GeometryCoarse.cpp','VirtualGeometry/GeometryMesh.cpp','VirtualGeometry/SceneRootPages.cpp','Animation/AnimationSerialization.cpp','VirtualGeometry/SceneCatalog.cpp'):
        cmd=[vsenv.resolve_tool('clang-cl'),'/nologo','/std:c++latest','/EHsc','/MD','/D_CRT_SECURE_NO_WARNINGS','/clang:-fsyntax-only']+shlex.split(includes)+['/I'+str(ROOT/'build/dependencies/src/meshoptimizer/demo')]+[str(ROOT/'octaryn-client/Source'/source)]
        tested=subprocess.run(cmd,capture_output=True,text=True,timeout=120)
        (out/(source.replace('/','-')+'.syntax.log')).write_text(tested.stdout+tested.stderr)
        receipt['syntaxCases'].append({'source':source,'exit':tested.returncode})
        print('layer_syntax',source,'exit',tested.returncode,flush=True)
    if receipt.get('cpuChecksExit')==0 and all(c['exit']==0 for c in receipt['syntaxCases']) and all(c['exit']==0 for c in receipt['shaderCases']):receipt['status']='passed'
    (out/'result.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
    print(json.dumps({'status':receipt['status'],'receipt':str(out/'result.json')}),flush=True)
    return 0 if receipt['status']=='passed' else 1


if __name__=='__main__':raise SystemExit(main())
