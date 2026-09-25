#pragma once

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void octaryn_native_log_init(const char* logger_name);
void octaryn_native_log_infof(const char* format, ...);
void octaryn_native_log_warnf(const char* format, ...);
void octaryn_native_log_errorf(const char* format, ...);
void octaryn_native_log_debugf(const char* format, ...);

#ifdef __cplusplus
}
#endif
