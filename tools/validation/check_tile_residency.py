"""Check bounded native tile identities, admission and actual host residency ABI without GPU work."""
import argparse,json,re,shlex,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,required=True);parser.add_argument('--manifest',type=Path);parser.add_argument('--collision-source',type=Path);args_cli=parser.parse_args();OUT=args_cli.out.resolve()
if OUT.exists():raise ValueError('Use a new output directory to preserve earlier evidence')
OUT.mkdir(parents=True)
sys.path.insert(0,str(ROOT/'tools/build'));import vsenv
vs=vsenv.find_vs_root();vsenv.import_vs_environment(vs,'x64');vsenv.prepend_tool_dirs(ROOT,vs,'x64')
ninja=(ROOT/'build/release-windows/cmake/build.ninja').read_text()
def args(source):
 match=re.search(r'build [^\n]*\.dir/'+re.escape(source)+r'\.obj:[^\n]*\n(?P<block>(?:  [^\n]*\n)+)',ninja);assert match,source
 block=match.group('block');inc=shlex.split(re.search(r'  INCLUDES = (.*)',block).group(1));defs=shlex.split(re.search(r'  DEFINES = (.*)',block).group(1)) if '  DEFINES =' in block else []
 return [str(vsenv.resolve_tool('clang-cl')),'/nologo','/std:c++latest','/EHsc','/MD','/D_CRT_SECURE_NO_WARNINGS']+inc+defs+['/I'+str(ROOT/'octaryn-shared/Source/HostAbi')]
wrapper=OUT/'ServerManifestProbe.cpp';wrapper.write_text('#include "'+(ROOT/'octaryn-server/Source/World/MapWorld/MapManifest.cpp').as_posix()+'"\n')
base=args('octaryn-client/Source/MapWorld/MapModel.cpp')+['/I'+str(ROOT/'octaryn-client/Source/App/OpenWorld'),'/I'+str(ROOT/'octaryn-client/Source/App/WorldLibrary')]; base += [str(ROOT/path) for path in ['build/release-windows/deps/build/meshoptimizer/meshoptimizer.lib','build/release-windows/deps/build/fastgltf/fastgltf.lib']];cmd=base+[str(wrapper)]+[str(ROOT/'octaryn-client/Source/MapWorld'/name) for name in ['MapModel.cpp','MapMaterials.cpp','MapSource.cpp','MapMeshOptimization.cpp']]+[str(ROOT/path) for path in ['tools/Source/TileResidencyProbe/main.cpp','octaryn-client/Source/WorldStreaming/TileSet.cpp','octaryn-client/Source/App/OpenWorld/MapManifest.cpp','octaryn-shared/Source/Libraries/Gltf/GltfTriangleReader.cpp','octaryn-shared/Source/Libraries/Gltf/GltfBufferViews.cpp','octaryn-shared/Source/Libraries/Gltf/GltfMappedViews.cpp','octaryn-shared/Source/Libraries/Gltf/GltfAccessorBounds.cpp','octaryn-server/Source/World/MapWorld/MapSceneGeometry.cpp']]+['/Fe'+str(OUT/'probe.exe'),'/Fo'+str(OUT)+'\\'];p=subprocess.run(cmd,capture_output=True,text=True);(OUT/'compile.log').write_text(p.stdout+p.stderr);result={'gpu':False,'compileExit':p.returncode}
if not p.returncode:
 if args_cli.collision_source and not args_cli.manifest:raise ValueError('--collision-source requires --manifest')
 p=subprocess.run([str(OUT/'probe.exe'),str(OUT)]+([str(args_cli.manifest.resolve())] if args_cli.manifest else [])+([str(args_cli.collision_source.resolve())] if args_cli.collision_source else []),capture_output=True,text=True);(OUT/'cpu.log').write_text(p.stdout+p.stderr);result['testExit']=p.returncode
checks=[]
for source in ['WorldStreaming/TileSession.cpp','WorldStreaming/TileSessionReadiness.cpp','WorldStreaming/TileSessionJobs.cpp','MapWorld/MapAssetPrepare.cpp','App/OpenWorld/MapManifest.cpp','App/OpenWorld/MapWorldSession.cpp','Host/ModuleHost.cpp','Rendering/RenderBackend/WorldTiles.cpp']:
 path='octaryn-client/Source/'+source;p=subprocess.run(args(path)+['/clang:-fsyntax-only',str(ROOT/path)],capture_output=True,text=True);(OUT/(Path(source).stem+'-syntax.log')).write_text(p.stdout+p.stderr);checks.append({'source':source,'exit':p.returncode})
for path in ['octaryn-server/Source/World/MapWorld/MapManifest.cpp','octaryn-server/Source/World/MapWorld/CollisionResidency.cpp','octaryn-client/Source/WorldStreaming/TileSessionRay.cpp']:
 p=subprocess.run(args(path)+['/clang:-fsyntax-only',str(ROOT/path)],capture_output=True,text=True);(OUT/(Path(path).parent.name+'-'+Path(path).stem+'-syntax.log')).write_text(p.stdout+p.stderr);checks.append({'source':path,'exit':p.returncode})
result['syntax']=checks;result['passed']=result['compileExit']==0 and result.get('testExit')==0 and all(p['exit']==0 for p in checks);(OUT/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result));sys.exit(0 if result['passed'] else 1)
