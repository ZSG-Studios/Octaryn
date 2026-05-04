from __future__ import annotations

from pathlib import Path

from PySide6 import QtCore

from .commands import PendingCommand, command_invocation
from .paths import LOG_ROOT, PODMAN_BUILD_SCRIPT, WORKSPACE_ROOT


class WorkspaceProcessControllerMixin:
    def _start_build_command(
        self, label: str, preset: str, target: str, action: str = "build"
    ) -> bool:
        build_command = PendingCommand(
            label=label,
            program=PODMAN_BUILD_SCRIPT,
            args=[action, preset]
            if action == "validate"
            else [action, preset, "--target", target],
        )
        if self.configure_checkbox.isChecked():
            if self._start_command(
                f"configure {preset}", PODMAN_BUILD_SCRIPT, ["configure", preset]
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
        process_environment = QtCore.QProcessEnvironment.systemEnvironment()
        process_environment.insert("OCTARYN_TARGET_ARCH", self.selected_arch())
        self.process.setProcessEnvironment(process_environment)
        self.process.start(launch_program, launch_args)
        self.process_start_timer.start(3000)
        self._refresh_dashboard()
        return True

    def _start_detached_tool(self, label: str, program: Path, args: list[str]) -> bool:
        if not program.exists():
            self._log(f"[error] expected debug tool was not found: {program}")
            return False
        launch_program, launch_args = command_invocation(program, args)
        self._log(f"$ {launch_program} {' '.join(launch_args)}".rstrip())
        if QtCore.QProcess.startDetached(launch_program, launch_args, str(WORKSPACE_ROOT)):
            self._log(f"[info] launched {label}; tool logs write under {LOG_ROOT}.")
            return True
        self._log(f"[error] failed to launch {label}.")
        return False

    def _handle_process_started(self) -> None:
        self._starting_process = False
        self.process_start_timer.stop()
        label = self.current_label
        if label.startswith("client probe ") or label.startswith("probe run "):
            QtCore.QTimer.singleShot(
                1200, lambda completed_label=label: self._launch_post_command_tools(completed_label)
            )

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
        continued = self._continue_probe_run(exit_code, completed_label)
        if succeeded and not continued and not completed_label.startswith(("client probe ", "probe run ")):
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
