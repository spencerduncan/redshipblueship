#ifdef _WIN32
#include <Windows.h>
#include <winuser.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#endif
#include "Extract.h"
#include "portable-file-dialogs.h"
#include <ship/utils/binarytools/BitConverter.h>
#include "build.h"

#ifdef unix
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef RSBS_SINGLE_EXECUTABLE
#include "zapd_subprocess.h"
#endif

#ifdef _MSC_VER
#define BSWAP32 _byteswap_ulong
#define BSWAP16 _byteswap_ushort
#elif __has_include(<byteswap.h>)
#include <byteswap.h>
#define BSWAP32 bswap_32
#define BSWAP16 bswap_16
#else
#define BSWAP16(value) ((((value)&0xff) << 8) | ((value) >> 8))

#define BSWAP32(value) \
    (((uint32_t)BSWAP16((uint16_t)((value)&0xffff)) << 16) | (uint32_t)BSWAP16((uint16_t)((value) >> 16)))
#endif

#if defined(_MSC_VER)
#define UNREACHABLE __assume(0)
#elif __llvm__
#define UNREACHABLE __builtin_assume(0)
#else
#define UNREACHABLE __builtin_unreachable();
#endif

#include <stdlib.h>

#include <SDL2/SDL_messagebox.h>

#include <array>
#include <fstream>
#include <filesystem>
#include <thread>
#include <unordered_map>
#include <random>
#include <string>

extern "C" uint32_t CRC32C(unsigned char* data, size_t dataSize);

static constexpr uint32_t MM_US_10 = 0x5354631C;
static constexpr uint32_t MM_US_GC = 0xB443EB08;

static const std::unordered_map<uint32_t, const char*> verMap = {
    { MM_US_10, "US 1.0" },
    { MM_US_GC, "US GC" },
};

// TODO only check the first 54MB of the rom.
static constexpr std::array<const uint32_t, 10> goodCrcs = {
    0x96F49400, // MM US 1.0 32MB
    0xBB434787, // MM GC
};

enum class ButtonId : int {
    YES,
    NO,
    FIND,
};

void MMExtractor::ShowErrorBox(const char* title, const char* text) {
#ifdef _WIN32
    MessageBoxA(nullptr, text, title, MB_OK | MB_ICONERROR);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, nullptr);
#endif
}

void MMExtractor::ShowSizeErrorBox() const {
    std::unique_ptr<char[]> boxBuffer = std::make_unique<char[]>(mCurrentRomPath.size() + 100);
    snprintf(boxBuffer.get(), mCurrentRomPath.size() + 100,
             "The Majora's Mask ROM file %s was not a valid size. Was %zu MB, expecting 32, 54, or 64MB.",
             mCurrentRomPath.c_str(), mCurRomSize / MB_BASE);
    ShowErrorBox("Majora's Mask - Invalid Rom Size", boxBuffer.get());
}

void MMExtractor::ShowCrcErrorBox() const {
    ShowErrorBox("Majora's Mask - Rom CRC invalid",
                 "The selected Majora's Mask ROM CRC did not match the list of known compatible roms. "
                 "Please find another.\n\n"
                 "Visit https://2ship.equipment/ to validate your ROM and see a list of compatible versions");
}

void MMExtractor::ShowCompressedErrorBox() const {
    ShowErrorBox("Majora's Mask - File is Compressed",
                 "The selected file appears to be compressed. Please extract before using.");
}

int MMExtractor::ShowRomPickBox(uint32_t verCrc) const {
    std::unique_ptr<char[]> boxBuffer = std::make_unique<char[]>(mCurrentRomPath.size() + 100);
    SDL_MessageBoxData boxData = { 0 };
    SDL_MessageBoxButtonData buttons[3] = { { 0 } };
    // Default to "No" so a failed SDL_ShowMessageBox (e.g. headless systems,
    // where boxes fail silently) reads as declining instead of leaving ret
    // uninitialized.
    int ret = (int)ButtonId::NO;

    buttons[0].buttonid = 0;
    buttons[0].text = "Yes";
    buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    buttons[1].buttonid = 1;
    buttons[1].text = "No";
    buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    buttons[2].buttonid = 2;
    buttons[2].text = "Find ROM";
    boxData.numbuttons = 3;
    boxData.flags = SDL_MESSAGEBOX_INFORMATION;
    boxData.message = boxBuffer.get();
    boxData.title = "Majora's Mask - Rom Detected";
    boxData.window = nullptr;

    boxData.buttons = buttons;
    snprintf(boxBuffer.get(), mCurrentRomPath.size() + 100,
             "Majora's Mask ROM detected: %s, Header CRC32: %8X. It appears to be: %s. "
             "Use this rom for Majora's Mask?",
             mCurrentRomPath.c_str(), verCrc, verMap.at(verCrc));

    SDL_ShowMessageBox(&boxData, &ret);
    return ret;
}

