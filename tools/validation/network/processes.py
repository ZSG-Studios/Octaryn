"""Lifecycle operations restricted to processes spawned by the qualification run."""
import subprocess
import time

from udp_impairment import bind_loopback


def available_port(requested):
    with bind_loopback(requested) as reservation:
        return reservation.getsockname()[1]


def launch(command, cwd, env, output):
    return subprocess.Popen(command, cwd=cwd, env=env, stdout=output,
                            stderr=subprocess.STDOUT,
                            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))


def wait_ready(child, log, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if child.poll() is not None:
            raise RuntimeError(f'server exited before readiness: {child.returncode}; {log}')
        if 'server_remote_listening' in log.read_text(encoding='utf-8', errors='replace'):
            return
        time.sleep(.05)
    raise TimeoutError(f'server readiness timed out: {log}')


def stop_child(child, marker, seconds):
    result = {'pid': child.pid, 'marker': str(marker), 'terminate': False, 'kill': False}
    # Only the fresh, run-owned directory is passed here, never a live session path.
    marker.parent.mkdir(parents=True, exist_ok=True)
    marker.write_text('stop\n', encoding='utf-8')
    try:
        child.wait(timeout=seconds)
    except subprocess.TimeoutExpired:
        result['terminate'] = True
        child.terminate()
        try:
            child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            result['kill'] = True
            child.kill()
            child.wait(timeout=5)
    result['exit_code'] = child.returncode
    return result
