/**
 * @file headless_crash.cpp
 * @brief Unattended-safe crash handling for the single executable (issue #388)
 *
 * See headless_crash.h for the rationale. The short version: libultraship's
 * crash handlers end in a modal dialog, and a modal dialog on a runner is an
 * infinite loop. Here a fatal fault instead produces a few lines on stderr and
 * an immediate non-zero exit.
 *
 * Everything on the fault path is written with raw write(2)/_write, not
 * printf: stdio locks can already be held by the thread we just interrupted,
 * and a crash handler that deadlocks on a mutex is the same 180-second
 * timeout by a different route.
 */

#include "headless_crash.h"

// C headers, not their <c*> spellings: the fault path uses size_t/uintptr_t
// unqualified, and only the C forms are guaranteed to declare them in the
// global namespace.
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ship/Context.h>

#include "integration_test_hooks.h"
#include "test_runner.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <ship/debug/CrashHandler.h>
#if defined(_MSC_VER)
// GetUserObjectInformationW (window-station visibility probe) lives in user32.
#pragma comment(lib, "user32.lib")
#endif
#else
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace {

// --------------------------------------------------------------------------
// Async-signal-safe output primitives
// --------------------------------------------------------------------------

void RawWrite(const char* str, size_t len) {
#ifdef _WIN32
    _write(2, str, (unsigned int)len);
#else
    // Deliberately ignoring the result: there is nothing useful to do about a
    // short write from inside a crash handler, and we are about to _exit.
    ssize_t ignored = write(2, str, len);
    (void)ignored;
#endif
}

void RawWrite(const char* str) {
    RawWrite(str, strlen(str));
}

// Unsigned -> text without snprintf, which is not async-signal-safe.
void RawWriteNum(unsigned long long value, int base) {
    char buf[24];
    int i = (int)sizeof(buf);
    buf[--i] = '\0';
    if (value == 0) {
        buf[--i] = '0';
    }
    while (value != 0 && i > 0) {
        unsigned digit = (unsigned)(value % (unsigned)base);
        buf[--i] = (char)(digit < 10 ? '0' + digit : 'a' + (digit - 10));
        value /= (unsigned)base;
    }
    RawWrite(&buf[i]);
}

// --------------------------------------------------------------------------
// Headless detection
// --------------------------------------------------------------------------

bool EnvSet(const char* name) {
    const char* value = getenv(name);
    return value != nullptr && value[0] != '\0';
}

bool sForcedHeadless = false;

bool DetectHeadless() {
    // Explicit override wins in both directions. RSBS_HEADLESS=0 is the escape
    // hatch for a desktop user whose environment trips one of the heuristics.
    const char* forced = getenv("RSBS_HEADLESS");
    if (forced != nullptr && forced[0] != '\0') {
        return strcmp(forced, "0") != 0;
    }

    // An entry point that knows it is unattended (the unit-test harness) said
    // so explicitly.
    if (sForcedHeadless) {
        return true;
    }

    // Any test mode is unattended by construction — a test that waits for a
    // human is a test that times out.
    if (TestRunner_IsIntegrationTestMode()) {
        return true;
    }

    if (EnvSet("CI") || EnvSet("GITHUB_ACTIONS")) {
        return true;
    }

#ifdef _WIN32
    // The authoritative Windows question is not "is stderr a tty" (CI
    // redirects it) but "does this process have a desktop to draw on".
    // A non-visible window station — a service, or a session-0 job — makes
    // MessageBoxA unanswerable.
    HWINSTA station = GetProcessWindowStation();
    USEROBJECTFLAGS flags;
    DWORD needed = 0;
    if (station != nullptr &&
        GetUserObjectInformationW(station, UOI_FLAGS, &flags, (DWORD)sizeof(flags), &needed)) {
        if ((flags.dwFlags & WSF_VISIBLE) == 0) {
            return true;
        }
    }
    return false;
#else
    // No display server at all: SDL cannot open a window, and its message box
    // fallback is what blocks in an X11 poll().
    return !EnvSet("DISPLAY") && !EnvSet("WAYLAND_DISPLAY");
#endif
}

// --------------------------------------------------------------------------
// Fault handlers
// --------------------------------------------------------------------------

#ifndef _WIN32

const char* SignalName(int sig) {
    switch (sig) {
        case SIGSEGV:
            return "SIGSEGV (invalid access to storage)";
        case SIGBUS:
            return "SIGBUS (bad memory access)";
        case SIGILL:
            return "SIGILL (illegal instruction)";
        case SIGFPE:
            return "SIGFPE (erroneous arithmetic operation)";
        case SIGABRT:
            return "SIGABRT (abort)";
        default:
            return "unknown fatal signal";
    }
}

void HeadlessSignalHandler(int sig, siginfo_t* info, void* /*ucontext*/) {
    // Re-arm as SIG_DFL first. If anything below faults a second time the
    // process dies on the spot rather than recursing through this handler.
    signal(sig, SIG_DFL);

    RawWrite("\n[CRASH] ");
    RawWrite(SignalName(sig));
    RawWrite(" (signal ");
    RawWriteNum((unsigned long long)sig, 10);
    RawWrite(")\n");

    if (info != nullptr && (sig == SIGSEGV || sig == SIGBUS)) {
        RawWrite("[CRASH] fault address: 0x");
        RawWriteNum((unsigned long long)(uintptr_t)info->si_addr, 16);
        // A small faulting address is a near-null dereference, and its value
        // is the struct field offset — worth reading literally.
        RawWrite("\n");
    }

    // backtrace_symbols_fd is the async-signal-safe member of the backtrace
    // family: it formats into the fd without allocating, unlike
    // backtrace_symbols. Symbols are mangled, but the frame addresses alone
    // resolve through addr2line, and that is infinitely more than a timeout
    // gives us today.
    void* frames[64];
    int count = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
    RawWrite("[CRASH] traceback (");
    RawWriteNum((unsigned long long)count, 10);
    RawWrite(" frames):\n");
    backtrace_symbols_fd(frames, count, 2);

    // Best effort, and deliberately last: this touches game state and is not
    // async-signal-safe, so it runs only after the breadcrumbs above have
    // already escaped to stderr.
    if (IntegrationTest_GetMode() == INT_TEST_GAMEPLAY_ROUNDTRIP) {
        IntegrationTest_LogGameplayState("signal");
    }

    RawWrite("[CRASH] exiting immediately (headless session; no crash dialog)\n");

    // 128 + signal is the convention the integration tier documents
    // (docs/ci-gameplay-repro-postmortem.md 7) so CI can tell "crashed" from
    // "test failed". _exit, not exit: static destructors on a corrupted heap
    // are how a crash turns back into a hang.
    _exit(128 + sig);
}

void InstallHandlers() {
    // Warm up backtrace() now. Its first call dlopens libgcc and allocates;
    // doing that lazily from inside a SIGSEGV handler can deadlock on the
    // loader lock.
    void* warmup[4];
    (void)backtrace(warmup, 4);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = HeadlessSignalHandler;
    sigemptyset(&action.sa_mask);

    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGABRT, &action, nullptr);
}

