"""Attach to a running process, dump every thread's RIP + raw-stack scan with
module resolution, then detach leaving it running. Usage: stacks.py <pid>

Windows-only: binds ctypes.windll at module scope, so importing this file on
Linux/macOS raises. Invoke it as a script, never import it."""
import ctypes as C
import ctypes.wintypes as W
import sys, struct

k32 = C.windll.kernel32
psapi = C.windll.psapi

TH32CS_SNAPTHREAD = 0x4
CONTEXT_SIZE = 1232 + 16
CONTEXT_FULL_AMD64 = 0x10000B

class THREADENTRY32(C.Structure):
    _fields_ = [("dwSize", W.DWORD), ("cntUsage", W.DWORD), ("th32ThreadID", W.DWORD),
                ("th32OwnerProcessID", W.DWORD), ("tpBasePri", W.LONG),
                ("tpDeltaPri", W.LONG), ("dwFlags", W.DWORD)]

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
            return "%s+0x%X" % (name.rsplit("\\", 1)[-1], addr - base)
    return None

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

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    pid = int(sys.argv[1])
    hProc = k32.OpenProcess(0x1F0FFF, False, pid)
    if not hProc:
        print("OpenProcess failed", k32.GetLastError()); return 1
    mods = module_list(hProc)
    exebase = next((b for b, s, n in mods if n.lower().endswith("redship.exe")), 0)
    print("modules=%d exe_base=0x%X" % (len(mods), exebase))

    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    te = THREADENTRY32(); te.dwSize = C.sizeof(te)
    tids = []
    if k32.Thread32First(snap, C.byref(te)):
        while True:
            if te.th32OwnerProcessID == pid:
                tids.append(te.th32ThreadID)
            if not k32.Thread32Next(snap, C.byref(te)):
                break
    k32.CloseHandle(snap)
    print("threads:", len(tids))

    for tid in tids:
        hT = k32.OpenThread(0x1FFFFF, False, tid)
        if not hT:
            continue
        k32.SuspendThread(hT)
        buf = C.create_string_buffer(CONTEXT_SIZE * 2)
        addr = (C.addressof(buf) + 15) & ~15
        ctx = (C.c_byte * CONTEXT_SIZE).from_address(addr)
        struct.pack_into("<I", ctx, 0x30, CONTEXT_FULL_AMD64)
        ok = k32.GetThreadContext(hT, C.byref(ctx))
        if ok:
            rsp = struct.unpack_from("<Q", ctx, 0x98)[0]
            rip = struct.unpack_from("<Q", ctx, 0xF8)[0]
            r = resolve(mods, rip)
            print("\n== tid %d RIP=0x%X %s" % (tid, rip, r))
            if True:
                stack = read_mem(hProc, rsp, 0x2000)
                printed = 0
                for i in range(0, len(stack) - 7, 8):
                    v = struct.unpack_from("<Q", stack, i)[0]
                    rr = resolve(mods, v)
                    if rr and 'redship.exe' in rr:
                        print("   [+0x%04X] %s" % (i, rr))
                        printed += 1
                        if printed >= 12:
                            break
        k32.ResumeThread(hT)
        k32.CloseHandle(hT)
    k32.CloseHandle(hProc)
    return 0


if __name__ == "__main__":
    sys.exit(main())
