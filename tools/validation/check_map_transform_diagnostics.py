"""Compile production transform guards and run adversarial CPU fixtures without GPU work."""
import argparse,json,re,shlex,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',type=Path,required=True);args=parser.parse_args();OUT=args.out.resolve()
if OUT.exists():raise ValueError('Use a new output directory to preserve earlier evidence')
OUT.mkdir(parents=True)
sys.path.insert(0,str(ROOT/'tools/build'));import vsenv
vs=vsenv.find_vs_root();vsenv.import_vs_environment(vs,'x64');vsenv.prepend_tool_dirs(ROOT,vs,'x64')
ninja=(ROOT/'build/release-windows/cmake/build.ninja').read_text();block=ninja[ninja.index('build CMakeFiles/octaryn_client_render_backend.dir/octaryn-client/Source/MapWorld/MapModel.cpp.obj:'):];includes=shlex.split(re.search(r'  INCLUDES = (.*)',block).group(1))
base=[str(vsenv.resolve_tool('clang-cl')),'/nologo','/std:c++latest','/EHsc','/MD','/D_CRT_SECURE_NO_WARNINGS']+includes
cmd=base+[str(ROOT/'tools/Source/MapTransformProbe/main.cpp'),str(ROOT/'octaryn-client/Source/VirtualGeometry/GeometryTransform.cpp'),'/Fe'+str(OUT/'probe.exe'),'/Fo'+str(OUT)+'\\']
p=subprocess.run(cmd,cwd=OUT,capture_output=True,text=True);(OUT/'compile.log').write_text(p.stdout+p.stderr);result={'gpu':False,'compileExit':p.returncode,'command':cmd}
if not p.returncode:
 p=subprocess.run([str(OUT/'probe.exe')],capture_output=True,text=True);(OUT/'cpu.log').write_text(p.stdout+p.stderr);result['testExit']=p.returncode
checks=[]
for source in ['MapWorld/MapModel.cpp','VirtualGeometry/SceneAssets.cpp','VirtualGeometry/WorldGeometry.cpp','MapWorld/MapRendererDraw.cpp']:
 p=subprocess.run(base+['/clang:-fsyntax-only',str(ROOT/'octaryn-client/Source'/source)],capture_output=True,text=True);(OUT/(Path(source).stem+'-syntax.log')).write_text(p.stdout+p.stderr);checks.append({'source':source,'exit':p.returncode})
result['syntax']=checks;result['passed']=result['compileExit']==0 and result.get('testExit')==0 and all(p['exit']==0 for p in checks);(OUT/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result));sys.exit(0 if result['passed'] else 1)
