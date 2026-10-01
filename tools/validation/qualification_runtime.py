"""Copy immutable runtime inputs so long qualification does not lock build outputs."""
import hashlib
import json
from pathlib import Path
import shutil


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def stage_runtime(build, evidence):
    records = {}
    roots = {}
    for owner in ("client", "server"):
        source = build / owner / "bundle"
        target = build / owner / "qualification" / evidence.name
        target.mkdir(parents=True, exist_ok=False)
        roots[owner] = target
        files = sorted(source.rglob("*")) if owner == "server" else sorted(source.iterdir())
        for path in files:
            if not path.is_file() or (owner == "client" and path.suffix not in (".dll", ".exe", ".json")):
                continue
            relative = path.relative_to(source)
            destination = target / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            before = digest(path)
            shutil.copy2(path, destination)
            after = digest(destination)
            if before != after or before != digest(path):
                raise RuntimeError("Build input changed while staging: " + str(path))
            records[owner + "/" + relative.as_posix()] = dict(source=str(path), staged=str(destination), sha256=after)
    result = dict(schema_version=1, copy_mode="independent files; no hardlinks", files=records)
    (evidence / "runtime-snapshot.json").write_text(json.dumps(result, indent=2))
    return roots["client"], roots["server"]
