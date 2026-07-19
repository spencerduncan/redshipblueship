#ifndef MM_SHIP_INIT_HPP
#define MM_SHIP_INIT_HPP

#ifdef __cplusplus

#include <vector>
#include <set>
#include <unordered_map>
#include <string>
#include <functional>

#ifdef RSBS_SINGLE_EXECUTABLE
// Single-exe symbol split for MM's ShipInit registrar system.
//
// OoT (games/oot/soh/ShipInit.hpp) defines an identically-named global
// `struct ShipInit` / `struct RegisterShipInitFunc`, but its map element type
// differs: OoT stores std::vector<ShipInitEntry> (a std::function PLUS a
// std::source_location, ~40 bytes) while MM stores
// std::vector<std::function<void()>> (~32 bytes). ShipInit::GetAll() is an
// inline (COMDAT) function, so the linker folds both games' copies to ONE
// definition backing ONE static map — a silent ODR violation (the class the
// #375 collision tripwire can't yet catch because it skips weak/COMDAT
// symbols). Each game's RegisterShipInitFunc then push_back()s its own element
// type into that single shared vector at a different stride, corrupting it;
// OoT's boot-time ShipInit::InitAll() later invokes a mis-decoded, garbage
// std::function and SIGSEGVs. On Linux that surfaced as the rando-gen "hang":
// the crash lands inside InitOTRForMMFirstBoot -> ShipInit::InitAll and the
// headless SDL crash-handler message box then blocks forever. (Windows hides
// it: /FORCE:MULTIPLE resolves the fold differently and it happens not to
// crash there.)
//
// Fix, mirroring this repo's mm_audio_prefix.h / namespace-S2H convention
// (only MM is renamed; SoH keeps the upstream names): move MM's ShipInit types
// into namespace S2H so they get distinct mangled names and their own map,
// with using-declarations so unqualified MM callers compile unchanged.
namespace S2H {
#endif

struct ShipInit {
    static std::unordered_map<std::string, std::vector<std::function<void()>>>& GetAll() {
        static std::unordered_map<std::string, std::vector<std::function<void()>>> shipInitFuncs;
        return shipInitFuncs;
    }

    static void InitAll() {
        ShipInit::Init("*");
    }

    static void Init(const std::string& path) {
        auto& shipInitFuncs = ShipInit::GetAll();
        for (const auto& initFunc : shipInitFuncs[path]) {
            initFunc();
        }
    }
};

struct RegisterShipInitFunc {
    RegisterShipInitFunc(std::function<void()> initFunc, const std::set<std::string>& updatePaths = {}) {
        auto& shipInitFuncs = ShipInit::GetAll();

        shipInitFuncs["*"].push_back(initFunc);

        for (const auto& path : updatePaths) {
            shipInitFuncs[path].push_back(initFunc);
        }
    }
};

#ifdef RSBS_SINGLE_EXECUTABLE
} // namespace S2H

// Let unqualified upstream MM callers (`ShipInit::Init(...)`,
// `static RegisterShipInitFunc x(...)`) resolve to the S2H versions unchanged.
using S2H::RegisterShipInitFunc;
using S2H::ShipInit;
#endif

#endif // __cplusplus

#endif // MM_SHIP_INIT_HPP