int MMExtractor::ShowYesNoBox(const char* title, const char* box) {
    // Default to "No" on a failed SDL_ShowMessageBox — see ShowRomPickBox.
    int ret = IDNO;
#ifdef _WIN32
    ret = MessageBoxA(nullptr, box, title, MB_YESNO | MB_ICONQUESTION);
#else
    SDL_MessageBoxData boxData = { 0 };
    SDL_MessageBoxButtonData buttons[2] = { { 0 } };

    buttons[0].buttonid = IDYES;
    buttons[0].text = "Yes";
    buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    buttons[1].buttonid = IDNO;
    buttons[1].text = "No";
    buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    boxData.numbuttons = 2;
    boxData.flags = SDL_MESSAGEBOX_INFORMATION;
    boxData.message = box;
    boxData.title = title;
    boxData.buttons = buttons;
    SDL_ShowMessageBox(&boxData, &ret);
#endif
    return ret;
}

void MMExtractor::SetRomInfo(const std::string& path) {
    mCurrentRomPath = path;
    mCurRomSize = GetCurRomSize();
}

void MMExtractor::FilterRoms(std::vector<std::string>& roms, RomSearchMode searchMode) {
    std::ifstream inFile;
    std::vector<std::string>::iterator it = roms.begin();

    while (it != roms.end()) {
        std::string rom = *it;
        SetRomInfo(rom);

        // Skip. We will handle rom size errors later on after filtering
        if (!ValidateRomSize()) {
            it++;
            continue;
        }

        inFile.open(rom, std::ios::in | std::ios::binary);
        inFile.read((char*)mRomData.get(), mCurRomSize);
        inFile.clear();
        inFile.close();

        BitConverter::RomToBigEndian(mRomData.get(), mCurRomSize);

        // Rom doesn't claim to be valid
        // Game type doesn't match search mode
        if (!verMap.contains(GetRomVerCrc()) || (searchMode == RomSearchMode::Vanilla && IsMasterQuest()) ||
            (searchMode == RomSearchMode::MQ && !IsMasterQuest())) {
            it = roms.erase(it);
            continue;
        }

        it++;
    }
}

void MMExtractor::GetRoms(std::vector<std::string>& roms) {
#ifdef _WIN32
    WIN32_FIND_DATAA ffd;
    // Search mSearchPath like the other platforms (upstream scanned the
    // process working directory, which breaks when cwd differs from the
    // requested search dir), and guard the handle: a failed FindFirstFileA
    // must not feed uninitialized ffd data into the loop.
    std::string searchPattern = mSearchPath + "\\*";
    HANDLE h = FindFirstFileA(searchPattern.c_str(), &ffd);

    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char* ext = PathFindExtensionA(ffd.cFileName);

                // Check for any standard N64 rom file extensions.
                if ((strcmp(ext, ".z64") == 0) || (strcmp(ext, ".n64") == 0) || (strcmp(ext, ".v64") == 0))
                    roms.push_back(mSearchPath + "\\" + ffd.cFileName);
            }
        } while (FindNextFileA(h, &ffd) != 0);
        FindClose(h);
    }
#elif unix
    // Open the directory of the app.
    DIR* d = opendir(mSearchPath.c_str());
    struct dirent* dir;

    if (d != NULL) {
        // Go through each file in the directory
        while ((dir = readdir(d)) != NULL) {
            struct stat path;

            auto fullPath = std::filesystem::path(mSearchPath) / dir->d_name;
            auto fullPathString = fullPath.string();
            const char* fullPathCStr = fullPathString.c_str();

            // Check if current entry is not folder
            stat(fullPathCStr, &path);
            if (S_ISREG(path.st_mode)) {

                // Get the position of the extension character.
                char* ext = strrchr(dir->d_name, '.');
                if (ext != NULL && (strcmp(ext, ".z64") == 0 || strcmp(ext, ".n64") == 0 || strcmp(ext, ".v64") == 0)) {
                    roms.push_back(fullPathCStr);
                }
            }
        }
    }
    closedir(d);
