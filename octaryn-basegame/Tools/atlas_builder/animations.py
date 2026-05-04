import json

from PIL import Image

from .pack_io import find_texture_entry, zip_read_optional
from .sources import ATLAS_SOURCES
from .tiles import resize_tile


def frame_tile(image, frame_index):
    frame_size = min(image.width, image.height)
    columns = max(1, image.width // frame_size)
    x = (frame_index % columns) * frame_size
    y = (frame_index // columns) * frame_size
    return image.crop((x, y, x + frame_size, y + frame_size))


def read_animation(zip_file, texture_path, image):
    metadata = zip_read_optional(zip_file, texture_path + ".mcmeta")
    frame_size = min(image.width, image.height)
    if metadata is None or frame_size <= 0 or image.width % frame_size != 0 or image.height % frame_size != 0:
        return None

    total_source_frames = (image.width // frame_size) * (image.height // frame_size)
    if total_source_frames <= 1:
        return None

    data = json.loads(metadata.decode("utf-8"))
    animation = data.get("animation", {})
    default_ticks = int(animation.get("frametime", 1))
    entries = animation.get("frames")
    frames = []
    frame_ticks = []
    if entries is None:
        frames = list(range(total_source_frames))
        frame_ticks = [default_ticks] * total_source_frames
    else:
        for entry in entries:
            if isinstance(entry, int):
                frames.append(entry)
                frame_ticks.append(default_ticks)
            else:
                frames.append(int(entry["index"]))
                frame_ticks.append(int(entry.get("time", default_ticks)))

    valid_frames = []
    valid_ticks = []
    for frame, ticks in zip(frames, frame_ticks):
        if 0 <= frame < total_source_frames and ticks > 0:
            valid_frames.append(frame)
            valid_ticks.append(ticks)
    if len(valid_frames) <= 1:
        return None
    return valid_frames, valid_ticks


def collect_animations(zip_file, tile_size, layer_count, warnings):
    if zip_file is None:
        return [], []

    animations = []
    frames = []
    for index in range(layer_count):
        source = ATLAS_SOURCES.get(index)
        if source is None:
            continue
        image, texture_path = find_texture_entry(zip_file, source[1])
        if image is None:
            continue
        animation = read_animation(zip_file, texture_path, image)
        if animation is None:
            continue
        source_frames, frame_ticks = animation
        first_frame = len(frames)
        for source_frame in source_frames:
            frames.append(resize_tile(frame_tile(image, source_frame), tile_size))
        animations.append(
            {
                "layer": index,
                "name": source[0],
                "first_frame": first_frame,
                "frame_count": len(source_frames),
                "frame_ticks": frame_ticks,
            }
        )
        warnings.append(f"animation layer {index}: {source[0]} frames={len(source_frames)}")
    return animations, frames


def save_animation_atlas(path, frames, tile_size):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not frames:
        Image.new("RGBA", (tile_size, tile_size), (0, 0, 0, 0)).save(path)
        return
    image = Image.new("RGBA", (tile_size * len(frames), tile_size), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        image.paste(frame, (index * tile_size, 0))
    image.save(path)


def write_animation_manifest(path, animations, frames, tile_size):
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "Octaryn generated atlas animations",
        f"tile_size={tile_size}",
        f"frames={len(frames)}",
        f"animations={len(animations)}",
    ]
    for animation in animations:
        ticks = ",".join(str(value) for value in animation["frame_ticks"])
        lines.append(
            "animation="
            f"{animation['layer']}|{animation['name']}|{animation['first_frame']}|"
            f"{animation['frame_count']}|{ticks}"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
