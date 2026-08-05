/**
 * ROM-free locks for the spoiler-drop identity gate (#610).
 * CTest label "redship" (display-free); rows `mm-spoiler-identity` and
 * `mm-foreign-pickup-gate` in src/common/test_runner.cpp.
 *
 * Lives MM-side because the surfaces under test need MM's headers:
 * `Rando::Spoiler::LoadFromFile` / `ApplyToSaveContext` (the two calls
 * OnFileCreate's LOAD branch makes, OnFileCreate.cpp:439-441) and
 * `Rando::Foreign::RecordForeignPickup`.
 *
 * ============================================================================
 * WHAT #610 IS
 * ============================================================================
 *
 * `Rando::Spoiler::HandleFileDropped` stages ANY file dragged onto the window
 * that parses as a `2S2H_RANDO_SPOILER` into MM's spoiler folder and selects it
 * (`gRando.SpoilerFile` / `gRando.SpoilerFileIndex`). That staging is correctly
 * identity-free — it is a file copy. The hole was downstream: on an UNPAIRED MM
 * file `Rando::MiscBehavior::OnFileCreate` takes the LOAD branch, and
 * `ApplyToSaveContext` called `ReconstructForeignPlacements` unconditionally,
 * committing whatever the dropped file's `"foreign"` section claimed straight
 * into `gComboCtx.foreignPlacements` — with no comparison against the terms
 * #570 freezes at creation (`sourceIsRando` / `sharedRandoSeed` /
 * `sharedRandoSettingsHash` / `mmProfileDigest`). From there `RecordForeignPickup`
 * fired on every collected check with no `Combo_ForeignPairingActive()` gate and
 * `Combo_RecordSharedItem` durably recorded a pickup that a LATER, genuine
 * paired-OoT arrival redeems — injecting an item from an unrelated world into a
 * world whose fill never placed it.
 *
 * Under one-game semantics (#564) the identity freezes at creation: a spoiler
 * that names a different world is corruption to REFUSE, never state to absorb.
 * That is #601's rule ("an identity a file did not have authored for it is not
 * that file's identity") applied to MM's spoiler entry path.
 *
 * ============================================================================
 * mm-spoiler-identity — the commit gate
 * ============================================================================
 *
 * Drives the REAL consumer pair (`LoadFromFile` + `ApplyToSaveContext`) over a
 * spoiler produced by the REAL writer (`GenerateFromSaveContext`) and staged
 * through the REAL path (`SaveToFile` into `SpoilerDirectory()`, selected with
 * the same two CVars `HandleFileDropped` sets). Nothing here is a paraphrase of
 * the shipping code.
 *
 * The divergence is modelled by moving the LIVE session's identity, never by
 * hand-editing the spoiler's: that keeps the staged file byte-authentic output
 * of the shipping writer, so leg 5 below is a genuine lock on the writer
 * EMITTING an identity at all (without it, every real paired spoiler would be
 * refused as unidentified and leg 5 goes red).
 *
 *   1. unpaired session (the issue's exact scenario) -> NOT committed, refused
 *   2. paired, divergent sharedRandoSeed            -> NOT committed, refused
 *   3. paired, divergent sharedRandoSettingsHash    -> NOT committed, refused
 *   4. paired, divergent mmProfileDigest            -> NOT committed, refused
 *   5. paired, identity matches                     -> COMMITTED, no refusal
 *   6. standalone spoiler, no "foreign" section     -> loads, no refusal
 *
 * Leg 5 is the non-vacuity leg: a gate that refuses everything would pass 1-4
 * and prove nothing. Leg 6 is the scope leg: upstream's standalone spoiler-load
 * capability must survive — the gate is on the COMBO commit, not on the spoiler
 * feature — so a spoiler that touches neither gComboCtx nor the shared records
 * must load exactly as before and latch nothing.
 *
 * Every refusal leg asserts the surface, not just the absence: the #533/#568
 * machinery latches the active slot with `RSBS_REFUSE_IDENTITY` (the same
 * reason #570's arrival gate uses — the slot FILE is healthy, the SESSION
 * diverged, so nothing is quarantined), and the shared overlay carries a toast
 * that NAMES the divergent term. A refusal the player cannot see is #564 V7's
 * silent vanilla revert wearing a fix's clothes.
 *
 * ============================================================================
 * mm-foreign-pickup-gate — the durable-record gate
 * ============================================================================
 *
 * Defense in depth for a placement table populated by ANY route: with no live
 * pairing, collecting a foreign-hosted check must author no durable
 * `sharedItemsTagged` record, because there is no world for that record to
 * belong to. Includes the matching non-vacuity leg (pairing active -> the
 * record IS authored), so "gate everything off" cannot pass.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>

#include "Rando/Rando.h"
#include "Rando/Foreign.h"
#include "Rando/Spoiler/Spoiler.h"
#include "Rando/StaticData/StaticData.h"

// src/common — outside any extern "C" block; these headers manage their own
// linkage (matching Foreign.cpp / mm_rando_options_test.cpp).
#include "foreign_items.h"
#include "shared_items.h"
#include "save.h"                // the #533 REFUSED surface this gate reports through
#include "notification_bridge.h" // the player-visible half of that surface

extern "C" {
#include "variables.h"
}

namespace {

int Fail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-SPOILER-ID] FAIL(%d): ", code);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
    return code;
}

// The slot the refusal latches. Nothing is written to it — RefuseSlotIdentity
// deliberately never quarantines, because the FILE is healthy. The SaveManager
// is pointed at a scratch directory first all the same: ArmSlotOnCreate reads
// (and would quarantine) whatever .redsave sits at the slot path, and a lock
// has no business touching the operator's real saves.
constexpr int kSlot = 0;
const char* const kScratchSaveDir = "rsbs_test_saves_spoiler_identity";

// Planted as the "last toast" before every leg, so a refusal assertion can
// never be satisfied by a toast an EARLIER leg emitted. The overlay's store has
// no drain entry point, and "the last toast still exists" is exactly the vacuous
// pass this sentinel closes.
const char* const kToastSentinel = "rsbs610-no-toast-was-emitted";

// The world the staged spoiler is authored for.
constexpr uint32_t kSeedA = 0x0610A11Au;
constexpr uint32_t kHashA = 0x5EED0610u;
constexpr uint32_t kDigestA = 0xD16E5701u;

/** Publish a live pairing carrier (the shape Playthrough_Init stamps). */
void ArmPairing(uint32_t seed, uint32_t settingsHash, uint32_t profileDigest) {
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = seed;
    gComboCtx.sharedRandoSettingsHash = settingsHash;
    gComboCtx.mmProfileDigest = profileDigest;
}

