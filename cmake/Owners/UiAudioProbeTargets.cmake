include_guard(GLOBAL)
octaryn_add_native_executable(octaryn_ui_audio_probe tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/UiAudioProbe/main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld/ActionSounds.cpp"
    PUBLIC_INCLUDE_DIRS
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Audio/ActionAudio"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/OpenWorld"
    PRIVATE_LINKS octaryn_client_action_audio octaryn::deps::glaze octaryn::deps::miniaudio)
