from client_app_launch_probe_log.parsing import parse_named_int


def validate_mesh_audit(log_file, lines, errors):
    section_failures = [
        line for line in lines
        if line.startswith("live_world_mesh_column_audit ")
        and ((parse_named_int(line, "surface_section_missing") or 0) > 0
             or (parse_named_int(line, "surface_section_not_drawn") or 0) > 0)
    ]
    if section_failures:
        errors.append(
            f"{log_file}: visible terrain surface section disappeared or was "
            f"not drawn, examples={section_failures[:3]}"
        )
    coverage_failures = [
        line for line in lines
        if line.startswith("live_terrain_mesh_surface_coverage ")
        and (parse_named_int(line, "missing") or 0) > 0
    ]
    if coverage_failures:
        errors.append(
            f"{log_file}: terrain mesh build left uncovered surface cells, "
            f"examples={coverage_failures[:3]}"
        )