/** Re-arm the slot so each leg observes ITS OWN refusal, not the previous one's. */
void ResetRefusalSurface() {
    RsbsSave_ResetSlotSessionState();
    RsbsSave_SetActiveSlot(kSlot);
    RsbsSave_ArmSlotOnCreate(kSlot);

    ComboNotification sentinel;
    memset(&sentinel, 0, sizeof(sentinel));
    sentinel.prefix = "";
    sentinel.message = kToastSentinel;
    sentinel.remainingTime = 1.0f;
    // Muted for the same reason #570's refusal toast is: the overlay's ding is
    // OoT's Audio_PlaySoundGeneral, and this runs in a display-free harness
    // where OoT's audio session is not a given.
    sentinel.mute = 1;
    OoT_Notification_Emit(&sentinel);
}

/**
 * The refusal surface, asserted as a whole: latched slot, RSBS_REFUSE_IDENTITY,
 * no quarantine (the .redsave is healthy), and a toast naming `term`.
 */
int AssertRefused(int baseCode, const char* leg, const char* term) {
    if (RsbsSave_IsSlotWritable(kSlot) != 0) {
        return Fail(baseCode,
                    "%s: the active slot was not latched — this session's captures can still reach the "
                    "pair's .redsave",
                    leg);
    }
    if (RsbsSave_GetSlotRefuseReason(kSlot) != (int)RSBS_REFUSE_IDENTITY) {
        return Fail(baseCode + 1, "%s: refusal reason is %d, expected RSBS_REFUSE_IDENTITY", leg,
                    RsbsSave_GetSlotRefuseReason(kSlot));
    }
    if (RsbsSave_HasQuarantine(kSlot) != 0) {
        return Fail(baseCode + 2,
                    "%s: an identity refusal quarantined the slot file — the file is healthy, the "
                    "session diverged (#570)",
                    leg);
    }
    ComboNotification toast;
    memset(&toast, 0, sizeof(toast));
    if (OoT_Notification_PeekLastForTest(&toast) != 1) {
        return Fail(baseCode + 3, "%s: no toast reached the shared overlay — stderr is not a player-visible surface",
                    leg);
    }
    const std::string message = toast.message != nullptr ? toast.message : "";
    const std::string prefix = toast.prefix != nullptr ? toast.prefix : "";
    if (message == kToastSentinel) {
        return Fail(baseCode + 3, "%s: the overlay still holds this leg's sentinel — no refusal toast was emitted",
                    leg);
    }
    if (prefix.find("REFUSED") == std::string::npos) {
        return Fail(baseCode + 3, "%s: the toast's prefix ('%s') does not read as a refusal", leg, prefix.c_str());
    }
    if (message.find(term) == std::string::npos) {
        return Fail(baseCode + 4, "%s: the refusal does not name the divergent term '%s' (message: '%s')", leg, term,
                    message.c_str());
    }
    return 0;
}

} // namespace

