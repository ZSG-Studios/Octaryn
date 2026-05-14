#pragma once

#include <stdarg.h>

void oct_log_init(const char* logger_name);
void oct_log_infof(const char* format, ...);
void oct_log_warnf(const char* format, ...);
void oct_log_errorf(const char* format, ...);
void oct_log_debugf(const char* format, ...);
