#!/usr/bin/env python3
"""Shared evidence helpers for packaged-client validation cases."""
import hashlib
import json
import subprocess


def record_build(bundle, case):
    paths = [bundle / name for name in ('Octaryn.Client.exe', 'slang-compiler.dll',
                                       'slang.dll', 'dxcompiler.dll', 'dxil.dll')]
    paths += sorted((bundle / 'Client/Shaders').rglob('*.slang'))
    manifest = {str(path.relative_to(bundle)): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in paths if path.is_file()}
    (case / 'client-build.json').write_text(json.dumps(dict(bundle=str(bundle), sha256=manifest),
                                                     indent=2), encoding='utf-8')


def stop_case(process, case):
    runtime = case / 'world/runtime'
    runtime.mkdir(exist_ok=True)
    (runtime / 'shutdown.request').write_text('stop\n', encoding='utf-8')
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
