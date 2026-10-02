/**
 * TrackerAdapterSingleExe.cpp — MM's combo-tracker adapter: the offset
 * descriptor over the frozen MM shadow blob, plus the check-name resolver
 * (#458; ADR 0002).
 *
 * This is the ONE translation unit where the combo tracker's MM half may see
 * MM's layout: the byte offsets of ShipSaveInfo's rando check table inside
 * SaveContext, the SAVETYPE_RANDO value, and Rando::StaticData's names. All
 * of it crosses into src/common as a flat ComboMMTrackerDesc — the
 * RsbsGameMetaDesc pattern (src/common/save.h) — so common code never
 * includes z64save.h and never hardcodes an MM offset.
 *
 * THE SHADOW BLOB WHILE MM IS NOT PLAYED, THE LIVE SAVE WHILE IT IS. While
 * OoT runs there is no live MM gSaveContext to read (unified storage holds
 * OoT's bytes), but the frozen shadow Context_GetMMSaveContext() hands out is
 * a raw SaveContext image written at freeze/save time. Reading it at these
 * offsets is exactly as valid as the freeze itself, and the view labels the
 * result stale ("As of the last game switch or save"). While MM is played the
 * shadow is useless: MM's arrival consumes it into gSaveContext and zeroes it,
 * and nothing refills it before MM's first save or the departure freeze
 * (#799). So the descriptor also carries MMTrackerLiveSave, which hands the
 * view &gSaveContext — the same layout, the same offsets — only while MM's
 * play state is loaded (ADR 0008 rule 5's amendment), and the view reports
 * that read live.
 *
 * THE TRIPWIRES. The descriptor is built from offsetof/sizeof, so it cannot
 * drift from the struct — what CAN break silently is the geometry: the table
 * outgrowing the MM_SAVE_CONTEXT_SIZE blob capacity (a truncated read), or a
 * flag member leaving its row. The static_asserts below turn both into build
 * errors in the TU that owns the layout knowledge.
 *
 * Lives in 2s2h/Rando/, glob-collected into `2ship_rando` (WHOLE_ARCHIVE), and
 * additionally referenced by name from Combo_TrackerWindow_Init — so unlike
 * the #516 dead-registrar class there is no elision mode where the tracker
 * silently shows an empty MM panel.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstddef>
#include <cstring>
#include <string>

#include "Rando/Rando.h" // SaveContext (via variables.h), RC_MAX, SAVETYPE_RANDO, StaticData

// src/common. Included OUTSIDE any extern "C" block: the header manages its
// own linkage and pulls context.h, whose <type_traits> include must not be
// wrapped in C linkage (matching Foreign.cpp).
#include "combo_tracker_view.h"
#include "game.h" // MM_SAVE_CONTEXT_SIZE

// The reader in combo_tracker_view.c walks the row's flag bytes as u8 and the
// header fields as u32; these pin the assumptions to the real types.
static_assert(sizeof(bool) == 1, "RandoSaveCheck's flags are read as single bytes through the descriptor");
static_assert(sizeof(SaveType) == 4, "ShipSaveInfo.saveType is read as a u32 through the descriptor");
static_assert(sizeof(((RandoSaveInfo*)0)->finalSeed) == 4, "finalSeed is read as a u32 through the descriptor");
static_assert(sizeof(((ShipSaveInfo*)0)->fileCreatedAt) == 8, "fileCreatedAt is read as a u64 through the descriptor");

// The whole check table must sit inside the shadow blob capacity, or the
// reader would count truncated rows. GameExports_SingleExe.cpp already
// asserts sizeof(SaveContext) <= MM_SAVE_CONTEXT_SIZE; this narrows it to the
// exact bytes the tracker reads, so an upstream ShipSaveInfo growth that
// pushes the table past the capacity names the actual consumer that breaks.
static_assert(offsetof(SaveContext, save.shipSaveInfo.rando.randoSaveChecks) +
                      (size_t)RC_MAX * sizeof(RandoSaveCheck) <=
                  (size_t)MM_SAVE_CONTEXT_SIZE,
              "MM's rando check table no longer fits the cross-game shadow blob; the combo tracker (and the "
              "freeze machinery) would truncate it");

// Every flag the descriptor names must live inside one row's stride.
static_assert(offsetof(RandoSaveCheck, shuffled) < sizeof(RandoSaveCheck) &&
                  offsetof(RandoSaveCheck, obtained) < sizeof(RandoSaveCheck) &&
                  offsetof(RandoSaveCheck, skipped) < sizeof(RandoSaveCheck),
              "RandoSaveCheck flag offsets must stay within the row stride");

namespace {

/**
 * Display name for an MM check id, from the same CheckNames array MM's own
 * check tracker labels rows with. Empty (RC_MAX empty strings) until
 * PopulateCheckNames runs — MM_TrackerAdapter_Register below calls it, so the
 * names exist the moment the adapter does, MM booted or not (#489 cause 2 was
 * exactly this array staying empty in single-exe). Returns NULL rather than
 * "" so the renderer's "no name -> show the id" fallback triggers.
 */
