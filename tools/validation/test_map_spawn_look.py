"""First authoritative map look regression against the real prediction owner."""
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs

def main():
    source = (ROOT/'octaryn-client/Source/App/OpenWorld/MapWorldSession.cpp').read_text()
    guard = 'if(!player_ready)input.yaw=input.pitch=std::numeric_limits<float>::quiet_NaN();'
    assert guard in source
    assert source.index(guard) < source.index('session.update(input, elapsed)')
    assert 'bool menu_loading = true;' in source
    assert 'const bool frame_ok = player_ready\n' in source
    assert ': graphics::open_world_renderer_render_menu(renderer);' in source
    assert '!player_ready || (options.benchmark_hidden && !benchmark_uncapped) ? 30 : settings.frame_cap_fps' in source
    assert "options.benchmark_hidden && uncapped_env && *uncapped_env=='1'" in source
    assert 'uncapped && player_ready' in source
    prediction = ROOT/'octaryn-client/Source/App/LocalSession'
    motion = ROOT/'octaryn-shared/Source/Libraries/CharacterMotion'
    output = ROOT/'build/release-windows/tools/map-spawn-look'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    includes = {prediction, motion}
    for header in ('BlockReceipts.h', 'JumpTransitions.h'):
        includes.add(next((ROOT/'octaryn-client').rglob(header)).parent)
    args = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20']
    args += ['/I'+str(path) for path in includes]
    args += [str(ROOT/'tools/validation/map_spawn_look_test.cpp'),
             str(prediction/'Prediction.cpp')]
    args += ['/Fe:map_spawn_look_test.exe']
    for command in (args, [str(output/'map_spawn_look_test.exe')]):
        subprocess.run(command, cwd=output, check=True, timeout=60,
                       creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)

if __name__ == '__main__':
    main()
