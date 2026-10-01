"""Native stationary-support regression; runs physics only, never a GPU client."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true', help='Observe the currently built character library')
    args = parser.parse_args()
    output = ROOT / 'build/release-windows/tools/character-support-probe'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    owner = ROOT / 'octaryn-shared/Source/Libraries/CharacterMotion'
    box = ROOT / 'build/dependencies/src/box3d'
    libraries = [ROOT / 'build/release-windows/shared/native/lib/octaryn_character_motion.lib',
                 ROOT / 'build/release-windows/deps/build/box3d/src/box3d.lib']
    flags = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20',
             '/I' + str(owner), '/I' + str(box / 'include')]
    source = [] if args.baseline else [str(owner / 'MeshCharacterStep.cpp')]
    for name, probe in [('support', ROOT / 'tools/Source/CharacterSupportProbe/main.cpp'),
                        ('movement', ROOT / 'tools/Source/PhysicsProbe/CharacterMotionProbe.cpp')]:
        binary = f'{name}_{"baseline" if args.baseline else "current"}.exe'
        subprocess.run(flags + [str(probe)] + source + ['/Fe:' + binary, '/link'] +
                       [str(path) for path in libraries], cwd=output, check=True)
        subprocess.run([str(output / binary)] + (['--observe'] if args.baseline and name == 'support' else []),
                       check=True)


if __name__ == '__main__':
    main()
