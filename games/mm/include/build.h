#ifndef BUILD_H
#define BUILD_H

#include <libultraship/libultra.h>
#ifdef __cplusplus
extern "C" {
#endif
// 2S2H [Port] Version information - prefixed to avoid collision with OoT
extern char MM_gBuildVersion[];
extern u16 MM_gBuildVersionMajor;
extern u16 MM_gBuildVersionMinor;
extern u16 MM_gBuildVersionPatch;

extern char MM_gGitBranch[];
extern char MM_gGitCommitHash[];
extern char MM_gGitCommitTag[];
extern char MM_gBuildTeam[];
extern char MM_gBuildDate[];
extern char MM_gBuildMakeOption[];
#ifdef __cplusplus
}
#endif


#endif
