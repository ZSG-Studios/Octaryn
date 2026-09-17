#!/usr/bin/env python3
"""Qualify a packaged remote jump/block probe against a fresh isolated server."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

from processes import available_port, launch, stop_child, wait_ready
from results import probe_result, proxy_passed
from udp_impairment import UdpImpairment


ROOT = Path(__file__).resolve().parents[3]


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', choices=('jump', 'blocks'), required=True)
    parser.add_argument('--preset', default='release-windows' if os.name == 'nt' else 'release-linux')
    parser.add_argument('--client-bundle', type=Path)
    parser.add_argument('--server-bundle', type=Path)
    parser.add_argument('--probe-executable', type=Path)
    parser.add_argument('--work-dir', type=Path, help='must not exist; fresh run-owned worlds/cache')
    parser.add_argument('--server-port', type=int, default=0, help='0 selects an available ephemeral port')
    parser.add_argument('--proxy-port', type=int, default=0, help='0 binds an ephemeral proxy port when impaired')
    parser.add_argument('--max-takeoff-ms', type=float)
    parser.add_argument('--simulate-min-delay-ms', type=int, default=0)
    parser.add_argument('--simulate-max-delay-ms', type=int, default=0)
    parser.add_argument('--simulate-loss-percent', type=float, default=0)
    parser.add_argument('--simulate-seed', type=int, default=20260917)
    parser.add_argument('--timeout-seconds', type=float, default=180)
    parser.add_argument('--startup-timeout-seconds', type=float, default=60)
    parser.add_argument('--shutdown-timeout-seconds', type=float, default=15)
    args = parser.parse_args(argv)
    if not args.preset or Path(args.preset).name != args.preset or args.preset in ('.', '..'):
        parser.error('preset must be a directory name, not a path')
    if not all(0 <= port <= 65535 for port in (args.server_port, args.proxy_port)):
        parser.error('ports must be between 0 and 65535')
    if args.server_port and args.server_port == args.proxy_port:
        parser.error('server and proxy ports must differ')
    if not 0 <= args.simulate_min_delay_ms <= args.simulate_max_delay_ms <= 2000:
        parser.error('delay must satisfy 0 <= min <= max <= 2000ms')
    if not 0 <= args.simulate_loss_percent <= 25:
        parser.error('loss percentage must be between 0 and 25')
    if args.max_takeoff_ms is not None and (args.probe != 'jump' or not 0 < args.max_takeoff_ms <= 1000):
        parser.error('takeoff budget is jump-only and must be greater than 0 and at most 1000ms')
    if not all(0 < value <= 1800 for value in
               (args.timeout_seconds, args.startup_timeout_seconds, args.shutdown_timeout_seconds)):
        parser.error('timeouts must be greater than 0 and at most 1800 seconds')
    return args


def root_path(value):
    return (ROOT / value).resolve()


def environment(build, client_bundle, server_bundle, marker):
    # Parent sessions may export mailbox/save overrides. None may reach this run.
    env = {key: value for key, value in os.environ.items() if not key.startswith('OCTARYN_')}
    env['OCTARYN_REMOTE_TIMING'] = '1'
    env['OCTARYN_SERVER_SHUTDOWN_REQUEST_PATH'] = str(marker)
    env['OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY'] = '1'
    env['OCTARYN_CLIENT_MANAGED_ASSEMBLY_PATH'] = str(client_bundle / 'Octaryn.Client.dll')
    env['OCTARYN_CLIENT_RUNTIME_CONFIG_PATH'] = str(client_bundle / 'Octaryn.Client.runtimeconfig.json')
    directories = [client_bundle, server_bundle] + [build / owner / 'native/bin'
                                                    for owner in ('client', 'server', 'shared')]
    library_paths = os.pathsep.join(str(path) for path in directories)
    for key in ('PATH', 'LD_LIBRARY_PATH', 'DYLD_LIBRARY_PATH'):
        env[key] = library_paths + (os.pathsep + env[key] if env.get(key) else '')
    return env


def run(args):
    name = f'remote-{args.probe}-{time.strftime("%Y%m%d-%H%M%S")}-{uuid.uuid4().hex[:8]}'
    build = ROOT / 'build' / args.preset
    work = root_path(args.work_dir) if args.work_dir else build / 'tools/validation' / name
    client_bundle = root_path(args.client_bundle) if args.client_bundle else build / 'client/bundle'
    server_bundle = root_path(args.server_bundle) if args.server_bundle else build / 'server/bundle'
    suffix = '.exe' if os.name == 'nt' else ''
    probe_name = 'octaryn_client_jump_probe' if args.probe == 'jump' else 'octaryn_client_block_actions_probe'
    probe = root_path(args.probe_executable) if args.probe_executable else build / 'tools/native/bin' / (probe_name + suffix)
    server_executable = server_bundle / ('Octaryn.Server' + suffix)
    logs = {owner: ROOT / 'logs' / owner / (name + '.log') for owner in ('client', 'server')}
    report_path = ROOT / 'logs/tools' / (name + '.json')
    proxy_path = ROOT / 'logs/tools' / (name + '-proxy.json')
    for path in (*logs.values(), report_path):
        path.parent.mkdir(parents=True, exist_ok=True)
    report = dict(probe=args.probe, work=str(work), logs={k: str(v) for k, v in logs.items()},
                  started_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                  probe_exit_code=None, server_exit_code=None, passed=False, errors=[])
    server = client = proxy = None
    exit_code = 1
    marker = work / 'server.shutdown.request'
    with logs['server'].open('w', encoding='utf-8') as sout, logs['client'].open('w', encoding='utf-8') as cout:
        try:
            for required in (probe, server_executable, client_bundle / 'Octaryn.Client.dll',
                             client_bundle / 'Octaryn.Client.runtimeconfig.json'):
                if not required.is_file():
                    raise FileNotFoundError(f'required packaged artifact missing: {required}')
            # Never reuse an existing world, even an earlier qualification directory.
            work.mkdir(parents=True, exist_ok=False)
            server_port = available_port(args.server_port)
            env = environment(build, client_bundle, server_bundle, marker)
            server_command = [str(server_executable), '--listen', f'127.0.0.1:{server_port}',
                              '--world-dir', str(work / 'server')]
            report['server_command'] = server_command
            server = launch(server_command, server_bundle, env, sout)
            wait_ready(server, logs['server'], args.startup_timeout_seconds)
            if args.simulate_max_delay_ms or args.simulate_loss_percent or args.proxy_port:
                proxy = UdpImpairment(server_port, args.proxy_port, args.simulate_min_delay_ms,
                                      args.simulate_max_delay_ms, args.simulate_loss_percent, args.simulate_seed)
                proxy.start()
            endpoint = f'127.0.0.1:{proxy.address[1] if proxy else server_port}'
            command = [str(probe), str(client_bundle), str(work / 'client'),
                       str(ROOT / 'logs/client' / (name + '-session')), endpoint]
            if args.max_takeoff_ms is not None:
                command += ['--max-takeoff-ms', str(args.max_takeoff_ms)]
            report.update(endpoint=endpoint, probe_command=command)
            print(f'qualification probe={args.probe} endpoint={endpoint} work={work}', flush=True)
            client = launch(command, ROOT, env, cout)
            client.wait(timeout=args.timeout_seconds)
            report['probe_exit_code'] = client.returncode
            result = probe_result(args.probe, logs['client'].read_text(encoding='utf-8', errors='replace'),
                                  client.returncode, args.max_takeoff_ms)
            report['result'] = result
            exit_code = client.returncode if client.returncode else (0 if result['passed'] else 1)
            if not result['passed']:
                report['errors'].append('probe exit/counters did not establish a pass')
        except (TimeoutError, subprocess.TimeoutExpired) as error:
            report['errors'].append(str(error))
            exit_code = 124
        except KeyboardInterrupt:
            report['errors'].append('qualification interrupted')
            exit_code = 130
        except Exception as error:
            report['errors'].append(f'{type(error).__name__}: {error}')
            exit_code = 1
        finally:
            for label, child, shutdown in (
                    ('probe', client, work / 'client/runtime/shutdown.request'), ('server', server, marker)):
                if child is None:
                    continue
                try:
                    cleanup = stop_child(child, shutdown, args.shutdown_timeout_seconds)
                    report[label + '_cleanup'] = cleanup
                    report[label + '_exit_code'] = child.returncode
                    if cleanup['terminate'] or cleanup['kill'] or (label == 'server' and child.returncode):
                        report['errors'].append(f'{label} did not exit cleanly')
                        if exit_code == 0:
                            exit_code = 1
                except Exception as error:
                    report['errors'].append(f'{label} cleanup: {error}')
                    if exit_code == 0:
                        exit_code = 1
            if proxy:
                try:
                    stats = proxy.stop()
                    proxy_path.write_text(json.dumps(stats, indent=2) + '\n', encoding='utf-8')
                    report['proxy'] = stats
                    report['proxy_log'] = str(proxy_path)
                    if not proxy_passed(stats):
                        report['errors'].append('proxy failed or exceeded queue bounds')
                        if exit_code == 0:
                            exit_code = 1
                except Exception as error:
                    report['errors'].append(f'proxy cleanup: {error}')
                    if exit_code == 0:
                        exit_code = 1
    report['passed'] = exit_code == 0 and not report['errors']
    report['runner_exit_code'] = exit_code if exit_code >= 0 else 128 + abs(exit_code)
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'passed': report['passed'], 'exit_code': report['runner_exit_code'],
                      'report': str(report_path), 'errors': report['errors']}), flush=True)
    return report['runner_exit_code']


if __name__ == '__main__':
    sys.exit(run(arguments()))
