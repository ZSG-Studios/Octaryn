"""CPU-only WorldFrames method tests with generated, explicitly non-RHI doubles."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/build"))
from vsenv import find_vs_root, import_vs_environment, prepend_tool_dirs

# This is deliberately not an ABI-compatible replacement for slang-rhi. It is
# emitted only in the ignored test output directory, never as a dependency shim.
DOUBLES = r"""
#pragma once
#include <cstdint>
#define SLANG_OK 0
#define SLANG_FAIL (-1)
#define SLANG_E_TIME_OUT (-2)
#define SLANG_SUCCEEDED(value) ((value)>=0)
#define SLANG_FAILED(value) ((value)<0)
namespace rhi {
struct IFence {virtual ~IFence()=default;virtual int getCurrentValue(std::uint64_t*)=0;};
struct ICommandBuffer {};
struct FenceDesc {const char* label{};};
struct SubmitDesc {
    ICommandBuffer** commandBuffers{};unsigned commandBufferCount{};
    IFence** signalFences{};const std::uint64_t* signalFenceValues{};unsigned signalFenceCount{};
};
struct ICommandQueue {virtual ~ICommandQueue()=default;virtual int submit(const SubmitDesc&)=0;};
struct IDevice {
    virtual ~IDevice()=default;
    virtual int createFence(const FenceDesc&,IFence**)=0;
    virtual int waitForFences(unsigned,IFence**,const std::uint64_t*,bool,std::uint64_t)=0;
};
}
namespace Slang {
// Borrowed pointer only. COM lifetime/refcount behavior is not under test.
template<class T> class ComPtr {
    T* pointer_{};
public:
    ComPtr& operator=(T* value) {pointer_=value;return *this;}
    operator T*() const {return pointer_;}
    T* operator->() const {return pointer_;}
    T** writeRef() {pointer_=nullptr;return &pointer_;}
};
}
"""


def inspect_mesh_poll(source):
    start = source.index("bool WorldMeshJob::poll(")
    end = source.index("std::uint64_t WorldMeshJob::gpu_bytes()", start)
    body = re.sub(r"//[^\n]*", "", source[start:end])
    compact = re.sub(r"\s+", "", body)
    assert "waitForFences" not in body, "mesh poll must not register/wait on the shared event"
    fragments = [
        "autoresult=s.fence->getCurrentValue(&completed);",
        "if(SLANG_SUCCEEDED(result))result=completed==UINT64_MAX?SLANG_FAIL:"
        "completed<s.signal?SLANG_E_TIME_OUT:SLANG_OK;",
        "if(result==SLANG_E_TIME_OUT){++s.resources.poll_timeouts;returntrue;}",
        "if(!world_rhi_ok(result))returnfalse;",
        "s.observed=s.signal;",
        "if(s.emitting){output=std::move(s.gpu);s.finished=true;complete=true;returntrue;}",
    ]
    positions = [compact.index(fragment) for fragment in fragments]
    assert positions == sorted(positions), "mesh completion guard must precede publication"
    return dict(source_inspection_only=True, value_poll=True, event_wait_absent=True,
                completion_guard_precedes_publication=True)


def run():
    output = ROOT / "build/release-windows/tools/fence-test"
    output.mkdir(parents=True, exist_ok=True)
    header = ROOT / "octaryn-client/Source/Rendering/RenderBackend/WorldFrames.h"
    source = header.read_text(encoding="utf-8")
    generated = source
    for dependency in ("slang-rhi.h", "slang-com-ptr.h"):
        include = f"#include <{dependency}>"
        assert generated.count(include) == 1, f"unexpected production include: {dependency}"
        generated = generated.replace(include, "// External API supplied by generated test doubles.")
    # Only external includes are replaced. All production method bodies remain exact.
    (output / "WorldFramesUnderTest.h").write_text(generated, encoding="utf-8")
    (output / "FenceTestDoubles.h").write_text(DOUBLES, encoding="utf-8")
    mesh_path = header.with_name("WorldMeshJob.cpp")
    mesh_source = mesh_path.read_text(encoding="utf-8")
    mesh = inspect_mesh_poll(mesh_source)
    vs = find_vs_root()
    import_vs_environment(vs, "x64")
    prepend_tool_dirs(ROOT, vs, "x64")
    subprocess.run(["clang-cl", "/nologo", "/O2", "/EHsc", "/std:c++20", "/W4", "/WX",
                    "/I" + str(output), str(ROOT / "tools/validation/world_frames_test.cpp"),
                    "/Fe:world_frames_test.exe"], cwd=output, check=True)
    result = subprocess.run([str(output / "world_frames_test.exe")], cwd=ROOT,
                            text=True, capture_output=True)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    result.check_returncode()
    report = dict(status="passed", production_method_bodies=True, generated_api_doubles=True,
                  real_rhi_integration=False, gpu_runtime=False, mesh_poll=mesh,
                  header_sha256=hashlib.sha256(source.encode()).hexdigest(),
                  mesh_source_sha256=hashlib.sha256(mesh_source.encode()).hexdigest(),
                  cpu_result=result.stdout.strip())
    log = ROOT / "logs/tools/fence-test.json"
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    run()