#else
    for (const auto& file : std::filesystem::directory_iterator(mSearchPath)) {
        if (file.is_directory())
            continue;
        if ((file.path().extension() == ".n64") || (file.path().extension() == ".z64") ||
            (file.path().extension() == ".v64")) {
            roms.push_back((file.path()));
        }
    }
#endif
}

bool MMExtractor::GetRomPathFromBox() {
#ifdef _WIN32
    OPENFILENAMEA box = { 0 };
    char nameBuffer[512];
    nameBuffer[0] = 0;

    box.lStructSize = sizeof(box);
    box.lpstrFile = nameBuffer;
    box.nMaxFile = sizeof(nameBuffer) / sizeof(nameBuffer[0]);
    box.lpstrTitle = "Open Majora's Mask Rom";
    box.Flags =
        OFN_NOCHANGEDIR | OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    box.lpstrFilter = "N64 Roms\0*.z64;*.v64;*.n64\0\0";
    if (!GetOpenFileNameA(&box)) {
        DWORD err = CommDlgExtendedError();
        // GetOpenFileName will return 0 but no error is set if the user just closes the box.
        if (err != 0) {
            const char* errStr = nullptr;
            switch (err) {
                case FNERR_BUFFERTOOSMALL:
                    errStr = "Path buffer too small. Move file closer to root of your drive";
                    break;
                case FNERR_INVALIDFILENAME:
                    errStr = "File name for rom provided is invalid.";
                    break;
                case FNERR_SUBCLASSFAILURE:
                    errStr = "Failed to open a filebox because there is not enough RAM to do so.";
                    break;
            }
            MessageBoxA(nullptr, errStr != nullptr ? errStr : "Unknown file dialog error.", "Box Error",
                        MB_OK | MB_ICONERROR);
            return false;
        }
    }
    // The box was closed without something being selected.
    if (nameBuffer[0] == 0) {
        return false;
    }
    mCurrentRomPath = nameBuffer;
#else
    auto selection =
        pfd::open_file("Select a Majora's Mask ROM", mSearchPath, { "N64 Roms", "*.z64 *.n64 *.v64" }).result();

    if (selection.empty()) {
        return false;
    }

    mCurrentRomPath = selection[0];
#endif
    mCurRomSize = GetCurRomSize();
    return true;
}

uint32_t MMExtractor::GetRomVerCrc() const {
    return BSWAP32(((uint32_t*)mRomData.get())[4]);
}

size_t MMExtractor::GetCurRomSize() const {
    return std::filesystem::file_size(mCurrentRomPath);
}

bool MMExtractor::ValidateAndFixRom() {
    const uint32_t actualCrc = CRC32C(mRomData.get(), mCurRomSize);

    for (const uint32_t crc : goodCrcs) {
        if (actualCrc == crc) {
            return true;
        }
    }
    return false;
}

// The file box will only allow selecting an n64 rom but typing in the file name will allow selecting anything.
bool MMExtractor::ValidateNotCompressed() const {
    // ZIP file header
    if (mRomData[0] == 'P' && mRomData[1] == 'K' && mRomData[2] == 0x03 && mRomData[3] == 0x04) {
        return false;
    }
    // RAR file header. Only the first 4 bytes.
    if (mRomData[0] == 'R' && mRomData[1] == 'a' && mRomData[2] == 'r' && mRomData[3] == 0x21) {
        return false;
    }
    // 7z file header. 37 7A BC AF 27 1C
    if (mRomData[0] == '7' && mRomData[1] == 'z' && mRomData[2] == 0xBC && mRomData[3] == 0xAF && mRomData[4] == 0x27 &&
        mRomData[5] == 0x1C) {
        return false;
    }

    return true;
}

bool MMExtractor::ValidateRomSize() const {
    if (mCurRomSize != MB32 && mCurRomSize != MB54 && mCurRomSize != MB64) {
        return false;
    }
    return true;
}

bool MMExtractor::ValidateRom(bool skipCrcTextBox) {
    if (!ValidateNotCompressed()) {
        ShowCompressedErrorBox();
        return false;
    }
    if (!ValidateRomSize()) {
        ShowSizeErrorBox();
        return false;
    }
    if (!ValidateAndFixRom()) {
        if (!skipCrcTextBox) {
            ShowCrcErrorBox();
        }
        return false;
    }
    return true;
}

