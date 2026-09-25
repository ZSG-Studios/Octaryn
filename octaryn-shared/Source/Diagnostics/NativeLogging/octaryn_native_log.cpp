#include "octaryn_native_log.h"

#include <array>
#include <cstdio>
#include <mutex>
#include <string>

#ifdef OCTARYN_NATIVE_LOGGING_USE_SPDLOG
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#endif

namespace {

constexpr std::size_t LogBufferSize = 2048;
std::once_flag log_init_once;

void init_default_logger(const char* logger_name)
{
#ifdef OCTARYN_NATIVE_LOGGING_USE_SPDLOG
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
#else
    (void)logger_name;
#endif
}

auto format_message(const char* format, va_list args) -> std::string
{
    std::array<char, LogBufferSize> buffer{};
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

void log_with_level(
#ifdef OCTARYN_NATIVE_LOGGING_USE_SPDLOG
    spdlog::level::level_enum level,
#else
    const char* level,
#endif
    const char* format,
    va_list args)
{
    octaryn_native_log_init(nullptr);
#ifdef OCTARYN_NATIVE_LOGGING_USE_SPDLOG
    spdlog::log(level, "{}", format_message(format, args));
#else
    std::fprintf(stderr, "[%s] %s\n", level, format_message(format, args).c_str());
#endif
}

} // namespace

void octaryn_native_log_init(const char* logger_name)
{
    std::call_once(log_init_once, init_default_logger, logger_name);
}

void octaryn_native_log_infof(const char* format, ...)
{
    va_list args;
    va_start(args, format);
#ifdef OCTARYN_NATIVE_LOGGING_USE_SPDLOG
    log_with_level(spdlog::level::info, format, args);
#else
    log_with_level("info", format, args);
#endif
    va_end(args);
}

void octaryn_native_log_warnf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
#ifdef OCTARYN_NATIVE_LOGGING_USE_SPDLOG
    log_with_level(spdlog::level::warn, format, args);
#else
    log_with_level("warn", format, args);
#endif
    va_end(args);
}

void octaryn_native_log_errorf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
#ifdef OCTARYN_NATIVE_LOGGING_USE_SPDLOG
    log_with_level(spdlog::level::err, format, args);
#else
    log_with_level("error", format, args);
#endif
    va_end(args);
}

void octaryn_native_log_debugf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
#ifdef OCTARYN_NATIVE_LOGGING_USE_SPDLOG
    log_with_level(spdlog::level::debug, format, args);
#else
    log_with_level("debug", format, args);
#endif
    va_end(args);
}
