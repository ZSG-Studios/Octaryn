"""Build the tiny CPU-only MikkTSpace bridge; never configures the engine."""
import hashlib
from pathlib import Path
import subprocess
import sys
import urllib.request

PIN = '3e895b49d05ea07e4c2133156cfa94369e19e409'
HASHES = {'mikktspace.c': 'de87e74107df766ce68108801262bd8d53899414236b59810509a8fc2a51e288',
          'mikktspace.h': '17fc433894f24c73753d548086cc4d8c5c0379f4a6edfb98b5da243e4f0bc3d0'}


def main():
    root = Path(__file__).resolve().parents[3]
    source = root / 'build/dependencies/mikktspace' / PIN
    output = root / 'build/release-windows/basegame/map-cook-tools'
    source.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    for name, expected in HASHES.items():
        path = source / name
        data = path.read_bytes() if path.exists() else urllib.request.urlopen(
            f'https://raw.githubusercontent.com/mmikk/MikkTSpace/{PIN}/{name}').read()
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError(f'Pinned MikkTSpace source hash mismatch: {name}')
        if not path.exists():
            path.write_bytes(data)
    sys.path.insert(0, str(root / 'tools/build'))
    import vsenv
    vs_root = vsenv.find_vs_root()
    vsenv.import_vs_environment(vs_root, 'x64')
    vsenv.prepend_tool_dirs(root, vs_root, 'x64')
    compiler = vsenv.resolve_tool('clang-cl')
    command = [compiler, '/nologo', '/O2', '/LD', '/TC', '/I' + str(source),
               str(Path(__file__).with_name('MikkBridge.c')), str(source / 'mikktspace.c'),
               '/Fe:' + str(output / 'map_mikk.dll')]
    subprocess.run(command, cwd=output, check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    print(output / 'map_mikk.dll')


if __name__ == '__main__':
    main()
