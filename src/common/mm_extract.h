/**
 * @file mm_extract.h
 * @brief In-app mm.o2r generation seams for the single executable (#944)
 *
 * Declared here and defined in games/mm/2s2h/GameExports_SingleExe.cpp, the
 * one translation unit on the OoT side of the bridge that may include MM's
 * Extractor/Extract.h: OoT's soh/Extractor/Extract.h defines the same global
 * names (RomSearchMode, MB32..MB64), so OoT's port layer (RunExtract in
 * games/oot/soh/OTRGlobals.cpp) reaches MMExtractor through these instead.
 * Every function catches every exception: the callers run on the extraction
 * worker thread or inside a boot flow where an escaped exception ends the app.
 */
#ifndef RSBS_COMMON_MM_EXTRACT_H
#define RSBS_COMMON_MM_EXTRACT_H

#include <string>
#include <vector>

/** True when <installPath>/assets holds MM's extractor data (Config_N64_US.xml) and a bundled ZAPD_MM executable. */
bool MM_Extract_ToolsAvailable(const std::string& installPath);
/** Directories to scan for an MM ROM: MM's app directory, installPath and the directory of $APPIMAGE; existing
 * directories only, de-duplicated. */
std::vector<std::string> MM_Extract_RomSearchDirs(const std::string& installPath);
/** Silent: full paths of the .z64/.n64/.v64 files in dirs that validate as a supported MM ROM. Never shows a dialog. */
std::vector<std::string> MM_Extract_FindValidRoms(const std::vector<std::string>& dirs);
/** Silent validation of one file (regular file, extension, size, not compressed, known header and whole-file CRC).
 * Never shows a dialog, never throws. */
bool MM_Extract_ValidateRomFile(const std::string& path);
/** Native file picker plus validation with the extractor's own error boxes; "" when cancelled or invalid. A cancel
 * shows nothing. */
std::string MM_Extract_PickRomInteractive(const std::string& startDir);
/** Blocking: validates romPath's contents again (not its extension: the picker never checks one), runs ZAPD_MM and
 * writes <exportDir>/mm.o2r. quiet suppresses the console window / "Extracting" box for the in-window flow. Safe on a
 * worker thread; reports its own errors through native boxes as CallZapd does; never throws. */
bool MM_Extract_Generate(const std::string& romPath, const std::string& installPath, const std::string& exportDir,
                         bool quiet);
/** Where mm.o2r is written: MM's app directory, created if absent. */
std::string MM_Extract_ExportDir();
/** Test seam: how many native message boxes MMExtractor has shown in this process. */
int MM_Extract_DebugNativeBoxCount();

#endif // RSBS_COMMON_MM_EXTRACT_H