bool MMExtractor::ManuallySearchForRom() {
    std::ifstream inFile;

    if (!GetRomPathFromBox()) {
        ShowErrorBox("Majora's Mask - No rom selected", "No Majora's Mask ROM selected. Exiting");
        return false;
    }

    inFile.open(mCurrentRomPath, std::ios::in | std::ios::binary);

    if (!inFile.is_open()) {
        return false; // TODO Handle error
    }

    // Validate the size BEFORE reading: mRomData is a fixed 64MB buffer and
    // the file dialog can hand back arbitrarily large files.
    if (!ValidateRomSize()) {
        ShowSizeErrorBox();
        return false;
    }

    inFile.read((char*)mRomData.get(), mCurRomSize);
    inFile.close();
    BitConverter::RomToBigEndian(mRomData.get(), mCurRomSize);

    if (!ValidateRom()) {
        return false;
    }

    return true;
}

bool MMExtractor::ManuallySearchForRomMatchingType(RomSearchMode searchMode) {
    if (!ManuallySearchForRom()) {
        return false;
    }

    char msgBuf[200];
    snprintf(msgBuf, sizeof(msgBuf),
             "The selected Majora's Mask ROM does not match the expected game type\nExpected type: %s.\n\n"
             "Do you want to search again?",
             searchMode == RomSearchMode::MQ ? "Master Quest" : "Vanilla");

    while ((searchMode == RomSearchMode::Vanilla && IsMasterQuest()) ||
           (searchMode == RomSearchMode::MQ && !IsMasterQuest())) {
        int ret = ShowYesNoBox("Majora's Mask - Wrong Game Type", msgBuf);
        switch (ret) {
            case IDYES:
                if (!ManuallySearchForRom()) {
                    return false;
                }
                continue;
            case IDNO:
            default:
                // A failed or closed message box counts as "No".
                return false;
        }
    }

    return true;
}

bool MMExtractor::Run(std::string searchPath, RomSearchMode searchMode) {
    std::vector<std::string> roms;
    std::ifstream inFile;
    // Only report success once a ROM has actually passed validation — the
    // selection loop below can fall through without one (every candidate
    // rejected), and the caller must not hand an empty mRomData to CallZapd.
    bool haveValidRom = false;

    mSearchPath = searchPath;

    GetRoms(roms);
    FilterRoms(roms, searchMode);

    if (roms.empty()) {
        int ret = ShowYesNoBox("Majora's Mask - No roms found",
                               "No Majora's Mask ROMs found. Look for one?");

        switch (ret) {
            case IDYES:
                if (!ManuallySearchForRomMatchingType(searchMode)) {
                    return false;
                }
                haveValidRom = true;
                break;
            case IDNO:
            default:
                // A failed or closed message box counts as "No".
                ShowErrorBox("Majora's Mask - No rom selected", "No Majora's Mask ROM selected. Exiting");
                return false;
        }
    }

    if (roms.size() > 1) {
        int ret = ShowYesNoBox("Majora's Mask - Multiple ROMs Found",
                               "Multiple Majora's Mask ROM files were detected. Select one manually?");
        if (ret == IDYES) {
            if (!ManuallySearchForRomMatchingType(searchMode)) {
                return false;
            }
            roms.clear();
            roms.push_back(mCurrentRomPath);
        }
    }

    for (const auto& rom : roms) {
        SetRomInfo(rom);

        if (!ValidateRomSize()) {
            ShowSizeErrorBox();
            continue;
        }

        inFile.open(rom, std::ios::in | std::ios::binary);
        inFile.read((char*)mRomData.get(), mCurRomSize);
        inFile.clear();
        inFile.close();
        BitConverter::RomToBigEndian(mRomData.get(), mCurRomSize);

        int option = ShowRomPickBox(GetRomVerCrc());

        if (option == (int)ButtonId::YES) {
            if (!ValidateRom(true)) {
                if (rom == roms.back()) {
                    ShowCrcErrorBox();
                } else {
                    ShowErrorBox(
                        "Majora's Mask - Rom CRC invalid",
                        "The selected Majora's Mask ROM CRC did not match the list of known compatible roms. "
                        "Trying the next one...\n\n"
                        "Visit https://2ship.equipment/ to validate your ROM and see a list of compatible versions");
                }
                continue;
            }
            haveValidRom = true;
            break;
        } else if (option == (int)ButtonId::FIND) {
            if (!ManuallySearchForRomMatchingType(searchMode)) {
                return false;
            }
            haveValidRom = true;
            break;
        } else {
            // ButtonId::NO, or a failed/closed message box — skip this ROM.
            if (rom == roms.back()) {
                ShowErrorBox("Majora's Mask - No rom provided", "No Majora's Mask ROM provided. Exiting");
                return false;
            }
            continue;
        }
    }
    return haveValidRom;
}

