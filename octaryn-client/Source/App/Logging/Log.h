#pragma once

#include <stdio.h>

namespace octaryn_client_app {

extern FILE *g_log;

void open_log();
void close_log();
void log_line(const char *message);
void log_result(const char *name, int result);

} // namespace octaryn_client_app
