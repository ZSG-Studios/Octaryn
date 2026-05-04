from __future__ import annotations

import json
import sys
from dataclasses import asdict

from PySide6 import QtCore, QtGui, QtWidgets

from .commands import (
    PendingCommand,
    ProbeRunPlan,
    WORKSPACE_VALIDATE_TARGET,
    build_target_for_preset,
)
from .layout import build_workspace_control_layout
from .logs import (
    LogEntry,
    append_text,
    classify_line,
    load_tool_settings,
    now_iso,
    render_log_entry_html,
)
from .paths import (
    BOOTSTRAP_SCRIPT,
    DASHBOARD_LOG,
    ERRORS_LOG,
    HOST_SETUP_SCRIPT,
    LOG_ROOT,
    PODMAN_BUILD_SCRIPT,
    STATE_PATH,
    TRACY_TOOL_SCRIPT,
    WARNINGS_LOG,
    WORKSPACE_ROOT,
    resolve_run_path,
)
from .process_controller import WorkspaceProcessControllerMixin
from .status import (
    ACTIVE_PRODUCT,
    CROSS_COMPILE_PRESETS,
    LINUX_DEBUG_PRESET,
    PRESET_LABELS,
    host_arch,
    host_platform,
    host_status_summary,
    native_run_state_summary,
    podman_build_environment_summary,
    preset_summary,
)


