include_guard(GLOBAL)
octaryn_owner_build_root(item_visual_root basegame)
set(item_visual_dir "${item_visual_root}/assets/Items")
set(item_visual_generator "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Tools/Items/build_visuals.py")
set(item_visual_catalog "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Data/Items/render.json")
set(item_visual_outputs)
foreach(item coin apple torch pebble)
    list(APPEND item_visual_outputs "${item_visual_dir}/${item}.glb")
endforeach()
add_custom_command(OUTPUT ${item_visual_outputs}
    COMMAND "${Python3_EXECUTABLE}" "${item_visual_generator}" "${item_visual_dir}"
    DEPENDS "${item_visual_generator}" "${item_visual_catalog}"
    VERBATIM)
add_custom_target(octaryn_item_visuals DEPENDS ${item_visual_outputs})