#else // _WIN32

// Map the fatal Windows exception codes onto the POSIX signal numbers the
// integration tier documents, so a Windows crash reads the same way a Linux
// one does (exit >= 128 means crashed, signal = code - 128).
int MapExceptionToSignal(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_STACK_OVERFLOW:
            return 11; // SIGSEGV
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_PRIV_INSTRUCTION:
            return 4; // SIGILL
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_OVERFLOW:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_INVALID_OPERATION:
        case EXCEPTION_FLT_OVERFLOW:
        case EXCEPTION_FLT_UNDERFLOW:
        case EXCEPTION_FLT_INEXACT_RESULT:
        case EXCEPTION_FLT_DENORMAL_OPERAND:
        case EXCEPTION_FLT_STACK_CHECK:
            return 8; // SIGFPE
        default:
            return 6; // SIGABRT-equivalent bucket
    }
}

LONG WINAPI HeadlessExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
    const DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;

    RawWrite("\n[CRASH] Windows exception: 0x");
    RawWriteNum((unsigned long long)code, 16);
    RawWrite("\n");

    // ASLR-independent fault location: an exe-relative offset survives across
    // runs and resolves to a function through the linker map (redship.map)
    // even without PDBs.
    {
        uintptr_t rip = (uintptr_t)exceptionInfo->ExceptionRecord->ExceptionAddress;
        uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
        RawWrite("[CRASH] addr=0x");
        RawWriteNum((unsigned long long)rip, 16);
        RawWrite(" exe_base=0x");
        RawWriteNum((unsigned long long)base, 16);
        RawWrite(" exe_offset=0x");
        RawWriteNum((unsigned long long)(rip >= base ? rip - base : rip), 16);
        RawWrite("\n");
    }

    if (code == EXCEPTION_ACCESS_VIOLATION && exceptionInfo->ExceptionRecord->NumberParameters >= 2) {
        const ULONG_PTR* info = exceptionInfo->ExceptionRecord->ExceptionInformation;
        RawWrite("[CRASH] access violation: ");
        RawWrite(info[0] == 1 ? "WRITE" : (info[0] == 8 ? "EXEC" : "READ"));
        RawWrite(" at 0x");
        RawWriteNum((unsigned long long)info[1], 16);
        RawWrite("\n");
    }

    if (IntegrationTest_GetMode() == INT_TEST_GAMEPLAY_ROUNDTRIP) {
        IntegrationTest_LogGameplayState("signal");
    }

    // The same registers + traceback dump libultraship's own filter produces
    // (appended to the rotating log as a CRITICAL record, then flushed) —
    // just without the modal MessageBoxA that follows it upstream.
    auto ctx = Ship::Context::GetInstance();
    if (ctx != nullptr && ctx->GetCrashHandler() != nullptr) {
        ctx->GetCrashHandler()->PrintStack(exceptionInfo->ContextRecord);
    }

    RawWrite("[CRASH] exiting immediately (headless session; no crash dialog)\n");

    TerminateProcess(GetCurrentProcess(), (UINT)(128 + MapExceptionToSignal(code)));
    return EXCEPTION_EXECUTE_HANDLER; // unreachable
}

void InstallHandlers() {
    SetUnhandledExceptionFilter(HeadlessExceptionFilter);
}

#endif // _WIN32

} // namespace

extern "C" void HeadlessCrash_ForceHeadless(void) {
    sForcedHeadless = true;
}

extern "C" int HeadlessCrash_IsHeadless(void) {
    // Cached: getenv is not async-signal-safe, and the verdict must not be
    // able to differ between install time and fault time.
    static const bool sHeadless = DetectHeadless();
    return sHeadless ? 1 : 0;
}

extern "C" void HeadlessCrash_ClaimAndInstall(void) {
    // Construct libultraship's CrashHandler now, on our terms. It is
    // idempotent, so the later calls from OoT's OTRGlobals and MM's BenPort
    // find it already built and skip their sigaction/SetUnhandledExceptionFilter
    // registration entirely. Without this the window between game init and
    // any later re-install is unprotected — and that window is exactly where
    // the #388 boot SIGSEGV fired.
    auto ctx = Ship::Context::GetInstance();
    if (ctx != nullptr) {
        (void)ctx->InitCrashHandler();
    }

    if (!HeadlessCrash_IsHeadless()) {
        return; // Desktop user: keep upstream's dialog and rich log dump.
    }

    InstallHandlers();
}