class WorkspaceControlWindow(WorkspaceProcessControllerMixin, QtWidgets.QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Octaryn Workspace Control")
        self.resize(980, 680)

        LOG_ROOT.mkdir(parents=True, exist_ok=True)
        self.pending_probe_run: ProbeRunPlan | None = None
        self.pending_command_after_configure: PendingCommand | None = None
        self.all_log_lines: list[LogEntry] = []
        self.pending_log_entries: list[LogEntry] = []
        self._starting_process = False
        self._stopping_process = False
        self.tool_settings = load_tool_settings()

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

        build_workspace_control_layout(self)
        self._refresh_dashboard()
        self._write_state("idle")
        self._log(
            "Octaryn control ready. Configure, build, validate, and full matrix builds run through the Podman build wrapper."
        )
        self._log(f"Workspace: {WORKSPACE_ROOT}")
        self._log(f"Logs: {LOG_ROOT}")

    def selected_preset_label(self) -> str:
        return self.preset_combo.currentText()

    def selected_preset(self) -> str:
        return self.preset_combo.currentData() or LINUX_DEBUG_PRESET

    def selected_arch(self) -> str:
        return self.arch_combo.currentData() or "x64"

    def build_workspace(self) -> None:
        preset = self.selected_preset()
        self._start_build_command(
            f"build {preset}",
            preset,
            build_target_for_preset(preset),
        )

    def validate_workspace(self) -> None:
        preset = self.selected_preset()
        self._start_build_command(
            f"validate/{preset}",
            preset,
            WORKSPACE_VALIDATE_TARGET,
            "validate",
        )

    def start_probe(self) -> None:
        preset = self.selected_preset()
        runnable, run_state = native_run_state_summary(preset, self.selected_arch())
        if not runnable and "blocked" in run_state:
            self._log(f"[warning] {run_state}. Native runtime launch stays outside Podman.")
            self.status_label.setText(run_state)
            self._write_state("native-runtime-blocked")
            return
        run_path = resolve_run_path(preset, self.selected_arch())
        if run_path is None:
            self._log(
                f"[error] could not find a built client launch probe for {preset}. "
                "Use Build to produce the client launch probe."
            )
            self.status_label.setText("No built probe found")
            self._write_state("missing-executable")
            return
        self._start_command(f"client probe {preset}", run_path, [])

    def stop_command(self) -> None:
        if self.process.state() != QtCore.QProcess.ProcessState.NotRunning:
            self._log(f"[info] stopping {self.current_label}...")
            self._write_state("stopping")
            self._stopping_process = True
            self.process.terminate()
            self.process_stop_timer.start(5000)
            return
        self._log("[info] no running process to stop.")
        self._write_state("idle")

    def show_status(self) -> None:
        self._start_command("list build presets", PODMAN_BUILD_SCRIPT, ["list-presets"])

    def run_build_doctor(self) -> None:
        self.show_status()

    def build_all_presets(self) -> None:
        self._start_command("build all presets and architectures", PODMAN_BUILD_SCRIPT, ["build-all"])

    def start_probe_run(self) -> None:
        host_debug_preset = LINUX_DEBUG_PRESET
        self.preset_combo.setCurrentText(PRESET_LABELS[host_debug_preset])
        self.pending_probe_run = ProbeRunPlan(preset=host_debug_preset)
        if not self._start_build_command(
            f"probe run build {host_debug_preset}",
            host_debug_preset,
            build_target_for_preset(host_debug_preset),
        ):
            self.pending_probe_run = None

    def open_logs_folder(self) -> None:
        self._log(f"[info] logs folder: {LOG_ROOT}")
        opener = None
        opener_args: list[str] = []
        if sys.platform.startswith("linux"):
            opener = "xdg-open"
            opener_args = [str(LOG_ROOT)]
        if opener is None:
            self._log("[warning] no folder opener configured for this platform.")
            return
        QtCore.QProcess.startDetached(opener, opener_args, str(WORKSPACE_ROOT))

    def launch_tracy(self) -> None:
        preset = self.selected_preset()
        if preset in CROSS_COMPILE_PRESETS:
            self._log(
                f"[warning] {preset} is a cross-build; Tracy profiler must run on the target platform."
            )
            return
        self._start_detached_tool(
            "Tracy profiler",
            TRACY_TOOL_SCRIPT,
            ["--preset", preset, "launch-profiler"],
        )

    def capture_tracy(self) -> None:
        preset = self.selected_preset()
        if preset in CROSS_COMPILE_PRESETS:
            self._log(
                f"[warning] {preset} is a cross-build; Tracy capture must run on the target platform."
            )
            return
        self._start_detached_tool(
            "Tracy capture",
            TRACY_TOOL_SCRIPT,
            ["--preset", preset, "--seconds", str(self.tracy_seconds_spin.value()), "capture"],
        )

    def run_bootstrap_check(self) -> None:
        self._start_command("validate build environment", HOST_SETUP_SCRIPT, [])

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        if self.process.state() != QtCore.QProcess.ProcessState.NotRunning:
            self.stop_command()
        super().closeEvent(event)

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
        cursor.insertHtml("".join(render_log_entry_html(entry) for entry in visible_entries))
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
        preset = self.selected_preset()
        preset_label = self.selected_preset_label()
        current_run = self.current_label or "idle"
        _runnable, run_state = native_run_state_summary(preset, self.selected_arch())
        self.status_label.setText(
            f"{ACTIVE_PRODUCT}/{preset_label} • preset {preset}"
            f" • {current_run} • {host_status_summary()}"
            f" • {podman_build_environment_summary()}"
            f" • {preset_summary(preset).replace(chr(10), ' • ')}"
            f" • {run_state}"
        )
        self.build_button.setText(f"Build {preset_label}")
        self.validate_button.setEnabled(True)
        self.validate_button.setText("Validate")
        self.build_all_button.setText("Build All Presets")
        self.probe_run_button.setText("Build + Start Probe")
        self.doctor_button.setText("List Presets")

    def _continue_probe_run(self, exit_code: int, completed_label: str) -> bool:
        plan = self.pending_probe_run
        if plan is None:
            return False
        if exit_code != 0:
            self._log("[error] probe run aborted because the build step failed.")
            self.pending_probe_run = None
            return True
        if completed_label.startswith("probe run build"):
            run_path = resolve_run_path(plan.preset, self.selected_arch())
            if run_path is None:
                self._log(
                    f"[error] probe run could not find client launch probe for {ACTIVE_PRODUCT}/{plan.preset}."
                )
                self.pending_probe_run = None
                return True
            self._start_command(f"probe run {ACTIVE_PRODUCT}/{plan.preset}", run_path, [])
            self.pending_probe_run = None
            return True
        return False

    def _launch_post_command_tools(self, completed_label: str) -> None:
        if not (
            completed_label.startswith("client probe ")
            or completed_label.startswith("probe run ")
        ):
            return
        if self.launch_tracy_checkbox.isChecked():
            self.launch_tracy()
        if self.capture_tracy_checkbox.isChecked():
            QtCore.QTimer.singleShot(2000, self.capture_tracy)

    def _save_tool_settings(self) -> None:
        self.tool_settings.launch_tracy_with_probe = self.launch_tracy_checkbox.isChecked()
        self.tool_settings.capture_tracy_with_probe = self.capture_tracy_checkbox.isChecked()
        self.tool_settings.tracy_capture_seconds = self.tracy_seconds_spin.value()
        from .paths import SETTINGS_PATH

        SETTINGS_PATH.write_text(
            json.dumps(asdict(self.tool_settings), indent=2), encoding="utf-8"
        )

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
        preset = self.selected_preset()
        state = {
            "updated_at": now_iso(),
            "phase": phase,
            "selected_product": ACTIVE_PRODUCT,
            "selected_preset_label": self.selected_preset_label(),
            "selected_preset": preset,
            "selected_arch": self.selected_arch(),
            "current_label": self.current_label,
            "current_program": self.current_program,
            "current_args": self.current_args,
            "process_id": int(self.process.processId()) if self.process.processId() else None,
            "status_text": self.status_label.text(),
            "warnings_log": str(WARNINGS_LOG),
            "errors_log": str(ERRORS_LOG),
            "dashboard_log": str(DASHBOARD_LOG),
            "podman_build_script": str(PODMAN_BUILD_SCRIPT),
            "host_platform": host_platform(),
            "host_arch": host_arch(),
            "podman_build_environment": podman_build_environment_summary(),
            "tracy_tool": str(TRACY_TOOL_SCRIPT),
            "bootstrap_script": str(BOOTSTRAP_SCRIPT),
            "host_setup_script": str(HOST_SETUP_SCRIPT),
            "tool_settings": asdict(self.tool_settings),
            "pending_probe_run": asdict(self.pending_probe_run)
            if self.pending_probe_run
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
