/**
 * @file combo_identity_display.cpp
 * @brief What a player reads to tell one paired world from another (#952). See
 *        combo_identity_display.h.
 */

#include "combo_identity_display.h"

#include "context.h"
#include "foreign_items.h"
#include "rsbs_version.h"
#include "save.h"

#include <cstdio>
#include <cstring>

namespace {

ComboSeedStringProvider gSeedStringProvider = nullptr;

// The slot cache (Combo_IdentityDisplay_SlotText): one read of every slot's
// record fingerprint, taken under the writer's lock.
uint32_t gSlotHash[RSBS_SAVE_MAX_SLOTS] = {};
bool gHaveSlots = false;
uint32_t gSlotEpoch = 0;
int gSlotReads = 0;

char gLastDrawn[RSBS_COMBO_IDENTITY_TEXT_CAP] = {};
int gDrawCount = 0;

int WriteEmpty(char* out, size_t cap) {
    if (out != nullptr && cap > 0) {
        out[0] = '\0';
    }
    return 0;
}

} // namespace

extern "C" const char* Combo_IdentityDisplay_Build(void) {
    // The window title's own accessor (#880): RSBS_VERSION_STRING, then
    // "+<short commit>[-dirty]" when configure saw git. The release part only
    // moves at a tag, so the commit is the part that tells two builds apart.
    const char* display = RsbsVersion_Display();
    const size_t releaseLen = std::strlen(RSBS_VERSION_STRING);
    if (std::strncmp(display, RSBS_VERSION_STRING, releaseLen) == 0 && display[releaseLen] == '+' &&
        display[releaseLen + 1] != '\0') {
        return display + releaseLen + 1;
    }
    return display;
}

extern "C" int Combo_IdentityDisplay_Format(uint32_t comboSettingsHash, char* out, size_t cap) {
    if (out == nullptr || cap == 0) {
        return 0;
    }
    if (comboSettingsHash == 0) {
        return WriteEmpty(out, cap);
    }
    std::snprintf(out, cap, "%08X\n%s", (unsigned)comboSettingsHash, Combo_IdentityDisplay_Build());
    return 1;
}

extern "C" int Combo_IdentityDisplay_GeneratedText(uint32_t stagedSeed, char* out, size_t cap) {
    if (!Combo_ForeignPairingActive() || gComboCtx.sharedRandoSeed != stagedSeed) {
        return WriteEmpty(out, cap);
    }
    return Combo_IdentityDisplay_Format(gComboCtx.comboSettingsHash, out, cap);
}

extern "C" int Combo_IdentityDisplay_SlotText(int slot, char* out, size_t cap) {
    if (slot < 0 || slot >= RSBS_SAVE_MAX_SLOTS) {
        return WriteEmpty(out, cap);
    }
    // The epoch is taken BEFORE the read, as the Save Files page does: a write
    // that lands during or after the read moves it past this value.
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    const uint32_t epoch = mgr.SlotStateEpoch();
    if (!gHaveSlots || epoch != gSlotEpoch) {
        rsbs::SlotMeta metas[RSBS_SAVE_MAX_SLOTS] = {};
        if (mgr.TryReadMetaAll(metas, RSBS_SAVE_MAX_SLOTS)) {
            for (int i = 0; i < RSBS_SAVE_MAX_SLOTS; i++) {
                // A refused record is not a world this build will open, so it
                // names none; ReadMeta leaves 0 for no record and legacy pairs.
                gSlotHash[i] = (metas[i].valid && metas[i].state == RSBS_SLOT_VALID) ? metas[i].comboSettingsHash : 0;
            }
            gHaveSlots = true;
            gSlotEpoch = epoch;
            gSlotReads++;
        } else if (!gHaveSlots) {
            // A write is in flight and nothing was read yet: draw nothing this
            // frame and try again on the next.
            return WriteEmpty(out, cap);
        }
    }
    return Combo_IdentityDisplay_Format(gSlotHash[slot], out, cap);
}

extern "C" int Combo_IdentityDisplay_SlotReadCountForTest(void) {
    return gSlotReads;
}

extern "C" ComboSeedStringProvider Combo_IdentityDisplay_SetSeedStringProvider(ComboSeedStringProvider provider) {
    const ComboSeedStringProvider previous = gSeedStringProvider;
    gSeedStringProvider = provider;
    return previous;
}

extern "C" void Combo_IdentityDisplay_PairedSeed(uint32_t sharedRandoSeed, char* out, size_t cap) {
    if (out == nullptr || cap == 0) {
        return;
    }
    if (gSeedStringProvider != nullptr && gSeedStringProvider(sharedRandoSeed, out, cap) != 0 && out[0] != '\0') {
        return;
    }
    std::snprintf(out, cap, "%u", (unsigned)sharedRandoSeed);
}

extern "C" void Combo_IdentityDisplay_NoteDrawn(const char* text) {
    std::snprintf(gLastDrawn, sizeof(gLastDrawn), "%s", text != nullptr ? text : "");
    gDrawCount++;
}

extern "C" const char* Combo_IdentityDisplay_LastDrawn(void) {
    return gLastDrawn;
}

extern "C" int Combo_IdentityDisplay_DrawCount(void) {
    return gDrawCount;
}
