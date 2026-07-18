"""Minimal Win32 debugger: run redship.exe --version, catch the heap-corruption
exception event (0xC0000374 / any late exception), dump RIP + a raw stack scan
with module!offset resolution (redship offsets resolvable via redship.map)."""
import ctypes as C
import ctypes.wintypes as W
import sys, struct, re, bisect

k32 = C.windll.kernel32
psapi = C.windll.psapi

DEBUG_ONLY_THIS_PROCESS = 0x00000002
INFINITE = 0xFFFFFFFF
EXCEPTION_DEBUG_EVENT = 1
CREATE_PROCESS_DEBUG_EVENT = 3
EXIT_PROCESS_DEBUG_EVENT = 5
LOAD_DLL_DEBUG_EVENT = 6
DBG_CONTINUE = 0x00010002
DBG_EXCEPTION_NOT_HANDLED = 0x80010001

class STARTUPINFO(C.Structure):
    _fields_ = [("cb", W.DWORD), ("lpReserved", W.LPWSTR), ("lpDesktop", W.LPWSTR),
                ("lpTitle", W.LPWSTR), ("dwX", W.DWORD), ("dwY", W.DWORD),
                ("dwXSize", W.DWORD), ("dwYSize", W.DWORD), ("dwXCountChars", W.DWORD),
                ("dwYCountChars", W.DWORD), ("dwFillAttribute", W.DWORD),
                ("dwFlags", W.DWORD), ("wShowWindow", W.WORD), ("cbReserved2", W.WORD),
                ("lpReserved2", C.c_void_p), ("hStdInput", W.HANDLE),
                ("hStdOutput", W.HANDLE), ("hStdError", W.HANDLE)]

class PROCESS_INFORMATION(C.Structure):
    _fields_ = [("hProcess", W.HANDLE), ("hThread", W.HANDLE),
                ("dwProcessId", W.DWORD), ("dwThreadId", W.DWORD)]

class EXCEPTION_RECORD(C.Structure):
    _fields_ = [("ExceptionCode", W.DWORD), ("ExceptionFlags", W.DWORD),
                ("ExceptionRecord", C.c_void_p), ("ExceptionAddress", C.c_void_p),
                ("NumberParameters", W.DWORD),
                ("ExceptionInformation", C.c_size_t * 15)]

class EXCEPTION_DEBUG_INFO(C.Structure):
    _fields_ = [("ExceptionRecord", EXCEPTION_RECORD), ("dwFirstChance", W.DWORD)]

class DEBUG_EVENT_UNION(C.Union):
    _fields_ = [("Exception", EXCEPTION_DEBUG_INFO), ("raw", C.c_byte * 160)]

class DEBUG_EVENT(C.Structure):
    _fields_ = [("dwDebugEventCode", W.DWORD), ("dwProcessId", W.DWORD),
                ("dwThreadId", W.DWORD), ("u", DEBUG_EVENT_UNION)]

# x64 CONTEXT: 16-byte aligned, big. We only need Rip/Rsp/Rbp: offsets in the
# canonical x64 CONTEXT: Rip=0xF8, Rsp=0x98, Rbp=0xA0. ContextFlags=0x30.
CONTEXT_SIZE = 1232 + 16
CONTEXT_FULL_AMD64 = 0x10000B

def get_context(hThread):
    buf = C.create_string_buffer(CONTEXT_SIZE * 2)
    addr = (C.addressof(buf) + 15) & ~15
    ctx = (C.c_byte * CONTEXT_SIZE).from_address(addr)
    struct.pack_into("<I", ctx, 0x30, CONTEXT_FULL_AMD64)
    if not k32.GetThreadContext(hThread, C.byref(ctx)):
        return None
    rsp = struct.unpack_from("<Q", ctx, 0x98)[0]
    rbp = struct.unpack_from("<Q", ctx, 0xA0)[0]
    rip = struct.unpack_from("<Q", ctx, 0xF8)[0]
    return rip, rsp, rbp

def read_mem(hProc, addr, size):
    k32.ReadProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
    out = b""
    pos = 0
    while pos < size:
        chunk = min(0x800, size - pos)
        buf = C.create_string_buffer(chunk)
        got = C.c_size_t(0)
        if not k32.ReadProcessMemory(hProc, C.c_void_p(addr + pos), buf, chunk, C.byref(got)) or got.value == 0:
            break
        out += buf.raw[:got.value]
        pos += got.value
    return out