bool MMExtractor::IsMasterQuest() const {
    return false;
}

const char* MMExtractor::GetZapdVerStr() const {
    switch (GetRomVerCrc()) {
        case MM_US_10:
            return "N64_US";
        case MM_US_GC:
            return "GC_US";
        default:
            // We should never be in a state where this path happens.
            UNREACHABLE;
            break;
    }
}

std::string MMExtractor::Mkdtemp() {
    std::string temp_dir = std::filesystem::temp_directory_path().string();

    // create 6 random alphanumeric characters (sizeof includes the NUL
    // terminator, so the last valid index is sizeof - 2)
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    char randchr[7];
    for (int i = 0; i < 6; i++) {
        randchr[i] = charset[dist(gen)];
    }
    randchr[6] = '\0';

    std::string tmppath = temp_dir + "/extractor-" + randchr;
    std::filesystem::create_directory(tmppath);
    return tmppath;
}

#ifndef RSBS_SINGLE_EXECUTABLE
extern "C" int zapd_main(int argc, char** argv);
#endif
static void MessageboxWorker();

bool MMExtractor::CallZapd(std::string installPath, std::string exportdir) {
    constexpr int argc = 22;
    char xmlPath[1024];
    char confPath[1024];
    char portVersion[18]; // 5 digits for int16_max (x3) + separators + terminator
    std::array<const char*, argc> argv;
    const char* version = GetZapdVerStr();
    const char* otrFile = "mm.o2r";

    std::string romPath = std::filesystem::absolute(mCurrentRomPath).string();
    installPath = std::filesystem::absolute(installPath).string();
    exportdir = std::filesystem::absolute(exportdir).string();

    std::string assetsPath = installPath + "/assets";

    // Verify assets directory exists before proceeding
    if (!std::filesystem::exists(assetsPath)) {
        std::string errMsg = "Extractor assets not found at: " + assetsPath +
                             "\n\nThis may indicate a packaging issue with the AppImage or installation.";
        ShowErrorBox("Extraction Failed", errMsg.c_str());
        return false;
    }

    // Work this out in a temporary folder. Everything below drives throwing
    // std::filesystem overloads; an escaped exception would propagate out of
    // the pre-window extraction flow and terminate the app, so wrap the body
    // and guarantee cwd + tempdir cleanup on every exit path (mirrors the
    // hardening in OoT's CallZapd, games/oot/soh/Extractor/Extract.cpp).
    std::string tempdir;
    std::string curdir;
    bool success = false;
    try {
        curdir = std::filesystem::current_path().string();
        tempdir = Mkdtemp();
#ifdef _WIN32
        // Copy the assets tree except extractor/ — the bundled ZAPD
        // executables are spawned from the install dir, not the temp copy.
        std::filesystem::create_directories(tempdir + "/assets");
        for (const auto& entry : std::filesystem::directory_iterator(assetsPath)) {
            if (entry.path().filename() == "extractor") {
                continue;
            }
            std::filesystem::copy(entry.path(), tempdir + "/assets/" + entry.path().filename().string(),
                                  std::filesystem::copy_options::recursive |
                                      std::filesystem::copy_options::update_existing);
        }
#else
        std::filesystem::create_symlink(assetsPath, tempdir + "/assets");
#endif

        std::filesystem::current_path(tempdir);

        snprintf(xmlPath, 1024, "assets/xml/%s", version);
        snprintf(confPath, 1024, "assets/Config_%s.xml", version);
        snprintf(portVersion, 18, "%d.%d.%d", MM_gBuildVersionMajor, MM_gBuildVersionMinor, MM_gBuildVersionPatch);

        argv[0] = "ZAPD";
        argv[1] = "ed";
        argv[2] = "-i";
        argv[3] = xmlPath;
        argv[4] = "-b";
        argv[5] = romPath.c_str();
        argv[6] = "-fl";
        argv[7] = "assets/filelists";
        argv[8] = "-gsf";
        argv[9] = "0";
        argv[10] = "-rconf";
        argv[11] = confPath;
        argv[12] = "-se";
        argv[13] = "OTR";
        argv[14] = "--otrfile";
        argv[15] = otrFile;
        argv[16] = "--portVer";
        argv[17] = portVersion;
        argv[18] = "-o";
        argv[19] = "placeholder";
        argv[20] = "-osf";
        argv[21] = "placeholder";

        // Validate config XML exists before invoking ZAPD
        if (!std::filesystem::exists(confPath)) {
            std::string errMsg = "Extractor config not found: " + std::string(confPath) +
                                 "\n\nThis may indicate an incomplete installation.";
            fprintf(stderr, "Extractor config not found: %s. This may indicate an incomplete installation.\n",
                    confPath);
            ShowErrorBox("Extraction Failed", errMsg.c_str());
        } else {
#ifdef _WIN32
            // Grab a handle to the command window.
            HWND cmdWindow = GetConsoleWindow();

            // Normally the command window is hidden. We want the window to be shown here so the user can see the
            // progess of the extraction.
            ShowWindow(cmdWindow, SW_SHOW);
            SetWindowPos(cmdWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
#else
            std::thread mbThread(MessageboxWorker);
            mbThread.detach();
#endif

            bool zapdRan = false;
#ifdef RSBS_SINGLE_EXECUTABLE
            // Issue #325: in the single exe zapd_main cannot be called in-process
            // — both ZAPDLib_OoT and ZAPDLib_MM define it, and MM's assets need
            // the GAME_MM exporter set. Spawn the bundled standalone ZAPD_MM
            // instead (shared driver in src/common/zapd_subprocess.cpp).
            std::string zapdPath = ZapdSubprocess_Locate(assetsPath, "ZAPD_MM");
            if (zapdPath.empty()) {
                std::string errMsg = "ZAPD extractor executable not found under: " + assetsPath +
                                     "/extractor\n\nThis may indicate an incomplete installation.";
                fprintf(stderr, "%s\n", errMsg.c_str());
                ShowErrorBox("Extraction Failed", errMsg.c_str());
            } else {
                int zapdRet = ZapdSubprocess_Run(zapdPath, argv.data(), argc);
                if (zapdRet != 0) {
                    fprintf(stderr, "ZAPD exited with code %d\n", zapdRet);
                    std::string errMsg = "ZAPD extraction failed (exit code " + std::to_string(zapdRet) +
                                         ").\n\nCheck that the ROM file is valid and the assets directory is "
                                         "complete.";
                    ShowErrorBox("Extraction Failed", errMsg.c_str());
                } else {
                    zapdRan = true;
                }
            }
#else
            try {
                zapd_main(argc, (char**)argv.data());
                zapdRan = true;
            } catch (const std::exception& e) {
                fprintf(stderr, "Extraction failed: %s\n", e.what());
                std::string errMsg = "ZAPD extraction failed with error: " + std::string(e.what());
                ShowErrorBox("Extraction Failed", errMsg.c_str());
            } catch (...) {
                fprintf(stderr, "Extraction failed with unknown error\n");
                ShowErrorBox("Extraction Failed", "ZAPD extraction failed with an unknown error.");
            }
#endif

#ifdef _WIN32
            // Hide the command window again.
            ShowWindow(cmdWindow, SW_HIDE);
#endif

            if (zapdRan) {
                // Verify the output file was created before attempting to copy
                if (!std::filesystem::exists(otrFile)) {
                    std::string errMsg =
                        "ZAPD extraction failed - output file was not created: " + std::string(otrFile) +
                        "\n\nCheck that the ROM file is valid and the assets directory is complete.";
                    ShowErrorBox("Extraction Failed", errMsg.c_str());
                } else {
                    std::filesystem::copy(otrFile, exportdir + "/" + otrFile,
                                          std::filesystem::copy_options::overwrite_existing);
                    success = true;
                }
            }
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "Extraction failed during file setup/copy: %s\n", e.what());
        std::string errMsg = "Extraction failed during file setup/copy: " + std::string(e.what());
        ShowErrorBox("Extraction Failed", errMsg.c_str());
    } catch (...) {
        fprintf(stderr, "Extraction failed during file setup/copy with unknown error\n");
        ShowErrorBox("Extraction Failed", "Extraction failed during file setup/copy with an unknown error.");
    }

    // Cleanup on all paths, including exception unwind. Use the non-throwing
    // overloads so a failing cleanup cannot escape either.
    std::error_code ec;
    if (!curdir.empty()) {
        std::filesystem::current_path(curdir, ec);
    }
    if (!tempdir.empty()) {
        std::filesystem::remove_all(tempdir, ec);
    }

    return success;
}

static void MessageboxWorker() {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Extracting",
                             "Extraction will now begin in the background.\n\nPlease be patient for the process to "
                             "finish. Do not close the main program.",
                             nullptr);
}