#!/usr/bin/env python3
"""Shared evidence helpers for packaged-client validation cases."""
import hashlib
import json
import subprocess
import os
import platform


def record_build(bundle, case):
    paths = [bundle / 'Octaryn.Client.exe']
    paths += sorted(bundle.glob('*.dll'))
    paths += sorted(bundle.glob('*.deps.json'))
    paths += sorted(bundle.glob('*.runtimeconfig.json'))
    paths += sorted((bundle / 'Client/Shaders').rglob('*.slang'))
    paths += sorted((bundle / 'Assets/Items').glob('*.glb'))
    paths += sorted((bundle / 'Data/Items').glob('*.json'))
    paths += sorted((bundle / 'Assets/Audio').glob('*.json'))
    paths += sorted((bundle / 'server').rglob('*.dll'))
    paths += sorted((bundle / 'server').rglob('*.exe'))
    manifest = {str(path.relative_to(bundle)): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in paths if path.is_file()}
    host = dict(system=platform.platform(), machine=platform.machine(), cpu=platform.processor())
    if os.name == 'nt':
        try:
            result = subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command',
                'Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,PNPDeviceID | ConvertTo-Json -Compress'],
                check=True, capture_output=True, text=True, timeout=15,
                creationflags=subprocess.CREATE_NO_WINDOW)
            host['graphics_adapters'] = json.loads(result.stdout)
        except (OSError, subprocess.SubprocessError, ValueError) as error:
            host['graphics_adapters_unavailable'] = type(error).__name__
    for name in ('settings.json', 'lighting.json'):
        path = case / name
        if path.exists():
            manifest['case/' + name] = hashlib.sha256(path.read_bytes()).hexdigest()
    (case / 'client-build.json').write_text(json.dumps(dict(schema_version=3, host=host, bundle=str(bundle), sha256=manifest),
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
