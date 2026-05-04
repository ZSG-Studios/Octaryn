#pragma once

#include <stdint.h>
#include <stdio.h>

void function_profile_configure(FILE* log);
int function_profile_enabled(const char* block_name);

class function_profile_scope
{
public:
    function_profile_scope(const char* block_name,
        uint64_t frame_index,
        const char* detail);
    ~function_profile_scope();

    function_profile_scope(
        const function_profile_scope&) = delete;
    function_profile_scope& operator=(
        const function_profile_scope&) = delete;

private:
    const char* block_name_;
    const char* detail_;
    uint64_t frame_index_;
    uint64_t start_ticks_;
    int active_;
};
