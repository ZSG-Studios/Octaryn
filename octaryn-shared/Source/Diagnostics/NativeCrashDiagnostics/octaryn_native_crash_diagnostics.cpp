#include "octaryn_native_crash_diagnostics.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#if defined(OCTARYN_NATIVE_DIAGNOSTICS_USE_CPPTRACE)
#include <cpptrace/cpptrace.hpp>
#define OCTARYN_NATIVE_HAS_CPPTRACE 1
#else
#define OCTARYN_NATIVE_HAS_CPPTRACE 0
#endif

#include "octaryn_native_log.h"

namespace {

constexpr std::size_t CrashMarkerPathSize = 256;
char crash_marker_path[CrashMarkerPathSize] = {};

void write_marker_bytes(const char* message, std::size_t message_size)
{
#if defined(_WIN32)
    DWORD bytes_written = 0;
    HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
    if (stderr_handle != INVALID_HANDLE_VALUE && stderr_handle != nullptr)
    {
        (void)WriteFile(stderr_handle, message, static_cast<DWORD>(message_size), &bytes_written, nullptr);
    }

    if (crash_marker_path[0] == '\0')
    {
        return;
    }

    HANDLE marker_file = CreateFileA(
        crash_marker_path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (marker_file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    bytes_written = 0;
    (void)WriteFile(marker_file, message, static_cast<DWORD>(message_size), &bytes_written, nullptr);
    CloseHandle(marker_file);
#else
    (void)write(STDERR_FILENO, message, message_size);

    if (crash_marker_path[0] == '\0')
    {
        return;
    }

    const int fd = open(crash_marker_path, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 0644);
    if (fd < 0)
    {
        return;
    }

    (void)write(fd, message, message_size);
    (void)close(fd);
#endif
}

#if defined(_WIN32)
LONG WINAPI windows_crash_handler(EXCEPTION_POINTERS* exception_info)
{
    const unsigned long exception_code =
        (exception_info && exception_info->ExceptionRecord)
            ? static_cast<unsigned long>(exception_info->ExceptionRecord->ExceptionCode)
            : 0ul;
    char message[96] = {};
    const int message_size = std::snprintf(
        message,
        sizeof(message),
        "0x%08lx fatal exception caught by Octaryn\n",
        exception_code);
    if (message_size > 0)
    {
        write_marker_bytes(
            message,
            static_cast<std::size_t>(message_size) < sizeof(message)
                ? static_cast<std::size_t>(message_size)
                : sizeof(message) - 1u);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}
#else
void append_signal_number(char* message, std::size_t message_size, int sig)
{
    if (!message || message_size < 4)
    {
        return;
    }

    int value = sig;
    for (int index = 2; index >= 0; --index)
    {
        message[index] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
}

void write_crash_marker(int sig)
{
    char message[] = "000 fatal signal caught by Octaryn\n";
    append_signal_number(message, sizeof(message), sig);
    write_marker_bytes(message, sizeof(message) - 1u);
}

void signal_crash_handler(int sig)
{
    write_crash_marker(sig);
    _exit(128 + sig);
}
#endif

void configure_crash_marker_path()
{
    const char* override_path = std::getenv("OCTARYN_CRASH_MARKER_PATH");
    if (override_path && override_path[0] != '\0')
    {
        std::snprintf(crash_marker_path, sizeof(crash_marker_path), "%s", override_path);
        return;
    }

#if defined(_WIN32)
    char temp_path[MAX_PATH] = {};
    const DWORD temp_path_size = GetTempPathA(static_cast<DWORD>(sizeof(temp_path)), temp_path);
    const char* base_path = (temp_path_size > 0 && temp_path_size < sizeof(temp_path)) ? temp_path : ".\\";
    std::snprintf(
        crash_marker_path,
        sizeof(crash_marker_path),
        "%soctaryn-crash-%lu.marker",
        base_path,
        static_cast<unsigned long>(GetCurrentProcessId()));
#else
    std::snprintf(
        crash_marker_path,
        sizeof(crash_marker_path),
        "/tmp/octaryn-crash-%ld.marker",
        static_cast<long>(getpid()));
#endif
}

#if !defined(_WIN32)
void install_signal_handler(int sig)
{
    struct sigaction action = {};
    action.sa_handler = signal_crash_handler;
    sigemptyset(&action.sa_mask);
    sigaction(sig, &action, nullptr);
}
#endif

void install_signal_handlers()
{
#if defined(_WIN32)
    SetUnhandledExceptionFilter(windows_crash_handler);
#else
    install_signal_handler(SIGSEGV);
    install_signal_handler(SIGABRT);
    install_signal_handler(SIGILL);
    install_signal_handler(SIGFPE);
    install_signal_handler(SIGBUS);
#endif
}

} // namespace

void octaryn_native_crash_diagnostics_init(const char* logger_name)
{
    static std::once_flag once;
    std::call_once(once, [logger_name]() {
        octaryn_native_log_init(logger_name);
        configure_crash_marker_path();
        install_signal_handlers();
#if OCTARYN_NATIVE_HAS_CPPTRACE
        cpptrace::register_terminate_handler();
        octaryn_native_log_infof(
            "Crash diagnostics initialized with cpptrace terminate handling and marker %s",
            crash_marker_path);
#else
        octaryn_native_log_infof(
            "Crash diagnostics initialized with marker %s; cpptrace support is disabled",
            crash_marker_path);
#endif
    });
}

const char* octaryn_native_crash_diagnostics_marker_path(void)
{
    return crash_marker_path;
}
