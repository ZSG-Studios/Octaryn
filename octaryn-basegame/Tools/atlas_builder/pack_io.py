import hashlib
import io
import urllib.request

from PIL import Image

from .sources import DEFAULT_PACK_FILE_NAME


def download_pack(url, cache_dir):
    cache_dir.mkdir(parents=True, exist_ok=True)
    destination = cache_dir / DEFAULT_PACK_FILE_NAME
    if destination.exists() and destination.stat().st_size > 0:
        return destination

    temporary = destination.with_suffix(destination.suffix + ".part")
    request = urllib.request.Request(url, headers={"User-Agent": "octaryn-basegame-atlas-pipeline/1.0"})
    with urllib.request.urlopen(request, timeout=120) as response, temporary.open("wb") as file:
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            file.write(chunk)
    temporary.replace(destination)
    return destination


def verify_sha256(path, expected):
    if not expected:
        return
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    actual = digest.hexdigest()
    if actual.lower() != expected.lower():
        raise RuntimeError(f"SHA256 mismatch for {path}: expected {expected}, got {actual}")


def zip_read_texture(zip_file, path):
    try:
        return zip_file.read(path)
    except KeyError:
        suffix = "/" + path
        for candidate in zip_file.namelist():
            if candidate.endswith(suffix):
                return zip_file.read(candidate)
    raise KeyError(path)


def zip_read_optional(zip_file, path):
    try:
        return zip_read_texture(zip_file, path)
    except KeyError:
        return None


def find_texture_entry(zip_file, names, suffix=""):
    if zip_file is None:
        return None, ""

    prefixes = (
        "assets/minecraft/textures/block/",
        "assets/minecraft/textures/blocks/",
    )
    for name in names:
        for prefix in prefixes:
            path = prefix + name.replace(".png", f"{suffix}.png")
            try:
                data = zip_read_texture(zip_file, path)
                return Image.open(io.BytesIO(data)).convert("RGBA"), path
            except KeyError:
                continue
    return None, ""


def find_texture(zip_file, names, suffix=""):
    image, _ = find_texture_entry(zip_file, names, suffix)
    return image
