"""Compile actual UI sampling policy and changed production translation units/shader."""
import argparse,json,os,re,shlex,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/build'))
import vsenv

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    out=parser.parse_args().out.resolve();out.mkdir(parents=True,exist_ok=False)
    vs=vsenv.find_vs_root();vsenv.import_vs_environment(vs,'x64');vsenv.prepend_tool_dirs(ROOT,vs,'x64')
    compiler=vsenv.resolve_tool('clang-cl');ninja=(ROOT/'build/release-windows/cmake/build.ninja').read_text()
    result={'gpu':False,'cases':[]}
    def run(name,command):
        process=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=120)
        (out/(name+'.log')).write_text(process.stdout+process.stderr)
        result['cases'].append({'name':name,'exit':process.returncode})
        print(name,process.returncode,flush=True)
        return process.returncode
    exe=out/'probe.exe'
    if run('compile-policy',[compiler,'/nologo','/std:c++20','/EHsc','/MD','/W4','/WX',
            str(ROOT/'tools/Source/RmlTextureSamplingProbe/main.cpp'),'/Fe'+str(exe),'/Fo'+str(out/'probe.obj')])==0:
        run('policy-checks',[str(exe)])
    for source in ('Rendering/Ui/RmlRenderer.cpp','Rendering/Ui/RmlClipMask.cpp','Ui/DeclaredScreen/DeclaredDocumentUi.cpp'):
        relative='octaryn-client/Source/'+source
        block=re.search(r'build [^\n]*\.dir/'+re.escape(relative)+r'\.obj:[^\n]*\n(?P<block>(?:  [^\n]*\n)+)',ninja).group('block')
        command=[compiler,'/nologo','/std:c++latest','/EHsc','/MD','/clang:-fsyntax-only']
        for field in ('INCLUDES','DEFINES'):command+=shlex.split(re.search(r'  '+field+r' = (.*)',block).group(1))
        run(Path(source).stem+'-syntax',command+[str(ROOT/relative)])
    slang=next((ROOT/'build/dependencies').rglob('slangc.exe'))
    os.environ['PATH']=str(ROOT/'build/dependencies/slang-rhi-windows-x64-Release')+os.pathsep+os.environ['PATH']
    for entry,stage in (('vertex_main','vertex'),('fragment_main','fragment')):
        for target in ('dxil','spirv'):
            name=entry+'-'+target
            run(name,[str(slang),str(ROOT/'octaryn-client/Shaders/Ui/Rml.slang'),'-entry',entry,
                '-stage',stage,'-target',target,'-profile','sm_6_6','-o',str(out/(name+'.bin'))])
    result['passed']=all(case['exit']==0 for case in result['cases'])
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    return 0 if result['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