extern "C" int MM_SpoilerIdentity_RunHeadless(void) {
    printf("[TEST] mm-spoiler-identity: a dropped spoiler's cross-game section reaches gComboCtx only when its "
           "identity names the live pairing (#610)\n");

    // ---- Stage a spoiler for world A, through the real writer --------------
    {
        std::error_code ec;
        std::filesystem::create_directories(kScratchSaveDir, ec);
        rsbs::SaveManager::Instance().SetSaveDirectory(kScratchSaveDir);
    }
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    ArmPairing(kSeedA, kHashA, kDigestA);
    if (!Combo_ForeignPairingActive()) {
        return Fail(1, "the staging carrier did not read as an active pairing");
    }

    // A host the CURRENT rule accepts (Rando::Foreign::IsEligibleHost): an
    // allowed check class holding a legal junk-class MM item, shuffled and not
    // user-excluded. Found by driving the REAL predicate one candidate at a
    // time and reverting the ones it rejects, so this lock neither pins a check
    // name a future table edit could remove nor paraphrases the host rule. Only
    // the winner is left shuffled, so the staged spoiler describes exactly one
    // check.
    RandoCheckId host = RC_UNKNOWN;
    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        if (randoStaticCheck.randoCheckId == RC_UNKNOWN) {
            continue;
        }
        const RandoSaveCheck previous = RANDO_SAVE_CHECKS[randoCheckId];
        RANDO_SAVE_CHECKS[randoCheckId].randoItemId = RI_JUNK;
        RANDO_SAVE_CHECKS[randoCheckId].shuffled = true;
        RANDO_SAVE_CHECKS[randoCheckId].skipped = false;
        if (Rando::Foreign::IsEligibleHost(randoCheckId)) {
            host = randoCheckId;
            break;
        }
        RANDO_SAVE_CHECKS[randoCheckId] = previous;
    }
    if (host == RC_UNKNOWN) {
        return Fail(2, "no eligible foreign host exists in the check table — the fixture cannot be built");
    }
    const char* hostName = Rando::StaticData::Checks.at(host).name;

    const ComboForeignItemDef* pool = nullptr;
    const int poolCount = Combo_GetForeignItemPoolFor((uint8_t)GAME_OOT, &pool);
    if (poolCount <= 0 || pool == nullptr) {
        return Fail(3, "the OoT foreign pool is not registered in this build");
    }
    const SharedItem placed = pool[0].item;

    Combo_ClearForeignPlacements();
    if (Combo_SetForeignPlacement((uint16_t)host, placed) < 0) {
        return Fail(4, "could not seed the fixture placement on host check %s", hostName);
    }

    gSaveContext.save.shipSaveInfo.rando.finalSeed = 0xABCD0610u;
    nlohmann::json staged = Rando::Spoiler::GenerateFromSaveContext();
    if (!staged.contains("foreign") || !staged["foreign"].is_object() || staged["foreign"].size() != 1) {
        return Fail(5, "the real writer did not emit a one-entry 'foreign' section for the fixture world");
    }

    // Stage it exactly as HandleFileDropped does: the file lands in the spoiler
    // folder and the two CVars select it. Reading the index back proves the
    // LOAD-branch precondition (OnFileCreate.cpp:83) is genuinely satisfied.
    const std::string stagedName = "rsbs610-staged.json";
    try {
        Rando::Spoiler::SaveToFile(stagedName, staged);
        CVarSetString("gRando.SpoilerFile", stagedName.c_str());
        Rando::Spoiler::RefreshOptions();
    } catch (const std::exception& e) { return Fail(6, "staging the spoiler threw: %s", e.what()); }
    if (CVarGetInteger("gRando.SpoilerFileIndex", 0) == 0) {
        return Fail(7, "the staged spoiler is not selected (SpoilerFileIndex == 0) — OnFileCreate would GENERATE, "
                       "so this fixture would not exercise the LOAD branch at all");
    }
    const std::string selected = CVarGetString("gRando.SpoilerFile", "");

    // A standalone (unpaired) spoiler for leg 6, written by the same real
    // writer while nothing is paired: it carries no "foreign" section at all.
    ComboContext_Init();
    const std::string soloName = "rsbs610-solo.json";
    gSaveContext.save.shipSaveInfo.rando.finalSeed = 0x501050A0u;
    nlohmann::json solo = Rando::Spoiler::GenerateFromSaveContext();
    if (solo.contains("foreign")) {
        return Fail(8, "an unpaired world's spoiler carries a 'foreign' section");
    }
    try {
        Rando::Spoiler::SaveToFile(soloName, solo);
    } catch (const std::exception& e) { return Fail(8, "staging the solo spoiler threw: %s", e.what()); }

    // One helper for the four refusal legs and the two accepting ones: reload
    // through the REAL validator and apply through the REAL entry point.
    auto driveLoad = [&](const std::string& fileName, int code, const char** err) -> int {
        *err = nullptr;
        Combo_ClearForeignPlacements();
        try {
            nlohmann::json loaded = Rando::Spoiler::LoadFromFile(fileName);
            Rando::Spoiler::ApplyToSaveContext(loaded);
        } catch (const std::exception& e) {
            static std::string held;
            held = e.what();
            *err = held.c_str();
            return code;
        }
        return 0;
    };
    const char* err = nullptr;

    // ---- Leg 1: the issue's scenario — no live pairing at all --------------
    // A player who never generated a paired OoT world this session drops a
    // spoiler and creates a file. There is no identity to compare against, so
    // there is no world these crossings can belong to.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    ResetRefusalSurface();
    if (int rc = driveLoad(selected, 10, &err)) {
        return Fail(rc, "unpaired load threw instead of refusing the foreign section: %s", err);
    }
    if (Combo_CountForeignPlacements() != 0) {
        return Fail(11,
                    "UNPAIRED session committed %d foreign placement(s) from a dropped spoiler — every check "
                    "collected there would author a durable shared-item record for a world that does not exist "
                    "(#610)",
                    Combo_CountForeignPlacements());
    }
    if (int rc = AssertRefused(12, "unpaired", "sourceIsRando")) {
        return rc;
    }
    // The MM world itself still loaded: the gate is on the combo commit, not on
    // the spoiler feature.
    if (gSaveContext.save.shipSaveInfo.rando.finalSeed != staged["finalSeed"].get<uint32_t>()) {
        return Fail(17, "the refusal took the MM world down with it (finalSeed %08X != spoiler's %08X)",
                    gSaveContext.save.shipSaveInfo.rando.finalSeed, staged["finalSeed"].get<uint32_t>());
    }

    // ---- Leg 2: paired, but a DIFFERENT world's seed -----------------------
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    ArmPairing(kSeedA ^ 0x00BADBADu, kHashA, kDigestA);
    ResetRefusalSurface();
    if (int rc = driveLoad(selected, 20, &err)) {
        return Fail(rc, "divergent-seed load threw instead of refusing: %s", err);
    }
    if (Combo_CountForeignPlacements() != 0) {
        return Fail(21, "a spoiler naming seed %08X committed %d placement(s) into a session pairing seed %08X", kSeedA,
                    Combo_CountForeignPlacements(), kSeedA ^ 0x00BADBADu);
    }
    if (int rc = AssertRefused(22, "divergent seed", "sharedRandoSeed")) {
        return rc;
    }

    // ---- Leg 3: paired, divergent settings digest --------------------------
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    ArmPairing(kSeedA, kHashA ^ 0x00BADBADu, kDigestA);
    ResetRefusalSurface();
    if (int rc = driveLoad(selected, 30, &err)) {
        return Fail(rc, "divergent-settings load threw instead of refusing: %s", err);
    }
    if (Combo_CountForeignPlacements() != 0) {
        return Fail(31, "a spoiler naming settings %08X committed %d placement(s) into a session pairing %08X", kHashA,
                    Combo_CountForeignPlacements(), kHashA ^ 0x00BADBADu);
    }
    if (int rc = AssertRefused(32, "divergent settings", "sharedRandoSettingsHash")) {
        return rc;
    }

    // ---- Leg 4: paired, divergent frozen MM profile ------------------------
    // The #570 term. Same seed and settings, different frozen MM profile: the
    // spoiler describes a world built from options this pair was not created
    // with.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    ArmPairing(kSeedA, kHashA, kDigestA ^ 0x00BADBADu);
    ResetRefusalSurface();
    if (int rc = driveLoad(selected, 40, &err)) {
        return Fail(rc, "divergent-profile load threw instead of refusing: %s", err);
    }
    if (Combo_CountForeignPlacements() != 0) {
        return Fail(41, "a spoiler naming profile %08X committed %d placement(s) into a pair frozen at %08X", kDigestA,
                    Combo_CountForeignPlacements(), kDigestA ^ 0x00BADBADu);
    }
    if (int rc = AssertRefused(42, "divergent profile", "mmProfileDigest")) {
        return rc;
    }

    // ---- Leg 5: NON-VACUITY — the identity matches, so it commits ----------
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    ArmPairing(kSeedA, kHashA, kDigestA);
    ResetRefusalSurface();
    if (int rc = driveLoad(selected, 50, &err)) {
        return Fail(rc, "the matching-identity load threw: %s", err);
    }
    if (Combo_CountForeignPlacements() != 1) {
        return Fail(51,
                    "a spoiler whose identity NAMES this session's pairing reconstructed %d placement(s), expected "
                    "1 — the gate refuses everything and locks nothing",
                    Combo_CountForeignPlacements());
    }
    {
        const SharedItem* got = Combo_GetForeignPlacementForCheck((uint16_t)host);
        if (got == nullptr || got->originGame != placed.originGame || got->id != placed.id) {
            return Fail(52, "the matching load did not rebuild host %s's placement", hostName);
        }
    }
    if (RsbsSave_IsSlotWritable(kSlot) != 1 || RsbsSave_GetSlotRefuseReason(kSlot) != (int)RSBS_REFUSE_NONE) {
        return Fail(53, "a matching identity latched the slot anyway (writable=%d reason=%d)",
                    RsbsSave_IsSlotWritable(kSlot), RsbsSave_GetSlotRefuseReason(kSlot));
    }

    // ---- Leg 6: SCOPE — upstream's standalone spoiler load is untouched ----
    // No "foreign" section means nothing that could reach gComboCtx or the
    // shared records, so an unpaired session must load it exactly as before:
    // no refusal, no latch, and the world applied.
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    ResetRefusalSurface();
    if (int rc = driveLoad(soloName, 60, &err)) {
        return Fail(rc, "a standalone (non-combo) spoiler failed to load: %s", err);
    }
    if (Combo_CountForeignPlacements() != 0) {
        return Fail(61, "a spoiler with no foreign section produced %d placement(s)", Combo_CountForeignPlacements());
    }
    if (RsbsSave_IsSlotWritable(kSlot) != 1 || RsbsSave_GetSlotRefuseReason(kSlot) != (int)RSBS_REFUSE_NONE) {
        return Fail(62,
                    "a standalone spoiler load latched the slot (writable=%d reason=%d) — the gate must sit on the "
                    "COMBO commit, not on the spoiler feature",
                    RsbsSave_IsSlotWritable(kSlot), RsbsSave_GetSlotRefuseReason(kSlot));
    }
    if (gSaveContext.save.shipSaveInfo.rando.finalSeed != solo["finalSeed"].get<uint32_t>()) {
        return Fail(63, "the standalone spoiler's world was not applied");
    }

    // Leave process-global state clean for whatever test runs next.
    RsbsSave_ResetSlotSessionState();
    RsbsSave_SetActiveSlot(-1);
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    CVarSetString("gRando.SpoilerFile", "");
    CVarSetInteger("gRando.SpoilerFileIndex", 0);

    printf("[TEST] PASS: the spoiler foreign section commits only under a matching pairing identity; every "
           "divergent term refuses through the #533 surface; standalone spoiler loads untouched\n");
    return 0;
}

