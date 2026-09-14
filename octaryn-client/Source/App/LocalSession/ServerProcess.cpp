#include "ServerProcess.h"
#include <map>
#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace octaryn::client::app::local_session {
struct ServerProcess::Handles {
#if defined(_WIN32)
  HANDLE process{}, job{};
#else
  mutable pid_t process{-1};
#endif
};
ServerProcess::ServerProcess() : handles_(std::make_unique<Handles>()) {}
ServerProcess::~ServerProcess() { terminate(); }

#if defined(_WIN32)
namespace {
std::wstring widen(const std::string& text) {
  const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring result(static_cast<size_t>(count), L'\0');
  if (count) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), count);
  return result;
}
struct EnvironmentOrder {
  bool operator()(const std::wstring& a, const std::wstring& b) const { return _wcsicmp(a.c_str(), b.c_str()) < 0; }
};
}
#endif

bool ServerProcess::start(const std::filesystem::path& executable, const std::filesystem::path& log,
    const std::vector<std::pair<std::string, std::string>>& overrides) {
  terminate();
#if defined(_WIN32)
  std::map<std::wstring, std::wstring, EnvironmentOrder> environment;
  auto* inherited = GetEnvironmentStringsW();
  if (!inherited) return false;
  for (const wchar_t* entry = inherited; *entry; entry += wcslen(entry) + 1) {
    const std::wstring value(entry);
    const auto separator = value.find(L'=', 1);
    if (separator != std::wstring::npos) environment[value.substr(0, separator)] = value.substr(separator + 1);
  }
  FreeEnvironmentStringsW(inherited);
  for (const auto& [key, value] : overrides) environment[widen(key)] = widen(value);
  std::vector<wchar_t> block;
  for (const auto& [key, value] : environment) {
    const auto entry = key + L"=" + value;
    block.insert(block.end(), entry.begin(), entry.end());
    block.push_back(L'\0');
  }
  block.push_back(L'\0');
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  HANDLE output = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, 0, nullptr);
  HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
  if (output == INVALID_HANDLE_VALUE || input == INVALID_HANDLE_VALUE) {
    if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    return false;
  }
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  startup.wShowWindow = SW_HIDE;
  startup.hStdOutput = output;
  startup.hStdError = output;
  startup.hStdInput = input;
  handles_->job = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  bool ok = handles_->job && SetInformationJobObject(handles_->job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
  PROCESS_INFORMATION process{};
  std::wstring command = L"\"" + executable.wstring() + L"\"";
  if (ok) ok = CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
      CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, block.data(),
      executable.parent_path().c_str(), &startup, &process) != 0;
  CloseHandle(output);
  CloseHandle(input);
  if (!ok) { terminate(); return false; }
  handles_->process = process.hProcess;
  if (!AssignProcessToJobObject(handles_->job, handles_->process)) {
    TerminateProcess(handles_->process, 1);
    CloseHandle(process.hThread);
    terminate();
    return false;
  }
  ok = ResumeThread(process.hThread) != static_cast<DWORD>(-1);
  CloseHandle(process.hThread);
  if (!ok) terminate();
  return ok;
#else
  const pid_t child = fork();
  if (child < 0) return false;
  if (child == 0) {
    const int output = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (output < 0 || chdir(executable.parent_path().c_str()) != 0) _exit(126);
    dup2(output, STDOUT_FILENO);
    dup2(output, STDERR_FILENO);
    close(output);
    for (const auto& [key, value] : overrides) setenv(key.c_str(), value.c_str(), 1);
    execl(executable.c_str(), executable.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }
  handles_->process = child;
  return true;
#endif
}

bool ServerProcess::running() const {
#if defined(_WIN32)
  return handles_->process && WaitForSingleObject(handles_->process, 0) == WAIT_TIMEOUT;
#else
  if (handles_->process < 0) return false;
  if (waitpid(handles_->process, nullptr, WNOHANG) == 0) return true;
  handles_->process = -1;
  return false;
#endif
}

void ServerProcess::terminate() {
#if defined(_WIN32)
  if (handles_->job) { CloseHandle(handles_->job); handles_->job = nullptr; }
  if (handles_->process) { WaitForSingleObject(handles_->process, 1500); CloseHandle(handles_->process); handles_->process = nullptr; }
#else
  if (handles_->process > 0) {
    kill(handles_->process, SIGKILL);
    waitpid(handles_->process, nullptr, 0);
    handles_->process = -1;
  }
#endif
}
}
