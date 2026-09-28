"""Small native CNG/portable hash fixtures using independent hashlib receipts."""
from pathlib import Path
import hashlib
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs


def fixtures(root):
    root.mkdir(parents=True, exist_ok=True)
    vectors = []
    for length in (0, 1, 55, 56, 63, 64, 65, 119, 120, 127, 128, 129, 257, 1048577):
        data = bytes((i * 37 + 11) & 255 for i in range(length))
        (root / f'{length}.bin').write_bytes(data)
        vectors.append(f'{length} {hashlib.sha256(data).hexdigest()}')
    (root / 'vectors.txt').write_text('\n'.join(vectors))
    data = bytes((i * 37 + 11) & 255 for i in range(257))
    keys = []
    for role in range(5):
        weighted, coverage, cutoff, factor = role % 2, (role // 2) % 2, .25, .75
        header = struct.pack('<IIIIIff', 0x5a534754, 3, role, weighted, coverage, cutoff, factor)
        keys.append(f'{role} {weighted} {coverage} {cutoff} {factor} {hashlib.sha256(header + data).hexdigest()}')
    (root / 'keys.txt').write_text('\n'.join(keys))
    cache = ROOT / 'build/release-windows/client/map-variants-opaque/bc7'
    candidates = sorted(cache.rglob('*.dds'), key=lambda path: path.stat().st_size)
    selected = {}
    for path in candidates:
        if path.stat().st_size > 1024 * 1024:
            break
        with path.open('rb') as source:
            source.seek(128)
            fmt = struct.unpack('<I', source.read(4))[0]
        kind = 'bc7' if fmt in (98, 99) else 'rgba'
        if kind not in selected:
            selected[kind] = path
        if len(selected) == 2:
            break
    if len(selected) != 2:
        raise RuntimeError('Need two existing small BC7/RGBA cooked assets; no recook performed')
    for kind, path in selected.items():
        data = path.read_bytes()
        receipt = Path(str(path) + '.sha256').read_text()
        assert hashlib.sha256(data).hexdigest() == receipt
        shutil.copyfile(path, root / f'{kind}.dds')
        (root / f'{kind}.dds.sha256').write_text(receipt)
        print(f'asset={path} bytes={len(data)} sha256={receipt}', flush=True)


def main():
    output = ROOT / 'build/release-windows/tools/map-hash-probe'
    output.mkdir(parents=True, exist_ok=True)
    vs = find_vs_root()
    import_vs_environment(vs, 'x64')
    prepend_tool_dirs(ROOT, vs, 'x64')
    source = ROOT / 'octaryn-client/Source/MapWorld'
    for mode in ('cng', 'portable'):
        command = ['clang-cl', '/nologo', '/O2', '/MD', '/EHsc', '/std:c++20', '/I' + str(source)]
        if mode == 'portable':
            command += ['/DOCTARYN_MAP_HASH_PORTABLE_TEST=1']
        command += [str(ROOT / 'tools/Source/MapHashProbe/main.cpp'), str(source / 'MapTextureHash.cpp'),
                    str(source / 'MapTextureCache.cpp'), '/Fe:map_hash_probe.exe']
        subprocess.run(command, cwd=output, check=True, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
        root = output / mode
        fixtures(root)
        print(f'mode={mode}', flush=True)
        subprocess.run([str(output / 'map_hash_probe.exe'), str(root)], check=True,
                       creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)


if __name__ == '__main__':
    main()
