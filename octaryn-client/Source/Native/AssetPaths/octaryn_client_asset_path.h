#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool octaryn_client_asset_path_build(char* output, size_t output_size, const char* relative_path);
bool octaryn_client_bundle_path_build(char* output, size_t output_size, const char* relative_path);

#ifdef __cplusplus
}
#endif
