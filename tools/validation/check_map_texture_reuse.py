import json,os,re,shlex,subprocess,sys,argparse,struct,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True)
a.add_argument('--map',type=Path,default=ROOT/'octaryn-client/Assets/Maps/map.json')
a.add_argument('--geometry-cache',type=Path,default=Path(os.environ.get('APPDATA',''))/'ZSGStudios/Octaryn/geometry-cache/v3')
opts=a.parse_args();out=opts.out.resolve();out.mkdir(exist_ok=False,parents=True)
sys.path.insert(0,str(ROOT/'tools/build'));import vsenv
vs=vsenv.find_vs_root();vsenv.import_vs_environment(vs,'x64');vsenv.prepend_tool_dirs(ROOT,vs,'x64')
ninja=(ROOT/'build/release-windows/cmake/build.ninja').read_text()
def args(source):
 m=re.search(r'build [^\n]*\.dir/'+re.escape(source)+r'\.obj:[^\n]*\n(?P<block>(?:  [^\n]*\n)+)',ninja);b=m.group('block');return [str(vsenv.resolve_tool('clang-cl')),'/nologo','/std:c++latest','/EHsc','/MD','/Gy','/D_CRT_SECURE_NO_WARNINGS']+shlex.split(re.search(r'  INCLUDES = (.*)',b).group(1))+shlex.split(re.search(r'  DEFINES = (.*)',b).group(1))
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
(out/'opaque.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(bytes([0,255,255,255,255])))+chunk(b'IEND',b''))
sources=['MapModel.cpp','MapMaterials.cpp','MapSource.cpp','MapMeshOptimization.cpp','MapForwardGeometry.cpp','MapAssetTextureUpload.cpp','MapAssetPrepare.cpp','MapImages.cpp','MapMipmaps.cpp','MapTextureCache.cpp','MapTextureHash.cpp']
cmd=args('octaryn-client/Source/MapWorld/MapAssetAllocation.cpp')+[str(ROOT/'tools/Source/MapTextureReuseProbe/main.cpp'),str(ROOT/'octaryn-client/Source/WorldStreaming/TileSet.cpp')]+[str(ROOT/'octaryn-client/Source/MapWorld'/s) for s in sources]+[str(ROOT/'build/release-windows/shared/native/lib/octaryn_content_digest.lib'),str(ROOT/'build/release-windows/shared/native/lib/octaryn_gltf_buffers.lib'),str(ROOT/'build/release-windows/deps/build/meshoptimizer/meshoptimizer.lib'),str(ROOT/'build/release-windows/deps/build/fastgltf/fastgltf.lib'),'/Fe'+str(out/'probe.exe'),'/Fo'+str(out)+'\\','/link','/OPT:REF']
p=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(p.stdout+p.stderr);r={'gpu':False,'compileExit':p.returncode}
if not p.returncode:
 p=subprocess.run([str(out/'probe.exe'),str(opts.map.resolve()),str(opts.geometry_cache),str(out/'opaque.png')],capture_output=True,text=True);(out/'cpu.log').write_text(p.stdout+p.stderr);r['testExit']=p.returncode;r['output']=p.stdout
checks=[]
for s in ['MapWorld/MapAssetAllocation.cpp','MapWorld/MapAssetTextureUpload.cpp','MapWorld/MapAssetPrepare.cpp','WorldStreaming/TileSession.cpp']:
 source='octaryn-client/Source/'+s;p=subprocess.run(args(source)+['/clang:-fsyntax-only',str(ROOT/source)],capture_output=True,text=True);(out/(Path(s).stem+'-syntax.log')).write_text(p.stdout+p.stderr);checks.append({'source':s,'exit':p.returncode})
r['syntax']=checks;r['passed']=r['compileExit']==0 and r.get('testExit')==0 and all(c['exit']==0 for c in checks);(out/'result.json').write_text(json.dumps(r,indent=2));print(json.dumps(r));sys.exit(0 if r['passed'] else 1)
