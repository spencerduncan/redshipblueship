#pragma once

#include "combo/Export.h"
#include <string>

#ifdef _WIN32
    #include <windows.h>
    using LibraryHandle = HMODULE;
    #define LIBRARY_HANDLE_NULL nullptr
#else
    using LibraryHandle = void*;
    #define LIBRARY_HANDLE_NULL nullptr
#endif

namespace Combo {

/**
 * Cross-platform dynamic library loading.
 *
 * Abstracts dlopen/LoadLibrary differences between Linux and Windows.
 * Used to load game shared libraries (soh.so/dll, 2ship.so/dll) at runtime.
 */
class COMBO_API DynamicLibrary {
public:
    /**
     * Load a shared library from the given path.
     * @param path Path to the library (.so on Linux, .dll on Windows)
     * @return Handle to the loaded library, or LIBRARY_HANDLE_NULL on failure
     */
    static LibraryHandle Load(const std::string& path);

    /**
     * Unload a previously loaded library.
     * @param handle Handle from a previous Load() call
     */
    static void Unload(LibraryHandle handle);

    /**
     * Get a symbol (function or variable) from a loaded library.
     * @param handle Handle from a previous Load() call
     * @param name Name of the symbol to find
     * @return Pointer to the symbol, or nullptr if not found
     */
    static void* GetSymbol(LibraryHandle handle, const std::string& name);

    /**
     * Get the last error message from a failed operation.
     * @return Human-readable error string
     */
    static std::string GetError();

    /**
     * Check if a handle is valid (non-null).
     */
    static bool IsValid(LibraryHandle handle) {
        return handle != LIBRARY_HANDLE_NULL;
    }
};

/**
 * RAII wrapper for LibraryHandle.
 * Automatically unloads the library when destroyed.
 */
class COMBO_API ScopedLibrary {
public:
    ScopedLibrary() : mHandle(LIBRARY_HANDLE_NULL) {}
    explicit ScopedLibrary(const std::string& path) : mHandle(DynamicLibrary::Load(path)) {}
    ~ScopedLibrary() { if (mHandle) DynamicLibrary::Unload(mHandle); }

    // Move only
    ScopedLibrary(ScopedLibrary&& other) noexcept : mHandle(other.mHandle) { other.mHandle = LIBRARY_HANDLE_NULL; }
    ScopedLibrary& operator=(ScopedLibrary&& other) noexcept {
        if (this != &other) {
            if (mHandle) DynamicLibrary::Unload(mHandle);
            mHandle = other.mHandle;
            other.mHandle = LIBRARY_HANDLE_NULL;
        }
        return *this;
    }

    // No copy
    ScopedLibrary(const ScopedLibrary&) = delete;
    ScopedLibrary& operator=(const ScopedLibrary&) = delete;

    LibraryHandle Get() const { return mHandle; }
    bool IsValid() const { return DynamicLibrary::IsValid(mHandle); }
    explicit operator bool() const { return IsValid(); }

    void* GetSymbol(const std::string& name) const {
        return DynamicLibrary::GetSymbol(mHandle, name);
    }

    template<typename T>
    T GetFunction(const std::string& name) const {
        return reinterpret_cast<T>(GetSymbol(name));
    }

private:
    LibraryHandle mHandle;
};

} // namespace Combo
