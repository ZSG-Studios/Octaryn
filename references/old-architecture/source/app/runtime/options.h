#pragma once

#include <string>

struct app_runtime_probe_options_t
{
    std::string color_path;
    std::string composite_path;
    bool require_visible_frame = false;
    bool visible_frame_verified = false;
    bool visible_frame_failed = false;
    bool color_written = false;
    bool color_failed = false;
    bool composite_written = false;
    bool composite_failed = false;
};

struct app_runtime_options_t
{
    std::string save_path;
    app_runtime_probe_options_t probes;
};

bool app_runtime_options_parse(app_runtime_options_t* options, int argc, char** argv);
bool app_runtime_probes_requested(const app_runtime_probe_options_t* probes);
