#!/usr/bin/env python3

from __future__ import annotations

import html
import json
import os
import sys
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path

from PySide6 import QtCore, QtGui, QtWidgets


WORKSPACE_ROOT = Path(__file__).resolve().parents[1]
CMAKE_BUILD_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "cmake_build.sh"
CONFIGURE_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "configure.sh"
STATUS_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "status.sh"
DOCTOR_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "doctor.sh"
INSTALL_DEPS_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "install_deps.sh"
BUILD_ALL_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "build_all.sh"
RUNTIME_SESSION_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "runtime_session.sh"
TRACY_PROFILER_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "tracy_profiler.sh"
TRACY_CAPTURE_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "tracy_capture.sh"
RENDERDOC_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "renderdoc.sh"
RENDERDOC_CAPTURE_SCRIPT = WORKSPACE_ROOT / "tools" / "build" / "renderdoc_capture.sh"
LOG_ROOT = WORKSPACE_ROOT / "logs" / "engine_control"
WARNINGS_LOG = LOG_ROOT / "warnings.log"
ERRORS_LOG = LOG_ROOT / "errors.log"
STATE_PATH = LOG_ROOT / "status.json"
DASHBOARD_LOG = LOG_ROOT / "dashboard.log"
SETTINGS_PATH = LOG_ROOT / "settings.json"

PRESET_DETAILS = {
    "linux-debug": "Linux debug build. Shared deps, profiling OFF, crash diagnostics ON.",
    "linux-profile": "Linux profiling build. Shared deps, crash diagnostics ON.",
    "linux-release": "Linux release build. Static deps, LTO ON, crash diagnostics ON.",
    "windows-debug": "Windows x64 MinGW debug cross-build. Assets and managed host OFF.",
    "windows-release": "Windows x64 MinGW release cross-build. Assets and managed host OFF.",
}

LINUX_DEBUG_PRESET = "linux-debug"
LINUX_PROFILE_PRESET = "linux-profile"
LINUX_RELEASE_PRESET = "linux-release"
WINDOWS_DEBUG_PRESET = "windows-debug"
WINDOWS_RELEASE_PRESET = "windows-release"
PROFILE_LABELS = {
    LINUX_DEBUG_PRESET: "Linux Debug",
    LINUX_PROFILE_PRESET: "Linux Profile",
    LINUX_RELEASE_PRESET: "Linux Release",
    WINDOWS_DEBUG_PRESET: "Windows Debug",
    WINDOWS_RELEASE_PRESET: "Windows Release",
}
BUILD_PROFILES = (
    LINUX_DEBUG_PRESET,
    LINUX_PROFILE_PRESET,
    LINUX_RELEASE_PRESET,
    WINDOWS_DEBUG_PRESET,
    WINDOWS_RELEASE_PRESET,
)
CROSS_COMPILE_PRESETS = {
    WINDOWS_DEBUG_PRESET,
    WINDOWS_RELEASE_PRESET,
}

CRASH_DIAGNOSTICS_PRESETS = {
    LINUX_DEBUG_PRESET,
    LINUX_PROFILE_PRESET,
    LINUX_RELEASE_PRESET,
    WINDOWS_DEBUG_PRESET,
    WINDOWS_RELEASE_PRESET,
}
ENGINE_PRODUCT = "octaryn-engine"
ENGINE_PRODUCT_LABEL = f"{ENGINE_PRODUCT} (engine-only)"


@dataclass
class AppSettings:
    doctor_repair_mode: bool = False
    launch_tracy_auto: bool = False
    capture_tracy_auto: bool = False
    launch_renderdoc_auto: bool = False
    tracy_capture_seconds: int = 15


@dataclass
class ProfileSessionPlan:
    preset: str = LINUX_PROFILE_PRESET


@dataclass
class PendingCommand:
    label: str
    program: Path
    args: list[str]


@dataclass
class LogEntry:
    level: str
    text: str


ENGINE_DEBUG_SHARED_LIBS_TARGET = "octaryn_engine_debug_shared_libs"
ENGINE_RUN_TARGET = "octaryn_engine_runtime"
ENGINE_BUILD_TARGET = "octaryn_engine_runtime_bundle"


def product_build_dir(preset: str) -> Path:
    return WORKSPACE_ROOT / "build" / ENGINE_PRODUCT / preset


def resolve_run_path(preset: str) -> Path | None:
    build_dir = product_build_dir(preset)
    target = ENGINE_RUN_TARGET
    hyphenated_target = target.replace("_", "-")
    candidates = [
        build_dir / "bin" / target,
        build_dir / "bin" / f"{target}.exe",
        build_dir / "bin" / hyphenated_target,
        build_dir / "bin" / f"{hyphenated_target}.exe",
        build_dir / "apps" / target,
        build_dir / "apps" / f"{target}.exe",
        build_dir / "apps" / hyphenated_target,
        build_dir / "apps" / f"{hyphenated_target}.exe",
        build_dir / target,
        build_dir / f"{target}.exe",
    ]
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None


def command_invocation(program: Path, args: list[str]) -> tuple[str, list[str]]:
    if program.suffix == ".sh":
        shell = QtCore.QStandardPaths.findExecutable("bash") or "/usr/bin/bash"
        return shell, [str(program), *args]
    return str(program), list(args)


