"""CPU-only production fence policy and deferred-map lifecycle source contracts."""
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs

def main():
    owner = ROOT/'octaryn-client/Source/MapWorld'
    source = (owner/'MapRendererRayLifecycle.cpp').read_text()
    assert source.index('if(map.ray_pending_fence)') < source.index('if(!requested')
    assert 'waitForFences' not in source.split('bool pump_map_ray_scene')[1].split('void finish_map_ray_scene')[0]
    assert 'map.ray_ready=true' not in (owner/'MapRendererRay.cpp').read_text()
    assert 'if(completed(map))publish(map)' in source
    assert 'finish_map_ray_scene(*map)' in (owner/'MapRenderer.cpp').read_text()
    assert 'if(SLANG_FAILED(queue->submit(submit)))frame_gpu_shutdown_failed("map_ray_enable_submit")' in source
    startup = (owner/'MapRendererRayInitialization.cpp').read_text()
    assert 'fail("submit");frame_gpu_shutdown_failed("map_ray_startup_submit")' in startup
    snapshot = (ROOT/'octaryn-client/Source/Rendering/RenderBackend/WorldRaySnapshot.cpp').read_text()
    assert 'current->map_blas.get()==map_blas' in snapshot
    assert 'next->map_blas=map_blas' in snapshot
    boot = (ROOT/'octaryn-client/Source/App/OpenWorld/OpenWorld.cpp').read_text()
    assert 'initial_scene.ray_tracing=controls.ui.ray_tracing_enabled!=0' in boot
    # MapStartup owns the deferred load now; OpenWorld must publish scene
    # settings before handing control to that startup worker.
    assert boot.index('open_world_renderer_set_scene(renderer,initial_scene)') < boot.index('start_map(window, renderer')
    output = ROOT/'build/release-windows/tools/map-ray-completion'
    output.mkdir(parents=True,exist_ok=True)
    vs=find_vs_root();import_vs_environment(vs,'x64');prepend_tool_dirs(ROOT,vs,'x64')
    args=['clang-cl','/nologo','/O2','/MD','/EHsc','/std:c++20',
          str(ROOT/'tools/validation/map_ray_completion_test.cpp'),'/Fe:map_ray_completion_test.exe']
    for command in (args,[str(output/'map_ray_completion_test.exe')]):
        subprocess.run(command,cwd=output,check=True,timeout=60,
                       creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)

if __name__=='__main__':
    main()
