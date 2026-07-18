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

def load_map_rvas(exe_path):
    """Resolve forensic globals from redship.map next to the exe, so the
    offsets track the current build instead of being hardcoded (RVAs shift on
    every relink). Duplicate names (both games define runFrameContext) are
    disambiguated by the obj column. Returns {} if no map is found."""
    import os
    mapf = os.path.join(os.path.dirname(exe_path), "redship.map")
    want = {  # symbol -> required obj substring (None = any)
        "runFrameContext": "2ship_src",
        "MM_gSystemArena": None,
        "MM_gSystemHeap": None,
        "OoT_gGameState": None,
        "OoT_gPlayState": None,
    }
    rvas = {}
    base = None
    try:
        with open(mapf, "r", errors="replace") as f:
            for line in f:
                if base is None:
                    m = re.search(r"Preferred load address is ([0-9a-fA-F]+)", line)
                    if m:
                        base = int(m.group(1), 16)
                    continue
                m = re.match(r"\s+\S+\s+(\S+)\s+([0-9a-fA-F]{16})\s+(.*)$", line)
                if not m or m.group(1) not in want:
                    continue
                name, addr, rest = m.group(1), int(m.group(2), 16), m.group(3)
                objfilter = want[name]
                if objfilter and objfilter not in rest:
                    continue
                rvas[name] = addr - base
    except OSError:
        pass
    return rvas

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
                if code == 0xC0000005 and rec.NumberParameters >= 2:
                    kind = {0: "READ", 1: "WRITE", 8: "DEP"}.get(rec.ExceptionInformation[0], "?")
                    print("  AV: %s at 0x%X" % (kind, rec.ExceptionInformation[1]))
                mods = module_list(pi.hProcess)
                loc = resolve(mods, rec.ExceptionAddress or 0)
                print("  faulting: %s" % loc)
                hThread = k32.OpenThread(0x1FFFFF, False, ev.dwThreadId)
                got = get_context(hThread)
                if got:
                    rip, rsp, rbp = got
                    print("  RIP=0x%X RSP=0x%X RBP=0x%X (modules=%d)" % (rip, rsp, rbp, len(mods)))
                    base_try = rsp & ~0xFFF
                    stack = read_mem(pi.hProcess, rsp, 0x3000)
                    if len(stack) == 0 and base_try != rsp:
                        stack = read_mem(pi.hProcess, base_try, 0x4000)
                        if stack:
                            off = rsp - base_try
                            print("  (rsp page read from 0x%X, rsp at +0x%X)" % (base_try, off))
                            stack = stack[off:] if off < len(stack) else b""
                    if len(stack) == 0:
                        # walk up: maybe rsp itself is in a decommitted/guard page (coroutine UAF?)
                        for probe in range(1, 9):
                            alt = (rsp & ~0xFFF) + probe * 0x1000
                            stack = read_mem(pi.hProcess, alt, 0x3000)
                            if stack:
                                print("  (rsp page UNREADABLE; scanning from 0x%X instead)" % alt)
                                break
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
                    # Full GPRs from CONTEXT (AMD64 offsets)
                    buf2 = C.create_string_buffer(CONTEXT_SIZE * 2)
                    a2 = (C.addressof(buf2) + 15) & ~15
                    ctx2 = (C.c_byte * CONTEXT_SIZE).from_address(a2)
                    struct.pack_into("<I", ctx2, 0x30, CONTEXT_FULL_AMD64)
                    if k32.GetThreadContext(hThread, C.byref(ctx2)):
                        regs = {}
                        for nm2, off in [("Rax",0x78),("Rcx",0x80),("Rdx",0x88),("Rbx",0x90),
                                         ("Rsi",0xA8),("Rdi",0xB0),("R8",0xB8),("R9",0xC0),
                                         ("R10",0xC8),("R11",0xD0)]:
                            regs[nm2] = struct.unpack_from("<Q", ctx2, off)[0]
                        print("  REGS: " + " ".join("%s=0x%X" % (kk, vv) for kk, vv in regs.items()))
                    # MM graph/arena forensics — global RVAs resolved from the
                    # CURRENT build's redship.map (next to the exe), never
                    # hardcoded: they shift on every relink.
                    rvas = load_map_rvas(sys.argv[1])
                    if not rvas:
                        print("  (no redship.map next to exe — skipping global forensics)")
                    if "MM_gSystemHeap" in rvas:
                        mm_heap = rq(exebase + rvas["MM_gSystemHeap"])
                        print("  MM_gSystemHeap = 0x%X" % (mm_heap or 0))
                    if "runFrameContext" in rvas:
                        # MM runFrameContext: gfxCtx at +0; tail holds gameState/nextOvl/ovl/state
                        mm_rfc = exebase + rvas["runFrameContext"]
                        tail = read_mem(pi.hProcess, mm_rfc + 0x3E0, 0x40)
                        if tail:
                            print("  MM rfc[+0x3E0..+0x420]: %s" % tail.hex())
                    if "MM_gSystemArena" in rvas:
                        ar = read_mem(pi.hProcess, exebase + rvas["MM_gSystemArena"], 0x40)
                        if ar:
                            print("  MM_gSystemArena[0x40]: %s" % ar.hex())
                            head = struct.unpack_from("<Q", ar, 0)[0]
                            start = struct.unpack_from("<Q", ar, 8)[0]
                            print("  arena.head=0x%X arena.start=0x%X" % (head, start))
                            if head:
                                node = read_mem(pi.hProcess, head, 0x20)
                                if node:
                                    print("  head node raw: %s" % node.hex())
                    # OoT side for cross-checks
                    ggs = rq(exebase + rvas["OoT_gGameState"]) if "OoT_gGameState" in rvas else None
                    gps = rq(exebase + rvas["OoT_gPlayState"]) if "OoT_gPlayState" in rvas else None
                    print("  OoT_gGameState=0x%X OoT_gPlayState=0x%X" % (ggs or 0, gps or 0))
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

# Forensics: on AV, dumps registers, MM runFrameContext tail (gameState/
# nextOvl/ovl/state), MM_gSystemArena + its head node, MM_gSystemHeap, and the
# OoT gamestate globals. Global RVAs are resolved from redship.map next to the
# exe at crash time (load_map_rvas) so they always match the running build.
