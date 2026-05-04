#include "FunctionProfile.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct function_profile_config
{
    bool configured = false;
    bool enabled = false;
    uint64_t log_every_frames = 60u;
    double minimum_ms = 0.0;
    FILE* log = nullptr;
    std::vector<std::string> enabled_blocks;
    std::vector<std::string> disabled_blocks;
};

function_profile_config g_config;

std::string trim_copy(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }).base();
    if (first >= last)
    {
        return {};
    }

    return std::string(first, last);
}

std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool enabled_value(const std::string& value)
{
    const std::string lowered = lower_copy(trim_copy(value));
    return lowered == "1" || lowered == "true" || lowered == "yes" ||
        lowered == "on" || lowered == "enabled";
}

void parse_block_list(const char* value, std::vector<std::string>& output)
{
    output.clear();
    if (value == nullptr || value[0] == '\0')
    {
        return;
    }

    std::string token;
    for (const char* cursor = value; *cursor != '\0'; ++cursor)
    {
        const char ch = *cursor;
        if (ch == ',' || ch == ';' || std::isspace(static_cast<unsigned char>(ch)) != 0)
        {
            token = trim_copy(token);
            if (!token.empty())
            {
                output.push_back(token);
            }
            token.clear();
            continue;
        }

        token.push_back(ch);
    }

    token = trim_copy(token);
    if (!token.empty())
    {
        output.push_back(token);
    }
}

bool list_contains(const std::vector<std::string>& blocks, const char* block_name)
{
    if (block_name == nullptr)
    {
        return false;
    }

    return std::any_of(blocks.begin(), blocks.end(), [&](const std::string& block) {
        return block == "all" || block == block_name;
    });
}

void apply_uint64(uint64_t& target, const std::string& value)
{
    char* end = nullptr;
    const unsigned long long parsed =
        std::strtoull(trim_copy(value).c_str(), &end, 10);
    if (end != nullptr && *end == '\0')
    {
        target = static_cast<uint64_t>(parsed);
    }
}

void apply_double(double& target, const std::string& value)
{
    char* end = nullptr;
    const double parsed = std::strtod(trim_copy(value).c_str(), &end);
    if (end != nullptr && *end == '\0')
    {
        target = std::max(0.0, parsed);
    }
}

void apply_config_pair(const std::string& key, const std::string& value)
{
    const std::string lowered = lower_copy(trim_copy(key));
    if (lowered == "enabled")
    {
        g_config.enabled = enabled_value(value);
    }
    else if (lowered == "logeveryframes" || lowered == "log_every_frames")
    {
        apply_uint64(g_config.log_every_frames, value);
    }
    else if (lowered == "minimummilliseconds" || lowered == "minimum_ms")
    {
        apply_double(g_config.minimum_ms, value);
    }
    else if (lowered == "enabledblocks" || lowered == "enabled_blocks")
    {
        parse_block_list(value.c_str(), g_config.enabled_blocks);
    }
    else if (lowered == "disabledblocks" || lowered == "disabled_blocks")
    {
        parse_block_list(value.c_str(), g_config.disabled_blocks);
    }
}

void load_config_file(const char* config_path)
{
    if (config_path == nullptr || config_path[0] == '\0' ||
        !std::filesystem::exists(config_path))
    {
        return;
    }

    std::ifstream input(config_path);
    std::string line;
    while (std::getline(input, line))
    {
        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed[0] == '#')
        {
            continue;
        }

        const size_t split = trimmed.find('=');
        if (split == std::string::npos)
        {
            continue;
        }

        apply_config_pair(trimmed.substr(0, split), trimmed.substr(split + 1u));
    }
}

void apply_environment_overrides()
{
    const char* enabled = std::getenv("OCTARYN_CLIENT_PROFILE_ENABLED");
    if (enabled != nullptr)
    {
        g_config.enabled = enabled_value(enabled);
    }

    const char* every = std::getenv("OCTARYN_CLIENT_PROFILE_LOG_EVERY_FRAMES");
    if (every != nullptr)
    {
        apply_uint64(g_config.log_every_frames, every);
    }

    const char* minimum = std::getenv("OCTARYN_CLIENT_PROFILE_MIN_MS");
    if (minimum != nullptr)
    {
        apply_double(g_config.minimum_ms, minimum);
    }

    parse_block_list(std::getenv("OCTARYN_CLIENT_PROFILE_ENABLED_BLOCKS"),
        g_config.enabled_blocks);
    parse_block_list(std::getenv("OCTARYN_CLIENT_PROFILE_DISABLED_BLOCKS"),
        g_config.disabled_blocks);
}

double elapsed_ms(uint64_t start_ticks, uint64_t end_ticks)
{
    return end_ticks >= start_ticks
        ? static_cast<double>(end_ticks - start_ticks) * 1.0e-6
        : 0.0;
}

} // namespace

void function_profile_configure(FILE* log)
{
    g_config = {};
    g_config.configured = true;
    g_config.log = log;

    load_config_file(std::getenv("OCTARYN_CLIENT_PROFILE_CONFIG_PATH"));
    apply_environment_overrides();
    if (g_config.log_every_frames == 0u)
    {
        g_config.log_every_frames = 1u;
    }
    if (g_config.enabled && g_config.enabled_blocks.empty())
    {
        g_config.enabled_blocks.push_back("all");
    }

    if (g_config.log != nullptr)
    {
        std::fprintf(g_config.log,
            "client_function_profile_config enabled=%d every=%" PRIu64
            " minimum_ms=%.3f enabled_blocks=%zu disabled_blocks=%zu\n",
            g_config.enabled ? 1 : 0,
            g_config.log_every_frames,
            g_config.minimum_ms,
            g_config.enabled_blocks.size(),
            g_config.disabled_blocks.size());
        std::fflush(g_config.log);
    }
}

int function_profile_enabled(const char* block_name)
{
    if (!g_config.configured)
    {
        function_profile_configure(nullptr);
    }
    if (!g_config.enabled || list_contains(g_config.disabled_blocks, block_name))
    {
        return 0;
    }
    if (g_config.enabled_blocks.empty())
    {
        return 0;
    }
    return list_contains(g_config.enabled_blocks, block_name) ? 1 : 0;
}

function_profile_scope::function_profile_scope(
    const char* block_name,
    uint64_t frame_index,
    const char* detail)
    : block_name_(block_name),
      detail_(detail),
      frame_index_(frame_index),
      start_ticks_(0u),
      active_(0)
{
    if (block_name == nullptr || function_profile_enabled(block_name) == 0)
    {
        return;
    }
    if (g_config.log_every_frames > 1u &&
        frame_index % g_config.log_every_frames != 0u)
    {
        return;
    }

    start_ticks_ = SDL_GetTicksNS();
    active_ = 1;
}

function_profile_scope::~function_profile_scope()
{
    if (active_ == 0 || g_config.log == nullptr)
    {
        return;
    }

    const double ms = elapsed_ms(start_ticks_, SDL_GetTicksNS());
    if (ms < g_config.minimum_ms)
    {
        return;
    }

    std::fprintf(g_config.log,
        "client_function_profile block=%s frame=%" PRIu64 " ms=%.3f",
        block_name_ != nullptr ? block_name_ : "unknown",
        frame_index_,
        ms);
    if (detail_ != nullptr && detail_[0] != '\0')
    {
        std::fprintf(g_config.log, " detail=%s", detail_);
    }
    std::fprintf(g_config.log, "\n");
    std::fflush(g_config.log);
}
