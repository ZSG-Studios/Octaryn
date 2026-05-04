from __future__ import annotations

from PySide6 import QtCore, QtWidgets

from .status import ACTIVE_PRODUCT_LABEL, BUILD_PRESETS, PRESET_LABELS


def build_workspace_control_layout(window: QtWidgets.QWidget) -> None:
    window.product_label = QtWidgets.QLabel(ACTIVE_PRODUCT_LABEL)
    window.product_label.setWordWrap(True)

    window.preset_combo = QtWidgets.QComboBox()
    for preset in BUILD_PRESETS:
        window.preset_combo.addItem(PRESET_LABELS[preset], preset)
    window.preset_combo.setCurrentIndex(0)

    window.arch_combo = QtWidgets.QComboBox()
    window.arch_combo.addItem("x64", "x64")
    window.arch_combo.addItem("ARM64", "arm64")
    window.arch_combo.currentIndexChanged.connect(window._refresh_dashboard)

    window.configure_checkbox = QtWidgets.QCheckBox("Configure before build")
    window.configure_checkbox.setChecked(True)
    window.preset_combo.currentIndexChanged.connect(window._refresh_dashboard)

    window.build_button = QtWidgets.QPushButton("Build")
    window.build_button.clicked.connect(window.build_workspace)

    window.validate_button = QtWidgets.QPushButton("Validate")
    window.validate_button.clicked.connect(window.validate_workspace)
    window.validate_button.setToolTip(
        "Run the active CMake validation aggregate for the selected preset."
    )

    window.start_button = QtWidgets.QPushButton("Start Probe")
    window.start_button.clicked.connect(window.start_probe)

    window.stop_button = QtWidgets.QPushButton("Stop")
    window.stop_button.clicked.connect(window.stop_command)

    window.status_button = QtWidgets.QPushButton("List Presets")
    window.status_button.clicked.connect(window.show_status)

    window.launch_tracy_checkbox = QtWidgets.QCheckBox("Launch Tracy with probe")
    window.launch_tracy_checkbox.setChecked(window.tool_settings.launch_tracy_with_probe)
    window.launch_tracy_checkbox.toggled.connect(lambda _checked: window._save_tool_settings())

    window.capture_tracy_checkbox = QtWidgets.QCheckBox("Capture Tracy after start")
    window.capture_tracy_checkbox.setChecked(window.tool_settings.capture_tracy_with_probe)
    window.capture_tracy_checkbox.toggled.connect(lambda _checked: window._save_tool_settings())

    window.tracy_seconds_spin = QtWidgets.QSpinBox()
    window.tracy_seconds_spin.setRange(1, 300)
    window.tracy_seconds_spin.setValue(window.tool_settings.tracy_capture_seconds)
    window.tracy_seconds_spin.valueChanged.connect(lambda _value: window._save_tool_settings())

    window.launch_tracy_button = QtWidgets.QPushButton("Launch Tracy")
    window.launch_tracy_button.clicked.connect(window.launch_tracy)
    window.capture_tracy_button = QtWidgets.QPushButton("Capture Tracy")
    window.capture_tracy_button.clicked.connect(window.capture_tracy)
    window.bootstrap_button = QtWidgets.QPushButton("Bootstrap Check")
    window.bootstrap_button.clicked.connect(window.run_bootstrap_check)

    window.status_label = QtWidgets.QLabel("Idle")
    window.status_label.setWordWrap(True)

    window.output_text = QtWidgets.QTextBrowser()
    window.output_text.setReadOnly(True)
    window.output_text.setOpenExternalLinks(False)
    window.output_text.setLineWrapMode(QtWidgets.QTextEdit.LineWrapMode.NoWrap)
    window.log_filter_all_checkbox = QtWidgets.QCheckBox("All")
    window.log_filter_all_checkbox.setChecked(True)
    window.log_filter_all_checkbox.toggled.connect(window._handle_all_filter_toggled)
    window.log_filter_debug_checkbox = QtWidgets.QCheckBox("Debug")
    window.log_filter_debug_checkbox.toggled.connect(window._handle_level_filter_toggled)
    window.log_filter_warning_checkbox = QtWidgets.QCheckBox("Warnings")
    window.log_filter_warning_checkbox.toggled.connect(window._handle_level_filter_toggled)
    window.log_filter_error_checkbox = QtWidgets.QCheckBox("Errors")
    window.log_filter_error_checkbox.toggled.connect(window._handle_level_filter_toggled)

    controls_group = QtWidgets.QGroupBox("Build + Probe")
    controls_layout = QtWidgets.QGridLayout(controls_group)
    controls_layout.addWidget(QtWidgets.QLabel("Workspace"), 0, 0)
    controls_layout.addWidget(QtWidgets.QLabel("Preset"), 0, 1)
    controls_layout.addWidget(QtWidgets.QLabel("Arch"), 0, 2)
    controls_layout.addWidget(window.product_label, 1, 0)
    controls_layout.addWidget(window.preset_combo, 1, 1)
    controls_layout.addWidget(window.arch_combo, 1, 2)
    controls_layout.addWidget(window.configure_checkbox, 1, 3)

    button_row = QtWidgets.QHBoxLayout()
    button_row.addWidget(window.build_button)
    button_row.addWidget(window.validate_button)
    button_row.addWidget(window.start_button)
    button_row.addWidget(window.stop_button)
    button_row.addWidget(window.status_button)
    button_row.addStretch(1)
    controls_layout.addLayout(button_row, 2, 0, 1, 4)
    controls_layout.addWidget(window.status_label, 3, 0, 1, 4)
    controls_layout.setColumnStretch(0, 1)
    controls_layout.setColumnStretch(1, 1)
    controls_layout.setColumnStretch(2, 1)
    controls_layout.setColumnStretch(2, 1)

    automation_group = QtWidgets.QGroupBox("Workspace Actions")
    automation_layout = QtWidgets.QVBoxLayout(automation_group)
    window.build_all_button = QtWidgets.QPushButton("Build All Presets")
    window.build_all_button.clicked.connect(window.build_all_presets)
    automation_layout.addWidget(window.build_all_button)
    window.doctor_button = QtWidgets.QPushButton("List Presets")
    window.doctor_button.clicked.connect(window.run_build_doctor)
    automation_layout.addWidget(window.doctor_button)
    window.probe_run_button = QtWidgets.QPushButton("Build + Start Probe")
    window.probe_run_button.clicked.connect(window.start_probe_run)
    automation_layout.addWidget(window.probe_run_button)
    window.logs_button = QtWidgets.QPushButton("Open Logs Folder")
    window.logs_button.clicked.connect(window.open_logs_folder)
    automation_layout.addWidget(window.logs_button)

    tools_group = QtWidgets.QGroupBox("Debug Tools")
    tools_layout = QtWidgets.QGridLayout(tools_group)
    tools_layout.addWidget(window.launch_tracy_checkbox, 0, 0, 1, 2)
    tools_layout.addWidget(window.capture_tracy_checkbox, 1, 0, 1, 2)
    tools_layout.addWidget(QtWidgets.QLabel("Tracy seconds"), 2, 0)
    tools_layout.addWidget(window.tracy_seconds_spin, 2, 1)
    tools_layout.addWidget(window.launch_tracy_button, 3, 0)
    tools_layout.addWidget(window.capture_tracy_button, 3, 1)
    tools_layout.addWidget(window.bootstrap_button, 4, 0, 1, 2)
    tools_layout.setColumnStretch(0, 1)
    tools_layout.setColumnStretch(1, 1)

    console_group = QtWidgets.QGroupBox("Console")
    console_layout = QtWidgets.QVBoxLayout(console_group)
    filter_row = QtWidgets.QHBoxLayout()
    filter_row.addWidget(QtWidgets.QLabel("Show"))
    filter_row.addWidget(window.log_filter_all_checkbox)
    filter_row.addWidget(window.log_filter_debug_checkbox)
    filter_row.addWidget(window.log_filter_warning_checkbox)
    filter_row.addWidget(window.log_filter_error_checkbox)
    filter_row.addStretch(1)
    console_layout.addLayout(filter_row)
    console_layout.addWidget(window.output_text)

    left_panel = QtWidgets.QWidget()
    left_layout = QtWidgets.QVBoxLayout(left_panel)
    left_layout.setContentsMargins(0, 0, 0, 0)
    left_layout.addWidget(controls_group)
    left_layout.addWidget(automation_group)
    left_layout.addWidget(tools_group)
    left_layout.addStretch(1)

    splitter = QtWidgets.QSplitter(QtCore.Qt.Orientation.Horizontal)
    splitter.addWidget(left_panel)
    splitter.addWidget(console_group)
    splitter.setStretchFactor(0, 3)
    splitter.setStretchFactor(1, 2)

    root_layout = QtWidgets.QVBoxLayout(window)
    root_layout.addWidget(splitter)
