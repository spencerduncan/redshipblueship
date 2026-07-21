#pragma once

#include "libultraship/libultraship.h"

namespace SOH {
class ConfigVersion1Updater final : public Ship::ConfigVersionUpdater {
  public:
    ConfigVersion1Updater();
    void Update(Ship::Config* conf);
};

class ConfigVersion2Updater final : public Ship::ConfigVersionUpdater {
  public:
    ConfigVersion2Updater();
    void Update(Ship::Config* conf);
};

class ConfigVersion3Updater final : public Ship::ConfigVersionUpdater {
  public:
    ConfigVersion3Updater();
    void Update(Ship::Config* conf);
};

class ConfigVersion4Updater final : public Ship::ConfigVersionUpdater {
  public:
    ConfigVersion4Updater();
    void Update(Ship::Config* conf);
};

class ConfigVersion5Updater final : public Ship::ConfigVersionUpdater {
  public:
    ConfigVersion5Updater();
    void Update(Ship::Config* conf);
};

class ConfigVersion6Updater final : public Ship::ConfigVersionUpdater {
  public:
    ConfigVersion6Updater();
    void Update(Ship::Config* conf);
};

/**
 * RSBS version 7 — cross-game key convergence (ADR 0003 §5.1, §6.2-6.3).
 *
 * Moves MM onto OoT's spelling for settings both games mean identically, so a
 * volume slider or a tunic colour set in one game is the same setting in the
 * other. Zero-loss: every row copies before it clears, and a row whose legacy
 * key is absent is a no-op.
 */
class ConfigVersion7Updater final : public Ship::ConfigVersionUpdater {
  public:
    ConfigVersion7Updater();
    void Update(Ship::Config* conf);
};

/**
 * @brief The ADR 0003 §6.3 conflict rule, as a pure predicate.
 *
 * A config can legitimately hold BOTH spellings of a converged setting: the
 * OoT one written by OoT's live menu, and the MM one hand-edited or imported
 * from a 2Ship preset. They can disagree. CVarCopy overwrites unconditionally,
 * so the plain rename pattern would let the MM-spelled value clobber the OoT
 * one — the wrong winner.
 *
 * Rule: **OoT's value wins.** The MM-spelled value is adopted only when the
 * OoT key is absent. The OoT-spelled value is the one the user could have set
 * through a live menu, so it is the one they last saw take effect.
 *
 * Factored out (rather than inlined into Update) so the ROM-free CTest tier can
 * exercise the decision itself — the same shape as the sequence-map bound
 * functions #371/#378 pulled out for `--test seq-map-bounds`.
 *
 * @return true if the legacy value should be adopted into the canonical key.
 */
bool ShouldAdoptLegacyValue(bool legacyPresent, bool canonicalPresent);

/**
 * @brief Float scale (0..1) to OoT's integer percent (0-100), clamped.
 *
 * MM stored volumes as a float scale; OoT stores integer percent. Converging
 * the key without converting the value would leave a Float-typed CVar under a
 * key whose readers all call CVarGetInteger — and libultraship returns the
 * DEFAULT on a type mismatch, so the user's imported volume would silently
 * never apply. Clamped because a hand-edited config can hold anything.
 */
int32_t VolumePercentFromFloatScale(float scale);
} // namespace SOH
