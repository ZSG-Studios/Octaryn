"""Verify the centrally pinned portable GPUPerfAPI SDK; never install or load it."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import urllib.request
import zipfile


def api_version(release_version):
    # Upstream CMake PROJECT_VERSION is major.minor.update.build.
    parts = release_version.split('.')
    if len(parts) != 4 or any(not re.fullmatch(r'\d+', part) for part in parts):
        raise ValueError('GPUPerfAPI release requires four numeric version fields')
    major, minor, update, build = map(int, parts)
    return [major, minor, build, update]


def pin(root):
    registry = (root / 'cmake/Dependencies/DependencyRegistry.cmake').read_text()
    match = re.search(r'octaryn_register_dependency\(gpu_perf_api\s+(.*?)\)', registry, re.S)
    if not match:
        raise RuntimeError('Missing central GPUPerfAPI pin')
    fields = dict(re.findall(r'(TAG|SOURCE_SUBDIR|URL_HASH|URL)\s+([^\s)]+)', match[1]))
    if set(fields) != {'TAG', 'SOURCE_SUBDIR', 'URL', 'URL_HASH'}:
        raise RuntimeError('Incomplete GPUPerfAPI pin')
    algorithm, digest = fields['URL_HASH'].split('=', 1)
    if algorithm != 'SHA256' or not re.fullmatch('[0-9a-f]{64}', digest):
        raise RuntimeError('GPUPerfAPI requires a SHA256 pin')
    return fields | {'sha256': digest}


def acquire(root):
    data = pin(root)
    destination = root / 'build/dependencies/tools/gpu-perf-api' / data['TAG']
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / data['URL'].rsplit('/', 1)[-1]
    if not archive.exists():
        temporary = archive.with_suffix('.download')
        with urllib.request.urlopen(data['URL'], timeout=60) as response, temporary.open('wb') as output:
            while block := response.read(1024 * 1024):
                output.write(block)
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != data['sha256']:
            raise RuntimeError('GPUPerfAPI downloaded archive checksum mismatch')
        temporary.replace(archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != data['sha256']:
        raise RuntimeError('GPUPerfAPI cached archive checksum mismatch; refusing replacement')
    package = destination / 'package'
    files = {}
    with zipfile.ZipFile(archive) as source:
        for entry in source.infolist():
            relative = PurePosixPath(entry.filename)
            if relative.is_absolute() or '..' in relative.parts or ':' in entry.filename or '\\' in entry.filename:
                raise RuntimeError('Unsafe SDK archive path')
            if entry.is_dir():
                continue
            payload = source.read(entry)
            target = package.joinpath(*relative.parts)
            if target.exists():
                if target.read_bytes() != payload:
                    raise RuntimeError(f'GPUPerfAPI extracted file differs from pinned archive: {relative}')
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(payload)
            files[str(relative)] = hashlib.sha256(payload).hexdigest()
    sdk = package / data['SOURCE_SUBDIR']
    for relative in ('bin/GPUPerfAPIDX12-x64.dll', 'include/gpu_performance_api/gpu_perf_api.h'):
        if not (sdk / relative).is_file():
            raise RuntimeError(f'Missing matched SDK component: {relative}')
    receipt = data | {'sdk': str(sdk.resolve()), 'files': files, 'loaded': False, 'machine_install': False}
    (destination / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[3])
    args = parser.parse_args()
    result = acquire(args.repo.resolve())
    print(f"gpu_perf_api_acquisition=verified sdk={result['sdk']} archive_sha256={result['sha256']}")


if __name__ == '__main__':
    main()