def now_iso() -> str:
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


def load_settings() -> AppSettings:
    if not SETTINGS_PATH.exists():
        return AppSettings()
    try:
        payload = json.loads(SETTINGS_PATH.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return AppSettings()
    defaults = asdict(AppSettings())
    return AppSettings(
        **{key: payload.get(key, value) for key, value in defaults.items()}
    )


def save_settings(settings: AppSettings) -> None:
    SETTINGS_PATH.write_text(json.dumps(asdict(settings), indent=2), encoding="utf-8")


def preset_summary(preset: str) -> str:
    details = PRESET_DETAILS.get(preset, "No preset summary available.")
    crash_state = "ON" if preset in CRASH_DIAGNOSTICS_PRESETS else "OFF"
    return f"{details}\nCrash diagnostics: {crash_state}"


def runtime_status_summary(preset: str) -> str:
    if preset in CROSS_COMPILE_PRESETS:
        return "Runtime: Windows cross-build"
    run_path = resolve_run_path(preset)
    if run_path is None:
        return "Runtime: missing"
    return f"Runtime: ready ({run_path.name})"


def build_target_for_preset(preset: str) -> str:
    if preset in CROSS_COMPILE_PRESETS:
        return ENGINE_RUN_TARGET
    return ENGINE_BUILD_TARGET


class EngineControlWindow(QtWidgets.QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Octaryn Engine Control")
        self.resize(980, 680)

        LOG_ROOT.mkdir(parents=True, exist_ok=True)
        self.settings = load_settings()
        self.pending_profile_session: ProfileSessionPlan | None = None
        self.pending_command_after_configure: PendingCommand | None = None
        self.all_log_lines: list[LogEntry] = []
        self.pending_log_entries: list[LogEntry] = []
        self._starting_process = False
        self._stopping_process = False

        self.current_label = ""
        self.current_program = ""
        self.current_args: list[str] = []
        self.process = QtCore.QProcess(self)
        self.process.setProcessChannelMode(
            QtCore.QProcess.ProcessChannelMode.MergedChannels
        )
        self.process.setWorkingDirectory(str(WORKSPACE_ROOT))
        self.process.readyReadStandardOutput.connect(self._append_process_output)
        self.process.started.connect(self._handle_process_started)
        self.process.finished.connect(self._handle_process_finished)
        self.process.errorOccurred.connect(self._handle_process_error)
        self.process_start_timer = QtCore.QTimer(self)
        self.process_start_timer.setSingleShot(True)
        self.process_start_timer.timeout.connect(self._handle_process_start_timeout)
        self.process_stop_timer = QtCore.QTimer(self)
        self.process_stop_timer.setSingleShot(True)
        self.process_stop_timer.timeout.connect(self._handle_process_stop_timeout)
        self.log_flush_timer = QtCore.QTimer(self)
        self.log_flush_timer.setSingleShot(True)
        self.log_flush_timer.timeout.connect(self._flush_log_updates)

        self.product_label = QtWidgets.QLabel(ENGINE_PRODUCT_LABEL)
        self.product_label.setWordWrap(True)

        self.profile_combo = QtWidgets.QComboBox()
        for preset in BUILD_PROFILES:
            self.profile_combo.addItem(PROFILE_LABELS[preset], preset)
        self.profile_combo.setCurrentIndex(0)

        self.configure_checkbox = QtWidgets.QCheckBox("Configure before build")
        self.configure_checkbox.setChecked(True)
        self.profile_combo.currentIndexChanged.connect(self._refresh_dashboard)

        self.build_button = QtWidgets.QPushButton("Build")
        self.build_button.clicked.connect(self.build_engine)

        self.build_shared_libs_button = QtWidgets.QPushButton("Build Libs")
        self.build_shared_libs_button.clicked.connect(self.build_debug_shared_libs)
        self.build_shared_libs_button.setToolTip(
            "Prebuild the shared dependency cache used by both debug presets."
        )

        self.start_button = QtWidgets.QPushButton("Start")
        self.start_button.clicked.connect(self.start_engine)

        self.stop_button = QtWidgets.QPushButton("Stop")
        self.stop_button.clicked.connect(self.stop_engine)

        self.status_button = QtWidgets.QPushButton("Build Status")
        self.status_button.clicked.connect(self.show_status)

        self.status_label = QtWidgets.QLabel("Idle")
        self.status_label.setWordWrap(True)

        self.doctor_repair_checkbox = QtWidgets.QCheckBox("Doctor repair mode")
        self.doctor_repair_checkbox.setChecked(self.settings.doctor_repair_mode)
        self.launch_tracy_auto_checkbox = QtWidgets.QCheckBox("Launch Tracy UI automatically")
        self.launch_tracy_auto_checkbox.setChecked(self.settings.launch_tracy_auto)
        self.capture_tracy_auto_checkbox = QtWidgets.QCheckBox("Capture Tracy automatically")
        self.capture_tracy_auto_checkbox.setChecked(self.settings.capture_tracy_auto)
        self.launch_renderdoc_auto_checkbox = QtWidgets.QCheckBox("Launch RenderDoc automatically")
        self.launch_renderdoc_auto_checkbox.setChecked(self.settings.launch_renderdoc_auto)
        self.tracy_capture_seconds_spinbox = QtWidgets.QSpinBox()
        self.tracy_capture_seconds_spinbox.setRange(1, 300)
        self.tracy_capture_seconds_spinbox.setValue(self.settings.tracy_capture_seconds)
        self.tracy_capture_seconds_spinbox.setSuffix(" sec")

        self.output_text = QtWidgets.QTextBrowser()
        self.output_text.setReadOnly(True)
        self.output_text.setOpenExternalLinks(False)
        self.output_text.setLineWrapMode(QtWidgets.QTextEdit.LineWrapMode.NoWrap)
        self.log_filter_all_checkbox = QtWidgets.QCheckBox("All")
        self.log_filter_all_checkbox.setChecked(True)
        self.log_filter_all_checkbox.toggled.connect(self._handle_all_filter_toggled)
        self.log_filter_debug_checkbox = QtWidgets.QCheckBox("Debug")
        self.log_filter_debug_checkbox.toggled.connect(
            self._handle_level_filter_toggled
        )
        self.log_filter_warning_checkbox = QtWidgets.QCheckBox("Warnings")
        self.log_filter_warning_checkbox.toggled.connect(
            self._handle_level_filter_toggled
        )
        self.log_filter_error_checkbox = QtWidgets.QCheckBox("Errors")
        self.log_filter_error_checkbox.toggled.connect(
            self._handle_level_filter_toggled
        )

        controls_group = QtWidgets.QGroupBox("Build + Runtime")
        controls_layout = QtWidgets.QGridLayout(controls_group)
        controls_layout.addWidget(QtWidgets.QLabel("Engine"), 0, 0)
        controls_layout.addWidget(QtWidgets.QLabel("Build Profile"), 0, 1)
        controls_layout.addWidget(self.product_label, 1, 0)
        controls_layout.addWidget(self.profile_combo, 1, 1)
        controls_layout.addWidget(self.configure_checkbox, 1, 2)

        button_row = QtWidgets.QHBoxLayout()
        button_row.addWidget(self.build_button)
        button_row.addWidget(self.build_shared_libs_button)
        button_row.addWidget(self.start_button)
        button_row.addWidget(self.stop_button)
        button_row.addWidget(self.status_button)
        button_row.addStretch(1)
        controls_layout.addLayout(button_row, 2, 0, 1, 3)
        controls_layout.addWidget(self.status_label, 3, 0, 1, 3)
        controls_layout.setColumnStretch(0, 1)
        controls_layout.setColumnStretch(1, 1)
        controls_layout.setColumnStretch(2, 1)

        profiling_group = QtWidgets.QGroupBox("Profiling + Diagnostics")
        profiling_layout = QtWidgets.QVBoxLayout(profiling_group)

        profiling_tools_layout = QtWidgets.QHBoxLayout()

        automation_group = QtWidgets.QGroupBox("Automation + Diagnostics")
        automation_layout = QtWidgets.QVBoxLayout(automation_group)
        self.install_deps_button = QtWidgets.QPushButton("Install Deps")
        self.install_deps_button.clicked.connect(self.install_dependencies)
        automation_layout.addWidget(self.install_deps_button)
        self.build_all_button = QtWidgets.QPushButton("Build Linux + Windows")
        self.build_all_button.clicked.connect(self.build_all_profiles)
        automation_layout.addWidget(self.build_all_button)
        self.doctor_button = QtWidgets.QPushButton("Run Build Doctor")
        self.doctor_button.clicked.connect(self.run_build_doctor)
        automation_layout.addWidget(self.doctor_button)
        self.profile_session_button = QtWidgets.QPushButton("Start Profiling Run")
        self.profile_session_button.clicked.connect(self.start_profile_session)
        automation_layout.addWidget(self.profile_session_button)
        self.tracy_profiler_button = QtWidgets.QPushButton("Start Tracy UI")
        self.tracy_profiler_button.clicked.connect(self.start_tracy_profiler)
        automation_layout.addWidget(self.tracy_profiler_button)
        self.tracy_capture_button = QtWidgets.QPushButton("Capture Tracy Now")
        self.tracy_capture_button.clicked.connect(self.start_tracy_capture)
        automation_layout.addWidget(self.tracy_capture_button)
        self.renderdoc_button = QtWidgets.QPushButton("Start RenderDoc UI")
        self.renderdoc_button.clicked.connect(self.start_renderdoc)
        automation_layout.addWidget(self.renderdoc_button)
        self.renderdoc_capture_button = QtWidgets.QPushButton("Run With RenderDoc")
        self.renderdoc_capture_button.clicked.connect(self.start_renderdoc_capture)
        automation_layout.addWidget(self.renderdoc_capture_button)
        self.logs_button = QtWidgets.QPushButton("Open Logs Folder")
        self.logs_button.clicked.connect(self.open_logs_folder)
        automation_layout.addWidget(self.logs_button)
        self.automation_help_button = QtWidgets.QPushButton("Help")
        self.automation_help_button.clicked.connect(
            lambda: self.show_help_dialog(
                "Automation + Diagnostics Help",
                "This area handles one-click workflows and recovery tools.\n\n"
                "Install Deps installs Linux system packages needed for local and Windows cross-builds.\n"
                "Build Linux + Windows configures and builds linux-debug plus windows-debug.\n"
                "Start Profiling Run builds linux-profile and launches the runtime.\n"
                "Start Tracy UI builds and launches the project-local Tracy profiler from the engine CPM cache.\n"
                "Capture Tracy Now records a headless .tracy capture and CSV export under logs/tracy.\n"
                "Start RenderDoc UI fetches, builds, registers, and launches project-local RenderDoc from the engine cache.\n"
                "Run With RenderDoc launches the selected engine build through project-local renderdoccmd on X11/Xwayland so frames can be captured from RenderDoc.\n"
                "Run Build Doctor checks tools and shared caches, and repair mode can clean broken preset/cache state when needed.\n"
                "The diagnostics and recovery path is the build doctor rather than a separate placeholder executable.",
            )
        )
        automation_layout.addWidget(self.automation_help_button)
        profiling_tools_layout.addWidget(automation_group)
        profiling_layout.addLayout(profiling_tools_layout)

        settings_group = QtWidgets.QGroupBox("Tool Settings")
        settings_layout = QtWidgets.QGridLayout(settings_group)
        settings_layout.addWidget(self.doctor_repair_checkbox, 0, 0)
        settings_layout.addWidget(self.launch_tracy_auto_checkbox, 0, 1)
        settings_layout.addWidget(self.launch_renderdoc_auto_checkbox, 0, 2)
        settings_layout.addWidget(self.capture_tracy_auto_checkbox, 1, 0)
        settings_layout.addWidget(QtWidgets.QLabel("Capture Length"), 1, 1)
        settings_layout.addWidget(self.tracy_capture_seconds_spinbox, 1, 2)
        self.save_settings_button = QtWidgets.QPushButton("Save Settings")
        self.save_settings_button.clicked.connect(self.persist_settings)
        settings_layout.addWidget(self.save_settings_button, 2, 0)
        self.settings_help_button = QtWidgets.QPushButton("Help")
        self.settings_help_button.clicked.connect(
            lambda: self.show_help_dialog(
                "Tool Settings Help",
                "Tool Settings control the local build helper behavior.\n\n"
                f"Save Settings writes your choices to {SETTINGS_PATH}.\n"
                "Doctor repair mode makes the doctor command clean broken cache/preset state when needed.\n"
                "Launch Tracy UI automatically starts the project-local Tracy profiler when a profiling run begins.\n"
                "Launch RenderDoc automatically starts the project-local RenderDoc UI after engine launch.\n"
                "Capture Tracy automatically records a headless capture after engine launch; it takes priority over auto UI because Tracy accepts one server connection at a time.",
            )
        )
        settings_layout.addWidget(self.settings_help_button, 2, 1)
        profiling_layout.addWidget(settings_group)

        console_group = QtWidgets.QGroupBox("Console")
        console_layout = QtWidgets.QVBoxLayout(console_group)
        filter_row = QtWidgets.QHBoxLayout()
        filter_row.addWidget(QtWidgets.QLabel("Show"))
        filter_row.addWidget(self.log_filter_all_checkbox)
        filter_row.addWidget(self.log_filter_debug_checkbox)
        filter_row.addWidget(self.log_filter_warning_checkbox)
        filter_row.addWidget(self.log_filter_error_checkbox)
        filter_row.addStretch(1)
        console_layout.addLayout(filter_row)
        console_layout.addWidget(self.output_text)

        left_panel = QtWidgets.QWidget()
        left_layout = QtWidgets.QVBoxLayout(left_panel)
        left_layout.setContentsMargins(0, 0, 0, 0)
        left_layout.addWidget(controls_group)
        left_layout.addWidget(profiling_group)
        left_layout.addStretch(1)

        splitter = QtWidgets.QSplitter(QtCore.Qt.Orientation.Horizontal)
        splitter.addWidget(left_panel)
        splitter.addWidget(console_group)
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)

        root_layout = QtWidgets.QVBoxLayout(self)
        root_layout.addWidget(splitter)

        self._refresh_dashboard()
        self._write_state("idle")
        self._log(
            "Octaryn old-architecture control ready. Build uses references/old-architecture/tools/build/cmake_build.sh; start launches the built runtime directly."
        )
        self._log(f"Workspace: {WORKSPACE_ROOT}")
        self._log(f"Logs: {LOG_ROOT}")

    def selected_profile_label(self) -> str:
        return self.profile_combo.currentText()

    def selected_preset(self) -> str:
        return self.profile_combo.currentData() or LINUX_DEBUG_PRESET

    def build_engine(self) -> None:
        preset = self.selected_preset()
        self._start_build_command(
            f"build engine/{preset}",
            preset,
            build_target_for_preset(preset),
        )

    def build_debug_shared_libs(self) -> None:
        preset = self.selected_preset()
        if preset == LINUX_RELEASE_PRESET:
            preset = LINUX_DEBUG_PRESET
        self._start_build_command(
            f"build debug shared libs/{preset}",
            preset,
            ENGINE_DEBUG_SHARED_LIBS_TARGET,
        )

    def start_engine(self) -> None:
        preset = self.selected_preset()
        if preset in CROSS_COMPILE_PRESETS:
            self._log(
                f"[warning] {preset} produces a Windows executable and cannot be started directly on this Linux host."
            )
            self.status_label.setText("Windows cross-build cannot start locally")
            self._write_state("cross-runtime-not-started")
            return
        run_path = resolve_run_path(preset)
        if run_path is None:
            self._log(
                f"[error] could not find a built runtime for {preset}. "
                "Build Libs only prepares shared dependencies; use Build to produce the engine executable."
            )
            self.status_label.setText("No built runtime found")
            self._write_state("missing-executable")
            return

        self._start_command(
            f"engine {preset}",
            RUNTIME_SESSION_SCRIPT,
            ["start", "--preset", preset, "--program", str(run_path)],
        )

    def stop_engine(self) -> None:
        if self.process.state() != QtCore.QProcess.ProcessState.NotRunning:
            self._log(f"[info] stopping {self.current_label}...")
            self._write_state("stopping")
            self._stopping_process = True
            self.process.terminate()
            self.process_stop_timer.start(5000)
            return

        preset = self.selected_preset()
        self._start_command(
            f"stop engine {preset}",
            RUNTIME_SESSION_SCRIPT,
            ["stop", "--preset", preset],
        )

    def show_status(self) -> None:
        self._start_command("status engine", STATUS_SCRIPT, [])

    def run_build_doctor(self) -> None:
        preset = self.selected_preset()
        args = ["--preset", preset]
        if self.doctor_repair_checkbox.isChecked():
            args.insert(0, "--repair")
        self._start_command("build doctor", DOCTOR_SCRIPT, args)

    def install_dependencies(self) -> None:
        self._start_command("install dependencies", INSTALL_DEPS_SCRIPT, ["--yes"])

    def build_all_profiles(self) -> None:
        self._start_command("build linux + windows", BUILD_ALL_SCRIPT, [])

    def start_profile_session(self) -> None:
        self.profile_combo.setCurrentText(PROFILE_LABELS[LINUX_PROFILE_PRESET])
        self.pending_profile_session = ProfileSessionPlan(preset=LINUX_PROFILE_PRESET)
        if not self._start_build_command(
            f"profiling run build engine/{LINUX_PROFILE_PRESET}",
            LINUX_PROFILE_PRESET,
            build_target_for_preset(LINUX_PROFILE_PRESET),
        ):
            self.pending_profile_session = None

    def start_tracy_profiler(self) -> None:
        self._start_tracy_profiler_detached()

    def start_tracy_capture(self) -> None:
        self._start_tracy_capture()

    def start_renderdoc(self) -> None:
        self._start_renderdoc_detached()

    def start_renderdoc_capture(self) -> None:
        preset = self.selected_preset()
        run_path = resolve_run_path(preset)
        if run_path is None:
            self._log(
                f"[error] could not find a built runtime for {preset}. Build the engine first."
            )
            self.status_label.setText("No built runtime found")
            self._write_state("missing-executable")
            return
        self._start_command(
            f"renderdoc capture {preset}",
            RENDERDOC_CAPTURE_SCRIPT,
            ["--preset", preset, "--program", str(run_path)],
        )

    def persist_settings(self) -> None:
        self.settings = self.current_settings()
        save_settings(self.settings)
        self._log(f"[info] saved tool settings: {SETTINGS_PATH}")
        self._write_state("settings-saved")
        self._refresh_dashboard()

    def current_settings(self) -> AppSettings:
        return AppSettings(
            doctor_repair_mode=self.doctor_repair_checkbox.isChecked(),
            launch_tracy_auto=self.launch_tracy_auto_checkbox.isChecked(),
            capture_tracy_auto=self.capture_tracy_auto_checkbox.isChecked(),
            launch_renderdoc_auto=self.launch_renderdoc_auto_checkbox.isChecked(),
            tracy_capture_seconds=self.tracy_capture_seconds_spinbox.value(),
        )

    def open_logs_folder(self) -> None:
        self._log(f"[info] logs folder: {LOG_ROOT}")
        opener = None
        opener_args: list[str] = []
        if sys.platform.startswith("linux"):
            opener = "xdg-open"
            opener_args = [str(LOG_ROOT)]
        elif sys.platform == "darwin":
            opener = "open"
            opener_args = [str(LOG_ROOT)]
        elif os.name == "nt":
            opener = "explorer"
            opener_args = [str(LOG_ROOT)]

        if opener is None:
            self._log("[warning] no folder opener configured for this platform.")
            return

        QtCore.QProcess.startDetached(opener, opener_args, str(WORKSPACE_ROOT))

    def show_help_dialog(self, title: str, body: str) -> None:
        QtWidgets.QMessageBox.information(self, title, body)

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        if self.process.state() != QtCore.QProcess.ProcessState.NotRunning:
            self.stop_engine()
        super().closeEvent(event)

    def _start_build_command(self, label: str, preset: str, target: str) -> bool:
        build_command = PendingCommand(
            label=label,
            program=CMAKE_BUILD_SCRIPT,
            args=["--preset", preset, "--target", target],
        )
        if self.configure_checkbox.isChecked():
            if self._start_command(
                f"configure {preset}", CONFIGURE_SCRIPT, ["--preset", preset]
            ):
                self.pending_command_after_configure = build_command
                return True
            return False
        return self._start_command(
            build_command.label, build_command.program, build_command.args
        )

    def _start_command(self, label: str, program: Path, args: list[str]) -> bool:
        if self.process.state() != QtCore.QProcess.ProcessState.NotRunning:
            self._log(
                "[info] another command is already running. Stop it before starting a new one."
            )
            self._write_state("busy")
            return False

        if not program.exists():
            self._log(f"[error] expected tool was not found: {program}")
            self.status_label.setText("Missing tool")
            self._write_state("missing-tool")
            return False

        self.current_label = label
        launch_program, launch_args = command_invocation(program, args)
        self.current_program = launch_program
        self.current_args = list(launch_args)
        self.status_label.setText(f"Running: {label}")
        self._log(f"$ {launch_program} {' '.join(launch_args)}".rstrip())
        self._write_state("running")
        self._starting_process = True
        self.process.start(launch_program, launch_args)
        self.process_start_timer.start(3000)
        self._refresh_dashboard()
        return True

    def _handle_process_started(self) -> None:
        self._starting_process = False
        self.process_start_timer.stop()

    def _handle_process_start_timeout(self) -> None:
        if (
            self._starting_process
            and self.process.state() == QtCore.QProcess.ProcessState.Starting
        ):
            self._starting_process = False
            self._log(f"[error] failed to start {self.current_label}.")
            self.status_label.setText(f"Failed: {self.current_label}")
            self._write_state("failed-to-start")
            self.process.kill()

    def _handle_process_stop_timeout(self) -> None:
        if (
            self._stopping_process
            and self.process.state() != QtCore.QProcess.ProcessState.NotRunning
        ):
            self._log(
                f"[warning] force-killing {self.current_label} after stop timeout."
            )
            self.process.kill()

    def _append_process_output(self) -> None:
        output = bytes(self.process.readAllStandardOutput()).decode(errors="replace")
        if output:
            for line in output.splitlines():
                self._log(line)

    def _handle_process_finished(
        self, exit_code: int, _exit_status: QtCore.QProcess.ExitStatus
    ) -> None:
        self.process_start_timer.stop()
        self.process_stop_timer.stop()
        self._starting_process = False
        self._stopping_process = False
        completed_label = self.current_label
        succeeded = exit_code == 0
        if exit_code == 0:
            self._log(f"[info] {self.current_label} finished successfully.")
            self.status_label.setText(f"Finished: {self.current_label}")
            self._write_state("finished", exit_code)
        else:
            self._log(f"[error] {self.current_label} exited with code {exit_code}.")
            self.status_label.setText(f"Failed: {self.current_label}")
            self._write_state("failed", exit_code)
        self.current_label = ""
        self.current_program = ""
        self.current_args = []
        if self._continue_pending_after_configure(exit_code, completed_label):
            return
        continued = self._continue_profile_session(exit_code, completed_label)
        if succeeded and not continued:
            self._launch_post_command_tools(completed_label)
        self._refresh_dashboard()

    def _continue_pending_after_configure(
        self, exit_code: int, completed_label: str
    ) -> bool:
        pending_command = self.pending_command_after_configure
        if pending_command is None or not completed_label.startswith("configure "):
            return False
        self.pending_command_after_configure = None
        if exit_code != 0:
            self._log("[error] build skipped because configure failed.")
            return False
        self._start_command(
            pending_command.label, pending_command.program, pending_command.args
        )
        return True

    def _handle_process_error(self, error: QtCore.QProcess.ProcessError) -> None:
        if error == QtCore.QProcess.ProcessError.UnknownError:
            return
        self.process_start_timer.stop()
        self._starting_process = False
        self._log(f"[error] process error: {error.name}.")
        self._write_state("process-error")

    def _log(self, message: str) -> None:
        for line in message.splitlines() or [message]:
            entry = LogEntry(level=classify_line(line) or "info", text=line)
            self.all_log_lines.append(entry)
            self.pending_log_entries.append(entry)
        if not self.log_flush_timer.isActive():
            self.log_flush_timer.start(33)
        append_text(DASHBOARD_LOG, message)
        self._write_filtered_logs(message)

    def _refresh_log_view(self) -> None:
        horizontal_scroll = self.output_text.horizontalScrollBar().value()
        html_body = "".join(
            render_log_entry_html(entry)
            for entry in self.all_log_lines
            if self._entry_matches_filter(entry)
        )
        self.output_text.setHtml(
            "<html><body style='background:#000000; margin:6px;'>"
            + html_body
            + "</body></html>"
        )
        cursor = self.output_text.textCursor()
        cursor.movePosition(QtGui.QTextCursor.MoveOperation.End)
        self.output_text.setTextCursor(cursor)
        self.output_text.horizontalScrollBar().setValue(horizontal_scroll)

    def _flush_log_updates(self) -> None:
        if not self.pending_log_entries:
            return
        visible_entries = [
            entry
            for entry in self.pending_log_entries
            if self._entry_matches_filter(entry)
        ]
        self.pending_log_entries.clear()
        if not visible_entries:
            return

        horizontal_scroll = self.output_text.horizontalScrollBar().value()
        cursor = self.output_text.textCursor()
        cursor.movePosition(QtGui.QTextCursor.MoveOperation.End)
        fragment = "".join(render_log_entry_html(entry) for entry in visible_entries)
        cursor.insertHtml(fragment)
        cursor.movePosition(QtGui.QTextCursor.MoveOperation.End)
        self.output_text.setTextCursor(cursor)
        self.output_text.ensureCursorVisible()
        self.output_text.horizontalScrollBar().setValue(horizontal_scroll)

    def _entry_matches_filter(self, entry: LogEntry) -> bool:
        if self.log_filter_all_checkbox.isChecked():
            return True
        active_levels = set()
        if self.log_filter_debug_checkbox.isChecked():
            active_levels.add("debug")
        if self.log_filter_warning_checkbox.isChecked():
            active_levels.add("warning")
        if self.log_filter_error_checkbox.isChecked():
            active_levels.add("error")
        if not active_levels:
            return True
        return entry.level in active_levels

    def _handle_all_filter_toggled(self, checked: bool) -> None:
        if checked:
            for checkbox in (
                self.log_filter_debug_checkbox,
                self.log_filter_warning_checkbox,
                self.log_filter_error_checkbox,
            ):
                previous = checkbox.blockSignals(True)
                checkbox.setChecked(False)
                checkbox.blockSignals(previous)
        self._refresh_log_view()

    def _handle_level_filter_toggled(self, checked: bool) -> None:
        if checked and self.log_filter_all_checkbox.isChecked():
            previous = self.log_filter_all_checkbox.blockSignals(True)
            self.log_filter_all_checkbox.setChecked(False)
            self.log_filter_all_checkbox.blockSignals(previous)

        if (
            not self.log_filter_debug_checkbox.isChecked()
            and not self.log_filter_warning_checkbox.isChecked()
            and not self.log_filter_error_checkbox.isChecked()
        ):
            previous = self.log_filter_all_checkbox.blockSignals(True)
            self.log_filter_all_checkbox.setChecked(True)
            self.log_filter_all_checkbox.blockSignals(previous)

        self._refresh_log_view()

    def _refresh_dashboard(self) -> None:
        settings = self.current_settings()
        preset = self.selected_preset()
        profile_label = self.selected_profile_label()
        current_run = self.current_label or "idle"
        self.status_label.setText(
            f"{ENGINE_PRODUCT}/{profile_label} • preset {preset}"
            f" • {current_run} • {preset_summary(preset).replace(chr(10), ' • ')}"
            f" • {runtime_status_summary(preset)}"
        )
        self.build_button.setText(f"Build {profile_label}")
        self.build_shared_libs_button.setEnabled(preset != LINUX_RELEASE_PRESET)
        self.build_shared_libs_button.setText("Build Libs")
        self.install_deps_button.setText("Install Deps")
        self.build_all_button.setText("Build Linux + Windows")
        self.profile_session_button.setText("Start Profiling Run")
        self.tracy_profiler_button.setText("Start Tracy UI")
        self.renderdoc_button.setText("Start RenderDoc UI")
        self.renderdoc_capture_button.setText(f"Run With RenderDoc ({preset})")
        self.doctor_button.setText(
            f"Run Build Doctor{' [Repair]' if settings.doctor_repair_mode else ''}"
        )

    def _continue_profile_session(self, exit_code: int, completed_label: str) -> bool:
        plan = self.pending_profile_session
        if plan is None:
            return False
        if exit_code != 0:
            self._log("[error] profiling run aborted because the build step failed.")
            self.pending_profile_session = None
            return True
        if completed_label.startswith("profiling run build"):
            run_path = resolve_run_path(plan.preset)
            if run_path is None:
                self._log(
                    f"[error] profiling run could not find runtime binary for {ENGINE_PRODUCT}/{plan.preset}."
                )
                self.pending_profile_session = None
                return True
            self._start_command(
                f"profiling run runtime {ENGINE_PRODUCT}/{plan.preset}", run_path, []
            )
            QtCore.QTimer.singleShot(1200, self._launch_profile_session_tools)
            return True
        return False

    def _launch_profile_session_tools(self) -> None:
        if self.pending_profile_session is None:
            return
        self._write_state("profiling-run-running")
        if self.current_settings().capture_tracy_auto:
            self._log("[info] auto-capturing Tracy data for profiling run.")
            self._start_tracy_capture()
            self.pending_profile_session = None
            return
        if self.current_settings().launch_tracy_auto:
            self._start_tracy_profiler_detached()
        if self.current_settings().launch_renderdoc_auto:
            self._start_renderdoc_detached()
        self.pending_profile_session = None

    def _launch_post_command_tools(self, completed_label: str) -> None:
        if not completed_label.startswith("engine "):
            return
        settings = self.current_settings()
        if settings.capture_tracy_auto:
            self._log("[info] auto-capturing Tracy data after engine start.")
            self._start_tracy_capture()
            return
        if settings.launch_tracy_auto:
            self._log("[info] auto-launching project Tracy UI after engine start.")
            self._start_tracy_profiler_detached()
        if settings.launch_renderdoc_auto:
            self._log("[info] auto-launching project RenderDoc UI after engine start.")
            self._start_renderdoc_detached()

    def _start_tracy_capture(self) -> None:
        if not TRACY_CAPTURE_SCRIPT.exists():
            self._log(f"[error] expected Tracy capture tool was not found: {TRACY_CAPTURE_SCRIPT}")
            self._write_state("missing-tracy-capture")
            return
        seconds = self.current_settings().tracy_capture_seconds
        self._start_command(
            "tracy capture",
            TRACY_CAPTURE_SCRIPT,
            ["--seconds", str(seconds)],
        )

    def _start_tracy_profiler_detached(self) -> None:
        if not TRACY_PROFILER_SCRIPT.exists():
            self._log(f"[error] expected Tracy launcher was not found: {TRACY_PROFILER_SCRIPT}")
            self._write_state("missing-tracy-launcher")
            return
        launcher = QtCore.QStandardPaths.findExecutable("bash") or "/usr/bin/bash"
        args = [str(TRACY_PROFILER_SCRIPT)]
        self._log(f"$ {launcher} {' '.join(args)}")
        if QtCore.QProcess.startDetached(launcher, args, str(WORKSPACE_ROOT)):
            self._log("[info] project Tracy UI launcher started.")
            self._write_state("tracy-launcher-started")
        else:
            self._log("[error] failed to start project Tracy UI launcher.")
            self._write_state("tracy-launcher-failed")

    def _start_renderdoc_detached(self) -> None:
        if not RENDERDOC_SCRIPT.exists():
            self._log(f"[error] expected RenderDoc launcher was not found: {RENDERDOC_SCRIPT}")
            self._write_state("missing-renderdoc-launcher")
            return
        launcher = QtCore.QStandardPaths.findExecutable("bash") or "/usr/bin/bash"
        args = [str(RENDERDOC_SCRIPT)]
        self._log(f"$ {launcher} {' '.join(args)}")
        self._log("[info] RenderDoc UI setup will build/register the project-local Vulkan layer and launch qrenderdoc on X11/XCB.")
        if QtCore.QProcess.startDetached(launcher, args, str(WORKSPACE_ROOT)):
            self._log("[info] project RenderDoc UI launcher started.")
            self._write_state("renderdoc-launcher-started")
        else:
            self._log("[error] failed to start project RenderDoc UI launcher.")
            self._write_state("renderdoc-launcher-failed")

    def _write_filtered_logs(self, message: str) -> None:
        for line in message.splitlines():
            level = classify_line(line)
            if level is None:
                continue
            if level == "warning":
                log_path = WARNINGS_LOG
            elif level == "error":
                log_path = ERRORS_LOG
            else:
                continue
            with log_path.open("a", encoding="utf-8") as handle:
                handle.write(f"[{now_iso()}] {line}\n")

    def _write_state(self, phase: str, exit_code: int | None = None) -> None:
        settings = self.current_settings()
        preset = self.selected_preset()
        state = {
            "updated_at": now_iso(),
            "phase": phase,
            "selected_product": ENGINE_PRODUCT,
            "selected_profile": self.selected_profile_label(),
            "selected_preset": preset,
            "current_label": self.current_label,
            "current_program": self.current_program,
            "current_args": self.current_args,
            "process_id": int(self.process.processId())
            if self.process.processId()
            else None,
            "status_text": self.status_label.text(),
            "warnings_log": str(WARNINGS_LOG),
            "errors_log": str(ERRORS_LOG),
            "dashboard_log": str(DASHBOARD_LOG),
            "doctor_script": str(DOCTOR_SCRIPT),
            "crash_diagnostics_enabled_for_preset": preset in CRASH_DIAGNOSTICS_PRESETS,
            "settings_path": str(SETTINGS_PATH),
            "tool_settings": asdict(settings),
            "pending_profile_session": asdict(self.pending_profile_session)
            if self.pending_profile_session
            else None,
            "pending_command_after_configure": {
                "label": self.pending_command_after_configure.label,
                "program": str(self.pending_command_after_configure.program),
                "args": self.pending_command_after_configure.args,
            }
            if self.pending_command_after_configure
            else None,
        }
        if exit_code is not None:
            state["exit_code"] = exit_code
        STATE_PATH.write_text(json.dumps(state, indent=2), encoding="utf-8")


def build_application() -> QtWidgets.QApplication:
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("Octaryn Engine Control")
    return app


def main() -> int:
    if (
        not CMAKE_BUILD_SCRIPT.exists()
        or not CONFIGURE_SCRIPT.exists()
        or not STATUS_SCRIPT.exists()
    ):
        print(
            "Expected Octaryn old-architecture build helpers were not found under references/old-architecture/tools/build.",
            file=sys.stderr,
        )
        return 1

    app = build_application()
    window = EngineControlWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