const char* MMTrackerCheckName(uint16_t checkId) {
    if (checkId >= (uint16_t)RC_MAX) {
        return nullptr;
    }
    const std::string& name = Rando::StaticData::CheckNames[checkId];
    return name.empty() ? nullptr : name.c_str();
}

/**
 * MM's live save for the view's LIVE read, or NULL (#799). Only while MM's play
 * state is loaded: MM_gPlayState is set late in MM_Play_Init, after the
 * arrival consumes the frozen half into gSaveContext (z_play.c), and nulled
 * by MM_Play_Destroy and MM_Graph_ResetRunFrameContext (graph.c). That excludes the
 * title screen, which authors an unmarked bootstrap save (Sram_InitNewSave),
 * and file select, which reads each slot through the live buffer while it
 * scans them — neither is the player's world.
 */
const void* MMTrackerLiveSave(void) {
    return (MM_gPlayState != nullptr) ? static_cast<const void*>(&gSaveContext) : nullptr;
}

} // namespace

extern "C" void MM_TrackerAdapter_Register(void) {
    // Pure overwrite over StaticData::Checks, idempotent, needs no archive —
    // the #457/#489 re-homing precedent. Called here (not from a file-scope
    // registrar) so it cannot race the static init of the maps it reads.
    Rando::StaticData::PopulateCheckNames();

    ComboMMTrackerDesc desc = {};
    desc.newfOffset = (uint32_t)offsetof(SaveContext, save.saveInfo.playerData.newf);
    desc.newfLen = 6;
    // MM's "newf" sentinel, mirroring SaveManager.cpp's RsbsRegisterMMMetaOnce:
    // an all-zero shadow (MM never entered) fails this compare and reads as
    // "no data" instead of as a vanilla save with zero progress.
    const char kMMNewf[6] = { 'Z', 'E', 'L', 'D', 'A', '3' };
    std::memcpy(desc.newf, kMMNewf, sizeof(kMMNewf));
    desc.saveTypeOffset = (uint32_t)offsetof(SaveContext, save.shipSaveInfo.saveType);
    desc.saveTypeRando = (uint32_t)SAVETYPE_RANDO;
    desc.createdAtOffset = (uint32_t)offsetof(SaveContext, save.shipSaveInfo.fileCreatedAt);
    desc.finalSeedOffset = (uint32_t)offsetof(SaveContext, save.shipSaveInfo.rando.finalSeed);
    desc.checkTableOffset = (uint32_t)offsetof(SaveContext, save.shipSaveInfo.rando.randoSaveChecks);
    desc.checkStride = (uint32_t)sizeof(RandoSaveCheck);
    desc.checkCount = (uint32_t)RC_MAX;
    desc.shuffledOffset = (uint32_t)offsetof(RandoSaveCheck, shuffled);
    desc.obtainedOffset = (uint32_t)offsetof(RandoSaveCheck, obtained);
    desc.skippedOffset = (uint32_t)offsetof(RandoSaveCheck, skipped);
    desc.checkName = MMTrackerCheckName;
    desc.liveSave = MMTrackerLiveSave;

    Combo_Tracker_RegisterMM(&desc);
}

/**
 * TEST BRIDGE (redship --test combo-tracker-view, #458 U4): store the MM item
 * whose display name is `name` at check `checkId` of the SaveContext image
 * `save`, as a generated world would, so the ROM-free lock can author a placed
 * item without seeing RandoItemId. Returns 1 when written, 0 for a bad argument
 * or a name no MM item carries.
 */
extern "C" int MM_TrackerAdapter_TestSetStoredItem(void* save, uint16_t checkId, const char* name) {
    if (save == nullptr || name == nullptr || checkId >= (uint16_t)RC_MAX) {
        return 0;
    }
    for (const auto& [randoItemId, item] : Rando::StaticData::Items) {
        if (item.name != nullptr && std::strcmp(item.name, name) == 0) {
            static_cast<SaveContext*>(save)->save.shipSaveInfo.rando.randoSaveChecks[checkId].randoItemId = randoItemId;
            return 1;
        }
    }
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
