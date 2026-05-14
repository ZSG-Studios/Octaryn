#!/usr/bin/env python3
import argparse
from pathlib import Path
import re
import statistics


KEY_VALUE = re.compile(r"([A-Za-z0-9_]+)=([-A-Za-z0-9_./]+)")


def fields(line):
    return {key: value for key, value in KEY_VALUE.findall(line)}


def number(values, key, default=0.0):
    try:
        return float(values.get(key, default))
    except ValueError:
        return default


def percentile(values, ratio):
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round((len(ordered) - 1) * ratio))))
    return ordered[index]


def top_frames(frames, key, count):
    return sorted(frames, key=lambda frame: frame.get(key, 0.0), reverse=True)[:count]


def parse_log(path):
    frames = []
    phases = []
    batches = []
    draws = []
    schedules = []
    for line in path.read_text(errors="replace").splitlines():
        if line.startswith("live_frame_profile "):
            data = fields(line)
            frames.append({key: number(data, key) for key in data})
        elif line.startswith("live_frame_phase_profile "):
            data = fields(line)
            phases.append({key: number(data, key) for key in data})
        elif line.startswith("live_server_stream_mesh_batch "):
            data = fields(line)
            batches.append({key: number(data, key) for key in data})
        elif line.startswith("live_native_schedule_runtime "):
            data = fields(line)
            schedules.append({key: number(data, key) for key in data})
        elif line.startswith("live_world_mesh_draw "):
            data = fields(line)
            draws.append({key: number(data, key) for key in data})
    return {
        "frames": frames,
        "phases": phases,
        "batches": batches,
        "schedules": schedules,
        "draws": draws,
    }


def mean(values):
    return statistics.fmean(values) if values else 0.0


def summarize(path, top_count):
    data = parse_log(path)
    frames = data["frames"]
    phases = data["phases"]
    batches = data["batches"]
    schedules = data["schedules"]
    draws = data["draws"]
    frame_ms = [frame["frame_ms"] for frame in frames if "frame_ms" in frame]
    actual_fps = [frame["actual_fps"] for frame in frames if "actual_fps" in frame]
    render_ms = [frame["render_ms"] for frame in frames if "render_ms" in frame]
    sim_ms = [frame["sim_ms"] for frame in frames if "sim_ms" in frame]
    world_ms = [frame["world_ms"] for frame in frames if "world_ms" in frame]
    upload_ms = [batch["upload_ms"] for batch in batches if "upload_ms" in batch]
    build_ms = [batch["build_ms"] for batch in batches if "build_ms" in batch]
    elapsed_ms = [item["elapsed_ms"] for item in schedules if "elapsed_ms" in item]
    visible_chunks = [draw["chunks"] for draw in draws if "chunks" in draw]
    faces = [draw["opaque_faces"] for draw in draws if "opaque_faces" in draw]

    print(f"\n{path}")
    print(
        "  frames={frames} avg={avg:.3f} p95={p95:.3f} p99={p99:.3f} "
        "max={max_ms:.3f} actual_fps_min={fps_min:.1f} "
        "actual_fps_avg={fps_avg:.1f} render_avg={render:.3f} sim_p99={sim:.3f} "
        "world_p99={world:.3f}".format(
            frames=len(frames),
            avg=mean(frame_ms),
            p95=percentile(frame_ms, 0.95),
            p99=percentile(frame_ms, 0.99),
            max_ms=max(frame_ms, default=0.0),
            fps_min=min(actual_fps, default=0.0),
            fps_avg=mean(actual_fps),
            render=mean(render_ms),
            sim=percentile(sim_ms, 0.99),
            world=percentile(world_ms, 0.99),
        )
    )
    print(
        "  mesh_batches={batches} build_avg={build_avg:.3f} build_p99={build_p99:.3f} "
        "upload_avg={upload_avg:.3f} upload_p99={upload_p99:.3f} "
        "schedule_p99={schedule_p99:.3f}".format(
            batches=len(batches),
            build_avg=mean(build_ms),
            build_p99=percentile(build_ms, 0.99),
            upload_avg=mean(upload_ms),
            upload_p99=percentile(upload_ms, 0.99),
            schedule_p99=percentile(elapsed_ms, 0.99),
        )
    )
    print(
        "  draw_chunks_avg={chunks:.1f} draw_chunks_max={chunks_max:.0f} "
        "faces_avg={faces:.0f} faces_max={faces_max:.0f}".format(
            chunks=mean(visible_chunks),
            chunks_max=max(visible_chunks, default=0.0),
            faces=mean(faces),
            faces_max=max(faces, default=0.0),
        )
    )
    if phases:
        phase_keys = [
            "controller_ms",
            "terrain_align_ms",
            "raycast_ms",
            "intent_ms",
            "poll_stream_ms",
            "host_tick_ms",
            "mesh_update_ms",
            "presentation_ms",
        ]
        print("  phase_profile_samples={}".format(len(phases)))
        for key in phase_keys:
            values = [phase[key] for phase in phases if key in phase]
            print(
                "    {key}: avg={avg:.3f} p99={p99:.3f} max={max_ms:.3f}".format(
                    key=key,
                    avg=mean(values),
                    p99=percentile(values, 0.99),
                    max_ms=max(values, default=0.0),
                )
            )
    for frame in top_frames(frames, "frame_ms", top_count):
        print(
            "  slow_frame frame={frame:.0f} total={total:.3f} sim={sim:.3f} "
            "actual_fps={actual_fps:.1f} world={world:.3f} render={render:.3f} other={other:.3f} "
            "submit={submit:.3f}".format(
                frame=frame.get("frame", 0.0),
                total=frame.get("frame_ms", 0.0),
                sim=frame.get("sim_ms", 0.0),
                actual_fps=frame.get("actual_fps", 0.0),
                world=frame.get("world_ms", 0.0),
                render=frame.get("render_ms", 0.0),
                other=frame.get("other_ms", 0.0),
                submit=frame.get("submit_ms", 0.0),
            )
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("logs", nargs="+", type=Path)
    parser.add_argument("--top", type=int, default=5)
    args = parser.parse_args()
    for path in args.logs:
        summarize(path, args.top)


if __name__ == "__main__":
    main()
