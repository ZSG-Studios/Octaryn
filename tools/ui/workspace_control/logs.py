from __future__ import annotations

import html
import json
from dataclasses import dataclass
from pathlib import Path

from .paths import SETTINGS_PATH


@dataclass
class ToolSettings:
    launch_tracy_with_probe: bool = True
    capture_tracy_with_probe: bool = False
    tracy_capture_seconds: int = 10


@dataclass
class LogEntry:
    level: str
    text: str


def now_iso() -> str:
    from datetime import datetime, timezone

    return datetime.now(timezone.utc).isoformat()


def classify_line(line: str) -> str | None:
    lowered = line.lower()
    if "[error]" in lowered or " error" in lowered or lowered.startswith("error"):
        return "error"
    if "[debug]" in lowered or " debug" in lowered or lowered.startswith("debug"):
        return "debug"
    if "[warn" in lowered or " warning" in lowered or lowered.startswith("warning"):
        return "warning"
    return None


def append_text(path: Path, message: str) -> None:
    with path.open("a", encoding="utf-8") as handle:
        handle.write(f"[{now_iso()}] {message}\n")


def load_tool_settings() -> ToolSettings:
    if not SETTINGS_PATH.is_file():
        return ToolSettings()
    try:
        raw = json.loads(SETTINGS_PATH.read_text(encoding="utf-8"))
        return ToolSettings(
            launch_tracy_with_probe=bool(raw.get("launch_tracy_with_probe", True)),
            capture_tracy_with_probe=bool(raw.get("capture_tracy_with_probe", False)),
            tracy_capture_seconds=int(raw.get("tracy_capture_seconds", 10)),
        )
    except (OSError, ValueError, TypeError, json.JSONDecodeError):
        return ToolSettings()


def render_log_entry_html(entry: LogEntry) -> str:
    palette = {
        "info": ("#ffffff", "#000000", "#ffffff"),
        "debug": ("#dbeafe", "#000000", "#2563eb"),
        "warning": ("#fef3c7", "#000000", "#d97706"),
        "error": ("#fee2e2", "#000000", "#dc2626"),
    }
    foreground, background, accent = palette.get(entry.level, palette["info"])
    level_label = entry.level.upper()
    body = html.escape(entry.text)
    return (
        f"<div style='margin: 0 0 8px 0; padding: 10px 12px; border-radius: 8px; "
        f"background:{background}; border: 1px solid {accent}; color:{foreground};'>"
        f"<div style='white-space:pre; font-family:monospace; font-size:12px; color:{foreground};'>"
        f"<span style='color:{accent}; font-weight:700;'>[{level_label}]</span> - {body}</div>"
        "</div>"
    )