def module_list(hProc):
    arr = (C.c_void_p * 2048)()
    needed = W.DWORD(0)
    mods = []
    psapi.EnumProcessModulesEx.argtypes = [W.HANDLE, C.POINTER(C.c_void_p), W.DWORD, C.POINTER(W.DWORD), W.DWORD]
    psapi.GetModuleFileNameExW.argtypes = [W.HANDLE, C.c_void_p, W.LPWSTR, W.DWORD]
    psapi.GetModuleInformation.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, W.DWORD]
    if psapi.EnumProcessModulesEx(hProc, arr, C.sizeof(arr), C.byref(needed), 0x03):
        count = min(needed.value // C.sizeof(C.c_void_p), 2048)
        for i in range(count):
            name = C.create_unicode_buffer(512)
            psapi.GetModuleFileNameExW(hProc, arr[i], name, 512)
            class MODINFO(C.Structure):
                _fields_ = [("lpBaseOfDll", C.c_void_p), ("SizeOfImage", W.DWORD),
                            ("EntryPoint", C.c_void_p)]
            mi = MODINFO()
            psapi.GetModuleInformation(hProc, arr[i], C.byref(mi), C.sizeof(mi))
            mods.append((mi.lpBaseOfDll or 0, mi.SizeOfImage, name.value))
    mods.sort()
    return mods

def resolve(mods, addr):
    for base, size, name in mods:
        if base <= addr < base + size:
            short = name.rsplit("\\", 1)[-1]
            return "%s+0x%X" % (short, addr - base)
    return None

def main():
    exe = sys.argv[1]
    args = '"%s" ' % exe + " ".join(sys.argv[2:])
    si = STARTUPINFO(); si.cb = C.sizeof(si)
    pi = PROCESS_INFORMATION()
    if not k32.CreateProcessW(exe, args, None, None, False, DEBUG_ONLY_THIS_PROCESS,
                              None, None, C.byref(si), C.byref(pi)):
        print("CreateProcess failed", k32.GetLastError()); return
    print("PID", pi.dwProcessId)
    ev = DEBUG_EVENT()
    while True:
        if not k32.WaitForDebugEvent(C.byref(ev), 60000):
            print("timeout"); break
        status = DBG_CONTINUE
        if ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT:
            rec = ev.u.Exception.ExceptionRecord
            code = rec.ExceptionCode & 0xFFFFFFFF
            first = ev.u.Exception.dwFirstChance
            if code in (0x80000003, 0x4000001F, 0x80000004):  # breakpoints/single-step
                status = DBG_CONTINUE
            elif code in (0xE06D7363, 0x406D1388, 0x6BA):  # C++ EH, thread-name, WER RPC
                status = DBG_EXCEPTION_NOT_HANDLED
            else:
                print("EXCEPTION 0x%08X firstChance=%d at %s" % (code, first, hex(rec.ExceptionAddress or 0)))
                mods = module_list(pi.hProcess)
                loc = resolve(mods, rec.ExceptionAddress or 0)
                print("  faulting: %s" % loc)
                hThread = k32.OpenThread(0x1FFFFF, False, ev.dwThreadId)
                got = get_context(hThread)
                if got:
                    rip, rsp, rbp = got
                    print("  RIP=0x%X RSP=0x%X (modules=%d)" % (rip, rsp, len(mods)))
                    stack = read_mem(pi.hProcess, rsp, 0x3000)
                    print("  stack bytes read: 0x%X" % len(stack))
                    printed = 0
                    for i in range(0, len(stack) - 7, 8):
                        v = struct.unpack_from("<Q", stack, i)[0]
                        r = resolve(mods, v)
                        if r:
                            print("  [rsp+0x%04X] 0x%016X %s" % (i, v, r))
                            printed += 1
                            if printed > 90: break
                if code == 0xC0000005:
                    exebase = next((b for b, s, nm in mods if nm.lower().endswith("redship.exe")), 0)
                    def rq(a):
                        d = read_mem(pi.hProcess, a, 8)
                        return struct.unpack("<Q", d)[0] if len(d) == 8 else None
                    rfc = exebase + 0x3FC7230        # OoT runFrameContext (gfxCtx embedded at +0)
                    ggs = rq(exebase + 0x42CCCD8)    # OoT_gGameState
                    gps = rq(exebase + 0x42E4D00)    # OoT_gPlayState
                    print("  FORENSICS: OoT_gGameState=0x%X OoT_gPlayState=0x%X rfc=0x%X" % (ggs or 0, gps or 0, rfc))
                    if ggs:
                        hdr = read_mem(pi.hProcess, ggs, 0x40)
                        print("  gameState[0:0x40]: %s" % hdr.hex())
                    arg = rq(rsp + 0x80)
                    print("  SetFrameBuffer arg (gfxCtx) = 0x%X" % (arg or 0))
                    rfc_gfx = read_mem(pi.hProcess, rfc + 0x370, 0x18)
                    print("  rfc.gfxCtx+0x370[0x18]: %s" % rfc_gfx.hex())
                if code in (0xC0000374, 0xC0000005):
                    print("== terminal, detaching ==")
                    k32.TerminateProcess(pi.hProcess, 1)
                    break
                status = DBG_EXCEPTION_NOT_HANDLED
        elif ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT:
            print("process exited")
            break
        k32.ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, status)

main()

# (appended) forensics hook — used when FORENSICS env var set; reads OoT graph
# globals at AV time. RVAs from redship.map (current build):
#   OoT runFrameContext = 0x3FC7230, OoT_gGameState = 0x42CCCD8, OoT_gPlayState = 0x42E4D00
