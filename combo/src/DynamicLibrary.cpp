#include "combo/DynamicLibrary.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace Combo {

LibraryHandle DynamicLibrary::Load(const std::string& path) {
#ifdef _WIN32
    // Convert UTF-8 path to wide string for Windows API
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, path.c_str(),
                                          static_cast<int>(path.length()), nullptr, 0);
    std::wstring wpath(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(),
                        static_cast<int>(path.length()), &wpath[0], size_needed);
    return LoadLibraryW(wpath.c_str());
#else
    // RTLD_NOW: Resolve all symbols immediately (fail fast if missing)
    // RTLD_LOCAL: Don't export symbols to other loaded libraries (isolation)
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void DynamicLibrary::Unload(LibraryHandle handle) {
    if (!handle) return;

#ifdef _WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
}

void* DynamicLibrary::GetSymbol(LibraryHandle handle, const std::string& name) {
    if (!handle) return nullptr;

#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(handle, name.c_str()));
#else
    return dlsym(handle, name.c_str());
#endif
}

std::string DynamicLibrary::GetError() {
#ifdef _WIN32
    DWORD error = GetLastError();
    if (error == 0) return "";

    LPSTR buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer), 0, nullptr);

    std::string message(buffer, size);
    LocalFree(buffer);

    // Remove trailing newline
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }
    return message;
#else
    const char* error = dlerror();
    return error ? error : "";
#endif
}

} // namespace Combo
