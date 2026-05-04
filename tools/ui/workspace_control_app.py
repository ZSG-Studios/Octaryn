#!/usr/bin/env python3

from __future__ import annotations

import sys

from PySide6 import QtWidgets

from workspace_control.paths import (
    HOST_SETUP_SCRIPT,
    PODMAN_BUILD_SCRIPT,
    TRACY_TOOL_SCRIPT,
)
from workspace_control.window import WorkspaceControlWindow


def build_application() -> QtWidgets.QApplication:
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("Octaryn Workspace Control")
    return app


def main() -> int:
    if (
        not PODMAN_BUILD_SCRIPT.exists()
        or not TRACY_TOOL_SCRIPT.exists()
        or not HOST_SETUP_SCRIPT.exists()
    ):
        print(
            "Expected Octaryn root tools were not found under tools/.",
            file=sys.stderr,
        )
        return 1

    app = build_application()
    window = WorkspaceControlWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
