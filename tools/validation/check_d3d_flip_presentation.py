"""Build the production DXGI presentation policy fixture; no GPU or CTest."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/build'))
import vsenv


def replay_patch(pending_bootstrap=False):
    sdk = ROOT / 'build/dependencies/slang-rhi'
    patches = ('slang-rhi-d3d-flip-presentation.patch','slang-rhi-d3d12-surface-retirement.patch',
               'slang-rhi-d3d12-surface-acquisition.patch','slang-rhi-d3d12-surface-completion.patch')
    groups = (('src/d3d/d3d-presentation.h','src/d3d/d3d-surface.h'),
              ('src/d3d/d3d-surface.h','src/d3d12/d3d12-surface-retirement.h','src/d3d12/d3d12-surface.cpp','src/d3d12/d3d12-surface.h'),
              ('src/d3d12/d3d12-surface-acquisition.h','src/d3d12/d3d12-surface.cpp'),
              ('src/d3d/d3d-surface.h','src/d3d12/d3d12-surface.h','src/d3d12/d3d12-surface.cpp',
               'src/d3d12/d3d12-surface-completion.h','src/d3d12/d3d12-surface-retirement.h'))
    paths = tuple(sorted(set(path for group in groups for path in group)))
    def git(directory, *args):
        return subprocess.run(['git', '-C', str(directory), *args], check=True,
                              capture_output=True, timeout=30).stdout.replace(b'\r\n', b'\n')
    with tempfile.TemporaryDirectory(prefix='d3d-presentation-policy-') as folder:
        scratch = Path(folder)
        git(scratch, 'init', '-q')
        git(scratch, 'config', 'core.autocrlf', 'false')
        for name in ('src/d3d/d3d-surface.h','src/d3d12/d3d12-surface.cpp','src/d3d12/d3d12-surface.h'):
            source=scratch/name;source.parent.mkdir(parents=True,exist_ok=True)
            source.write_bytes(git(sdk,'show','HEAD:'+name))
        git(scratch, 'add', '--all')
        git(scratch, '-c', 'user.name=Policy Fixture', '-c', 'user.email=policy@localhost',
            'commit', '-qm', 'Pinned presentation baseline', '--no-gpg-sign')
        base=git(scratch,'rev-parse','HEAD').decode().strip()
        for patch_name,group in zip(patches,groups):
            patch=ROOT/'tools/build/patches'/patch_name
            git(scratch,'apply','--check',str(patch));git(scratch,'apply',str(patch));git(scratch,'add','--all')
            actual=git(scratch,'diff','--binary','--no-ext-diff','HEAD','--',*group).rstrip(b'\n')
            if actual!=patch.read_bytes().replace(b'\r\n',b'\n').rstrip(b'\n'):
                raise RuntimeError('Canonical pinned patch replay differs: '+patch_name)
            git(scratch,'-c','user.name=Policy Fixture','-c','user.email=policy@localhost','commit','-qm',patch_name,'--no-gpg-sign')
            if patch_name==patches[2]:
                prior=git(scratch,'diff','--binary','--no-ext-diff',base,'--',*paths).rstrip(b'\n')
        expected=git(scratch,'diff','--binary','--no-ext-diff',base,'--',*paths).rstrip(b'\n')
        actual=git(sdk,'diff','--binary','--no-ext-diff','HEAD','--',*paths).rstrip(b'\n')
        if actual!=(prior if pending_bootstrap else expected):
            raise RuntimeError('Active SDK differs from exact registered presentation patch prefix')
        headers={name:(scratch/'src/d3d12'/name).read_bytes() for name in (
            'd3d12-surface-acquisition.h','d3d12-surface-retirement.h','d3d12-surface-completion.h')}
        surface=(scratch/'src/d3d12/d3d12-surface.cpp').read_text()
    print(f'd3d_presentation_patch pinned_replay=1 retirement_replay=1 acquisition_replay=1 completion_replay=1 active_exact={int(not pending_bootstrap)} active_unchanged=1')
    return headers,surface


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/windows-x64/tools/d3d-presentation')
    parser.add_argument('--pending-bootstrap',action='store_true',help='Validate registered helper before canonical dependency rebuild; active SDK must match prior three patches')
    args = parser.parse_args()
    output = args.out.resolve()
    if output.exists():raise RuntimeError('Preserve previous receipt; use a new --out directory')
    output.mkdir(parents=True)
    headers,surface=replay_patch(args.pending_bootstrap)
    for name,header in headers.items():(output/name).write_bytes(header)
    vs = vsenv.find_vs_root()
    vsenv.import_vs_environment(vs, 'x64')
    vsenv.prepend_tool_dirs(ROOT, vs, 'x64')
    source = ROOT / 'build/dependencies/slang-rhi/src/d3d/d3d-surface.h'
    text = source.read_text(encoding='utf-8')
    required = ('DXGI_FEATURE_PRESENT_ALLOW_TEARING', 'DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING',
                'd3d_allow_tearing(m_swapEffect, m_config.vsync, tearingSupported)',
                'd3d_present_flags(m_allowTearing, m_config.vsync, fullscreen == TRUE)',
                'GetFullscreenState(&fullscreen, nullptr)')
    if any(token not in text for token in required) or 'swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT' in text:
        raise RuntimeError('Production surface does not admit the checked presentation policy')
    for token in ('d3d_retire_image(fenceValue','m_fence->SetEventOnCompletion(value, m_frameEvents[image])',
                  'SLANG_D3D_RETURN_ON_FAIL_REPORT(m_queue->Signal(m_fence, value), m_device)',
                  'if (!m_configured || m_acquiredImage >= 0)','m_configured = false;'):
        if token not in surface:raise RuntimeError('Production surface does not wire checked retirement contract')
    for token in ('d3d_wait_image_completion(target,', 'WaitForSingleObject(m_frameEvents[index], timeout)',
                  'm_fence->GetCompletedValue()', 'ResetEvent(m_frameEvents[index])',
                  'm_imageRetirements[image] = value;',
                  'for (auto event : m_frameEvents)'):
        if token not in surface:raise RuntimeError('Registered surface does not wire checked bounded acquisition')
    if 'WaitForSingleObject(m_frameEvents[index], INFINITE)' in surface:
        raise RuntimeError('Image acquisition still waits indefinitely')
    executable = output / 'D3DPresentationProbe.exe'
    command = [vsenv.resolve_tool('clang-cl'), '/nologo', '/std:c++20', '/EHsc', '/MD', '/O2', '/W4', '/WX',
               str(ROOT / 'tools/Source/D3DPresentationProbe/main.cpp'), '/Fe' + str(executable),
               '/I'+str(output),'/Fo' + str(output / 'main.obj')]
    started = time.perf_counter()
    receipt = {'kind': 'authored-presentation-policy', 'gpu': False, 'command': command, 'status': 'failed',
               'activeSdkMatchesAllRegisteredPatches':not args.pending_bootstrap,'pendingCanonicalBootstrap':args.pending_bootstrap}
    receipt['pinnedSdkCommit']=subprocess.check_output(['git','-C',str(ROOT/'build/dependencies/slang-rhi'),
                                                      'rev-parse','HEAD'],text=True).strip()
    receipt['visiblePerformanceQualified']=False
    receipt['sourceSha256']={str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest() for path in [
        ROOT/'tools/Source/D3DPresentationProbe/main.cpp',Path(__file__).resolve(),ROOT/'tools/build/slang-rhi.py',
        *(ROOT/'tools/build/patches'/name for name in ('slang-rhi-d3d-flip-presentation.patch',
          'slang-rhi-d3d12-surface-retirement.patch','slang-rhi-d3d12-surface-acquisition.patch',
          'slang-rhi-d3d12-surface-completion.patch'))]}
    with (output / 'checks.log').open('w', encoding='utf-8') as log:
        compiled = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=120)
        receipt['compileExitCode'] = compiled.returncode
        if compiled.returncode == 0:
            checked = subprocess.run([str(executable)], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=20)
            receipt['checksExitCode'] = checked.returncode
            if checked.returncode == 0:
                receipt['status'] = 'passed'
    receipt['seconds'] = time.perf_counter() - started
    (output / 'result.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print((output / 'checks.log').read_text(encoding='utf-8'))
    print(json.dumps({'status': receipt['status'], 'receipt': str(output / 'result.json')}))
    return 0 if receipt['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
