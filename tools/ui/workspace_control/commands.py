from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from PySide6 import QtCore


@dataclass
class ProbeRunPlan:
    preset: str


@dataclass
class PendingCommand:
    label: str
    program: Path
    args: list[str]


WORKSPACE_BUILD_TARGET = "octaryn_all"
WORKSPACE_VALIDATE_TARGET = "octaryn_validate_all"


def build_target_for_preset(_preset: str) -> str:
    return WORKSPACE_BUILD_TARGET


def command_invocation(program: Path, args: list[str]) -> tuple[str, list[str]]:
    if program.suffix == ".sh":
        shell = QtCore.QStandardPaths.findExecutable("bash") or "/usr/bin/bash"
        return shell, [str(program), *args]
    return str(program), list(args)
