#pragma once

#include <stdint.h>
#include <stdio.h>

void octaryn_client_function_profile_configure(FILE* log);
int octaryn_client_function_profile_enabled(const char* block_name);

class octaryn_client_function_profile_scope
{
public:
    octaryn_client_function_profile_scope(const char* block_name,
        uint64_t frame_index,
        const char* detail);
    ~octaryn_client_function_profile_scope();

    octaryn_client_function_profile_scope(
        const octaryn_client_function_profile_scope&) = delete;
    octaryn_client_function_profile_scope& operator=(
        const octaryn_client_function_profile_scope&) = delete;

private:
    const char* block_name_;
    const char* detail_;
    uint64_t frame_index_;
    uint64_t start_ticks_;
    int active_;
};
