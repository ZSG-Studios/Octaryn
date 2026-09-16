"""Install a completed sibling staging directory without deleting a live bundle."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import stat
import sys
import uuid


def bundle_paths(bundle):
    bundle = Path(os.path.abspath(bundle))
    parent = bundle.parent.resolve(strict=True)
    if bundle.parent != parent or bundle.is_symlink():
        raise ValueError(f"Bundle path must not traverse links: {bundle}")
    stage = parent / (bundle.name + ".staging")
    retired = parent / (bundle.name + ".retired")
    for path in (bundle, stage, retired):
        if path.resolve() != path or path.is_symlink():
            raise ValueError(f"Bundle directory must not be redirected: {path}")
        if path.exists() and not path.is_dir():
            raise ValueError(f"Expected directory: {path}")
    return bundle, stage, retired


def prepare(bundle):
    _, stage, _ = bundle_paths(bundle)
    # Only the verified sibling staging directory may be removed, never runtime.
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir()
    return stage


def identical_trees(first, second):
    def entries(root):
        return {Path("."): root, **{path.relative_to(root): path for path in root.rglob("*")}}

    def digest(path):
        result = hashlib.sha256()
        with path.open("rb") as stream:
            for data in iter(lambda: stream.read(1024 * 1024), b""):
                result.update(data)
        return result.digest()

    left, right = entries(first), entries(second)
    if left.keys() != right.keys():
        return False
    for name, path in left.items():
        other = right[name]
        if path.is_symlink() or other.is_symlink():
            return False
        if path.is_dir() != other.is_dir():
            return False
        if stat.S_IMODE(path.stat().st_mode) != stat.S_IMODE(other.stat().st_mode):
            return False
        if path.is_file() and (path.stat().st_size != other.stat().st_size or digest(path) != digest(other)):
            return False
    return True


def install(bundle):
    bundle, stage, retired_root = bundle_paths(bundle)
    if not stage.is_dir() or not any(stage.iterdir()):
        raise ValueError(f"Completed staging directory is missing or empty: {stage}")
    if bundle.exists() and identical_trees(stage, bundle):
        return "unchanged"
    retired = None
    if bundle.exists():
        retired_root.mkdir(exist_ok=True)
        retired = retired_root / uuid.uuid4().hex
        # A Windows sharing violation fails this rename before any file changes.
        os.rename(bundle, retired)
    try:
        os.rename(stage, bundle)
    except OSError as install_error:
        if retired is not None:
            try:
                os.rename(retired, bundle)
            except OSError as rollback_error:
                raise RuntimeError(
                    f"Install and rollback failed; previous bundle is intact at {retired}; "
                    f"staging remains at {stage}; rollback error: {rollback_error}"
                ) from install_error
        raise
    # Keep the previous directory intact for recovery; never delete old runtimes.
    return retired


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "install"))
    parser.add_argument("--bundle", required=True, type=Path)
    args = parser.parse_args()
    try:
        result = prepare(args.bundle) if args.action == "prepare" else install(args.bundle)
    except (OSError, ValueError, RuntimeError) as error:
        print(f"bundle_{args.action}=failed: {error}", file=sys.stderr)
        return 1
    print(f"bundle_{args.action}=passed path={args.bundle} retained={result}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