extern "C" int MM_ForeignPickupGate_RunHeadless(void) {
    printf("[TEST] mm-foreign-pickup-gate: an unpaired session authors no durable shared-item record, whatever the "
           "placement table holds (#610)\n");

    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();
    Combo_ClearSharedItemOutbox();

    const ComboForeignItemDef* pool = nullptr;
    const int poolCount = Combo_GetForeignItemPoolFor((uint8_t)GAME_OOT, &pool);
    if (poolCount <= 0 || pool == nullptr) {
        return Fail(70, "the OoT foreign pool is not registered in this build");
    }

    RandoCheckId host = RC_UNKNOWN;
    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        if (randoStaticCheck.randoCheckId == RC_UNKNOWN) {
            continue;
        }
        const RandoSaveCheck previous = RANDO_SAVE_CHECKS[randoCheckId];
        RANDO_SAVE_CHECKS[randoCheckId].randoItemId = RI_JUNK;
        RANDO_SAVE_CHECKS[randoCheckId].shuffled = true;
        RANDO_SAVE_CHECKS[randoCheckId].skipped = false;
        if (Rando::Foreign::IsEligibleHost(randoCheckId)) {
            host = randoCheckId;
            break;
        }
        RANDO_SAVE_CHECKS[randoCheckId] = previous;
    }
    if (host == RC_UNKNOWN) {
        return Fail(71, "no eligible foreign host exists in the check table");
    }

    // The corrupted state the rest of #610 exists to prevent, reached here
    // directly: a populated placement table with NO live pairing. Whatever
    // route put it there, collecting the check must not author a durable
    // record — `Combo_RecordSharedItem` has no identity parameter, so the
    // record a later genuine pair redeems would carry no evidence of where it
    // came from.
    if (Combo_SetForeignPlacement((uint16_t)host, pool[0].item) < 0) {
        return Fail(72, "could not seed the fixture placement");
    }
    if (Combo_ForeignPairingActive()) {
        return Fail(73, "the unpaired fixture reads as paired");
    }

    if (Rando::Foreign::RecordForeignPickup(host)) {
        return Fail(74, "an UNPAIRED session recorded a foreign pickup as a durable shared item (#610)");
    }
    if (Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true) != 0) {
        return Fail(75,
                    "an unpaired pickup left %d durable shared-item record(s) — a later genuine paired OoT arrival "
                    "would redeem them into a world whose fill never placed them",
                    Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/true));
    }

    // ---- NON-VACUITY: the same pickup under a live pairing DOES record -----
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0x0610C0DEu;
    gComboCtx.sharedRandoSettingsHash = 0x0610FEEDu;
    if (!Combo_ForeignPairingActive()) {
        return Fail(76, "the paired fixture does not read as paired");
    }
    if (!Rando::Foreign::RecordForeignPickup(host)) {
        return Fail(77, "a PAIRED session failed to record a foreign pickup — the gate is not a gate, it is an off "
                        "switch");
    }
    if (Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false) != 1) {
        return Fail(78, "a paired pickup recorded %d un-redeemed shared item(s), expected 1",
                    Combo_CountSharedItems(GAME_OOT, /*includeRedeemed=*/false));
    }

    Combo_ClearSharedItemOutbox();
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    ComboContext_Init();

    printf("[TEST] PASS: foreign pickups author durable shared-item records only under a live pairing\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
