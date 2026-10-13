#ifndef MM_EXTRACT_H
#define MM_EXTRACT_H

#include <stdint.h>
#include <string>
#include <memory>
#include <vector>

// Values come from windows.h
#ifndef IDYES
#define IDYES 6
#endif
#ifndef IDNO
#define IDNO 7
#endif

static constexpr size_t MB_BASE = 1024 * 1024;
static constexpr size_t MB32 = 32 * MB_BASE;
static constexpr size_t MB54 = 54 * MB_BASE;
static constexpr size_t MB64 = 64 * MB_BASE;

enum class RomSearchMode {
    Both = 0,
    Vanilla = 1,
    MQ = 2,
};

// Named MMExtractor (not Extractor like upstream 2S2H) because single-exe
// builds also compile OoT's identically-named Extractor class
// (games/oot/soh/Extractor/Extract.h) and the two would collide at link time.
class MMExtractor {
    std::unique_ptr<unsigned char[]> mRomData = std::make_unique<unsigned char[]>(MB64);
    std::string mCurrentRomPath;
    std::string mSearchPath;
    size_t mCurRomSize = 0;

    bool GetRomPathFromBox();

    uint32_t GetRomVerCrc() const;
    size_t GetCurRomSize() const;
    bool ValidateAndFixRom();
    bool ValidateRomSize() const;

    bool ValidateRom(bool skipCrcBox = false);
    bool ValidateNotCompressed() const;
    const char* GetZapdVerStr() const;

    void SetRomInfo(const std::string& path);

    void FilterRoms(std::vector<std::string>& roms, RomSearchMode searchMode);
    void ShowSizeErrorBox() const;
    void ShowCrcErrorBox() const;
    void ShowCompressedErrorBox() const;
    int ShowRomPickBox(uint32_t verCrc) const;
    bool ManuallySearchForRomMatchingType(RomSearchMode searchMode);

  public:
    // TODO create some kind of abstraction for message boxes.
    static int ShowYesNoBox(const char* title, const char* text);
    static void ShowErrorBox(const char* title, const char* text);
    bool IsMasterQuest() const;

    // Public for the in-window extraction flow (#944, src/common/mm_extract.h),
    // which drives the ROM search, the picker and the validation step by step
    // instead of through Run's chain of native prompts. SetSearchPath, GetRoms
    // and RunFileStandalone mirror OoT's Extractor
    // (games/oot/soh/Extractor/Extract.h).
    void SetSearchPath(const std::string& path);
    const std::string& GetCurrentRomPath() const;
    void GetRoms(std::vector<std::string>& roms);
    // quietCancel: a cancelled picker returns false without the "No rom
    // selected. Exiting" box, because that caller offers its own retry.
    bool ManuallySearchForRom(bool quietCancel = false);
    // Silent on every path: never shows a box. A directory, a missing file or
    // one that vanishes mid-check is a plain false. Unlike OoT's, it also
    // requires a known header CRC. checkExtension: also require an exact
    // ".z64", ".n64" or ".v64" (case-sensitive, as GetRoms and OoT's check
    // are); false checks the contents only.
    bool RunFileStandalone(const std::string& rom, bool checkExtension = true);
    // Test seam: native message boxes this class has shown in this process.
    static int DebugNativeBoxCount();

    bool Run(std::string searchPath, RomSearchMode searchMode = RomSearchMode::Both);
    // quiet: no console window and no "Extracting" box; the in-window flow
    // draws its own progress modal over the game window.
    bool CallZapd(std::string installPath, std::string exportdir, bool quiet = false);
    const char* GetZapdStr();
    std::string Mkdtemp();
};
#endif
