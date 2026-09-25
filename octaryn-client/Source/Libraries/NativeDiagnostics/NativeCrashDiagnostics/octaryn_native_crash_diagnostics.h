#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void octaryn_native_crash_diagnostics_init(const char* logger_name);
const char* octaryn_native_crash_diagnostics_marker_path(void);

#ifdef __cplusplus
}
#endif
