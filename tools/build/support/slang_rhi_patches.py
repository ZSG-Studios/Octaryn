"""Verify an ordered patch stack exactly, including overlapping file edits."""
import os
from pathlib import Path
import subprocess
import tempfile


def apply_registered_patches(source, patches):
    source = Path(source).resolve()
    def git(*args, env=None, data=None):
        return subprocess.run(['git', '-C', str(source), *args], input=data,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              env=env, check=True).stdout.replace(b'\r\n', b'\n').rstrip(b'\n')
    payloads = [Path(path).read_bytes().replace(b'\r\n', b'\n') for path in patches]
    actual = git('diff', '--binary', '--no-ext-diff', 'HEAD')
    with tempfile.TemporaryDirectory(prefix='rhi-patch-index-') as temp:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(temp) / 'index'))
        git('read-tree', 'HEAD', env=env)
        matched = 0 if not actual else None
        expected = b''
        for i, payload in enumerate(payloads, 1):
            if not payload.startswith(b'diff --git '):
                raise ValueError(f'Invalid registered patch: {patches[i-1]}')
            git('apply', '--cached', '--ignore-space-change', '-', env=env, data=payload)
            expected = git('diff', '--cached', '--binary', '--no-ext-diff', 'HEAD', env=env)
            if actual == expected:
                matched = i
        if matched is None:
            raise ValueError('Dependency edits differ from the exact registered patch stack')
        for payload in payloads[matched:]:
            git('apply', '--check', '--ignore-space-change', '-', data=payload)
            git('apply', '--ignore-space-change', '-', data=payload)
        # Add only the registered paths, including generated new files; unrelated untracked files stay untouched.
        paths = git('diff', '--cached', '--name-only', 'HEAD', env=env).decode().splitlines()
        if paths:
            git('add', '--', *paths)
        if git('diff', '--binary', '--no-ext-diff', 'HEAD') != expected:
            raise ValueError('Dependency patch application did not match the exact registered stack')
