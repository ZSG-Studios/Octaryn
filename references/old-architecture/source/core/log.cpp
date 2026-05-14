#include "core/log.h"

#include <array>
#include <cstdio>
#include <mutex>
#include <string>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace {

constexpr std::size_t k_log_buffer_size = 2048;
std::once_flag g_log_init_once;

void init_default_logger(const char* logger_name)
{
    const std::string name = logger_name && *logger_name ? logger_name : "octaryn";
    auto logger = spdlog::stdout_color_mt(name);
    logger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
    spdlog::set_default_logger(logger);
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif
    spdlog::flush_on(spdlog::level::info);
}

auto format_message(const char* format, va_list args) -> std::string
{
    std::array<char, k_log_buffer_size> buffer{};
    va_list args_copy;
    va_copy(args_copy, args);
    const int written = std::vsnprintf(buffer.data(), buffer.size(), format, args_copy);
    va_end(args_copy);
    if (written < 0)
    {
        return "log formatting error";
    }
    if (static_cast<std::size_t>(written) < buffer.size())
    {
        return std::string(buffer.data(), static_cast<std::size_t>(written));
    }
    std::string dynamic(static_cast<std::size_t>(written) + 1u, '\0');
    std::vsnprintf(dynamic.data(), dynamic.size(), format, args);
    dynamic.resize(static_cast<std::size_t>(written));
    return dynamic;
}

void log_with_level(spdlog::level::level_enum level, const char* format, va_list args)
{
    oct_log_init(nullptr);
    spdlog::log(level, "{}", format_message(format, args));
}

} // namespace

void oct_log_init(const char* logger_name)
{
    std::call_once(g_log_init_once, init_default_logger, logger_name);
}

void oct_log_infof(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_with_level(spdlog::level::info, format, args);
    va_end(args);
}

void oct_log_warnf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_with_level(spdlog::level::warn, format, args);
    va_end(args);
}

void oct_log_errorf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_with_level(spdlog::level::err, format, args);
    va_end(args);
}

void oct_log_debugf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_with_level(spdlog::level::debug, format, args);
    va_end(args);
}
