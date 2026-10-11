/**
 * @file test_scratch.cpp
 * @brief Per-process scratch directories for test rows that write files (#926)
 *
 * See test_scratch.h for the rationale.
 */

#include "test_scratch.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace rsbs::test {

namespace {

// The directories handed out since the last ScratchDirs_EndRow.
std::vector<std::filesystem::path>& RowScratchDirs() {
    static std::vector<std::filesystem::path> sDirs;
    return sDirs;
}

long ProcessId() {
#ifdef _WIN32
    return static_cast<long>(_getpid());
#else
    return static_cast<long>(getpid());
#endif
}

std::string ThisExecutablePath() {
#ifdef _WIN32
    char buf[MAX_PATH * 4] = { 0 };
    const DWORD len = GetModuleFileNameA(nullptr, buf, static_cast<DWORD>(sizeof(buf)));
    return (len > 0 && len < sizeof(buf)) ? std::string(buf, len) : std::string();
#elif defined(__APPLE__)
    char buf[4096] = { 0 };
    uint32_t size = sizeof(buf);
    return _NSGetExecutablePath(buf, &size) == 0 ? std::string(buf) : std::string();
#else
    char buf[4096] = { 0 };
    const ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    return len > 0 ? std::string(buf, static_cast<size_t>(len)) : std::string();
#endif
}

} // namespace

std::filesystem::path ScratchDir(const std::string& name) {
    std::error_code ec;
    std::filesystem::path base = std::filesystem::temp_directory_path(ec);
    if (ec) {
        base = std::filesystem::current_path(ec);
    }
    const std::filesystem::path dir = base / (name + "." + std::to_string(ProcessId()));
    auto& dirs = RowScratchDirs();
    if (std::find(dirs.begin(), dirs.end(), dir) == dirs.end()) {
        // A directory of this name can only be left over from an earlier
        // process that had the same id and failed; it is never another live
        // run's, so emptying it is safe.
        std::filesystem::remove_all(dir, ec);
        std::filesystem::create_directories(dir, ec);
        dirs.push_back(dir);
    }
    return dir;
}

void ScratchDirs_EndRow(const char* rowName, bool passed) {
    auto& dirs = RowScratchDirs();
    for (const auto& dir : dirs) {
        if (passed) {
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        } else {
            printf("[TestScratch] %s did not pass: kept %s\n", rowName ? rowName : "(row)", dir.string().c_str());
        }
    }
    dirs.clear();
}

std::vector<int> RunRowConcurrently(const char* row, int copies, const std::filesystem::path& logDir) {
    std::vector<int> codes(static_cast<size_t>(std::max(copies, 0)), -1);
    const std::string exe = ThisExecutablePath();
    if (exe.empty()) {
        printf("[TestScratch] could not resolve this executable's path\n");
        return codes;
    }
    std::vector<std::string> logs;
    for (int i = 0; i < copies; i++) {
        logs.push_back((logDir / (std::string(row) + "." + std::to_string(i) + ".log")).string());
    }
    fflush(nullptr); // nothing buffered in this process may be duplicated into a child

#ifdef _WIN32
    std::vector<HANDLE> procs(codes.size(), nullptr);
    for (int i = 0; i < copies; i++) {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
        HANDLE log = CreateFileA(logs[i].c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (log == INVALID_HANDLE_VALUE) {
            printf("[TestScratch] could not create %s: error %lu\n", logs[i].c_str(), GetLastError());
            continue;
        }
        std::string cmdLine = "\"" + exe + "\" --test " + row;
        std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
        mutableCmd.push_back('\0');
        STARTUPINFOA si = { 0 };
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = nullptr;
        si.hStdOutput = log;
        si.hStdError = log;
        PROCESS_INFORMATION pi = { 0 };
        // Each log handle is created, inherited by its one child and closed
        // before the next is created, so no child holds another's log.
        // CREATE_NO_WINDOW: never a console window, so never a focus change.
        if (CreateProcessA(exe.c_str(), mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                           nullptr, &si, &pi)) {
            procs[i] = pi.hProcess;
            CloseHandle(pi.hThread);
        } else {
            printf("[TestScratch] could not spawn %s: error %lu\n", exe.c_str(), GetLastError());
        }
        CloseHandle(log);
    }
    for (size_t i = 0; i < procs.size(); i++) {
        if (procs[i] == nullptr) {
            continue;
        }
        WaitForSingleObject(procs[i], INFINITE);
        DWORD exitCode = static_cast<DWORD>(-1);
        GetExitCodeProcess(procs[i], &exitCode);
        CloseHandle(procs[i]);
        codes[i] = static_cast<int>(exitCode);
    }
#else
    // Everything the child touches between fork and exec is built here: the
    // process is multithreaded, so the child may only make async-signal-safe
    // calls (open, dup2, execv, _exit).
    std::vector<char*> argv = { const_cast<char*>(exe.c_str()), const_cast<char*>("--test"), const_cast<char*>(row),
                                nullptr };
    std::vector<pid_t> pids(codes.size(), -1);
    for (int i = 0; i < copies; i++) {
        const char* logPath = logs[i].c_str();
        const pid_t pid = fork();
        if (pid == 0) {
            const int fd = open(logPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                dup2(fd, 1);
                dup2(fd, 2);
                close(fd);
            }
            execv(exe.c_str(), argv.data());
            _exit(127);
        }
        if (pid < 0) {
            printf("[TestScratch] could not fork for %s\n", row);
        }
        pids[i] = pid;
    }
    for (size_t i = 0; i < pids.size(); i++) {
        if (pids[i] <= 0) {
            continue;
        }
        int status = 0;
        pid_t waited;
        do {
            waited = waitpid(pids[i], &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited == pids[i] && WIFEXITED(status)) {
            codes[i] = WEXITSTATUS(status);
        }
    }
#endif
    return codes;
}

} // namespace rsbs::test
