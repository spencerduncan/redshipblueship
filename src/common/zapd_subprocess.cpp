/**
 * @file zapd_subprocess.cpp
 * @brief Bundled standalone ZAPD subprocess driver for in-app extraction (issue #325)
 *
 * See zapd_subprocess.h for the rationale.
 */

#include "zapd_subprocess.h"

#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

std::string ZapdSubprocess_Locate(const std::string& assetsPath, const char* variant) {
    const std::string base = assetsPath + "/extractor/" + variant;
#ifdef _WIN32
    const std::string candidates[] = { base + ".exe" };
#else
    // ZAPDTR names the Linux/macOS executables "<variant>.out" (OUTPUT_NAME,
    // ZAPDTR/ZAPD/CMakeLists.txt); accept the bare name too in case a
    // packaging step strips the suffix.
    const std::string candidates[] = { base + ".out", base };
#endif
    for (const auto& path : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            return path;
        }
    }
    return "";
}

int ZapdSubprocess_Run(const std::string& exePath, const char* const* argv, int argc) {
#ifdef _WIN32
    // CreateProcess takes a single command line; quote each argument. Naive
    // quoting is sufficient here: the only variable arguments are filesystem
    // paths (quotes are illegal in Windows paths, and these paths do not end
    // in a backslash) and a x.y.z version string.
    std::string cmdLine = "\"" + exePath + "\"";
    for (int i = 1; i < argc; i++) {
        cmdLine += " \"";
        cmdLine += argv[i];
        cmdLine += "\"";
    }
    std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back('\0');

    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcessA(exePath.c_str(), mutableCmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        fprintf(stderr, "Failed to spawn ZAPD (%s): error %lu\n", exePath.c_str(), GetLastError());
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = (DWORD)-1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exitCode;
#else
    // Build the child argv BEFORE forking: the process is multithreaded here
    // (SDL dialog threads, logger threads), so the child may only make
    // async-signal-safe calls between fork and exec — no heap allocation.
    std::vector<char*> childArgv;
    childArgv.reserve(argc + 1);
    childArgv.push_back(const_cast<char*>(exePath.c_str()));
    for (int i = 1; i < argc; i++) {
        childArgv.push_back(const_cast<char*>(argv[i]));
    }
    childArgv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Failed to fork for ZAPD (%s)\n", exePath.c_str());
        return -1;
    }
    if (pid == 0) {
        execv(exePath.c_str(), childArgv.data());
        _exit(127);
    }
    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
#endif
}
