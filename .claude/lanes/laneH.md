# Lane H — #661: pin the Happy Mask Shop interior out of OoT's entrance shuffle

**Branch:** `claude/661-pin-mask-shop-entrance`

**Task:** Operator ruling: **pin**, not dynamic resolution. In single-exe
builds, the Happy Mask Shop interior pair
(`RR_THE_MARKET ↔ RR_MARKET_MASK_SHOP`) must never enter any OoT entrance-
shuffle pool (Interior, mixed, decoupled, "all interiors") when a paired
world is requested — the combo's crossing is keyed on the vanilla entrance
ids and silently breaks otherwise. Wrap the vendored edit in the file's
single-exe guard; check the entrance tracker rows and spoiler still describe
it as vanilla. Lock non-vacuously: RED (shuffled) without the pin, GREEN
(pinned vanilla) with it, under interior shuffle plus the strongest
pool-mixing setting. Digests should stay byte-identical unless the pin
changes RNG consumption when interior shuffle is on — verify, and only re-pin
if they move. Does not touch ADR 0010 (lane F owns it this wave).

**Files owned:** `games/oot/soh/Enhancements/randomizer/entrance.cpp` (pool
construction), `randomizer_entrance_tracker.cpp` only if a display fix is
needed, new tests.

**Keywords:** `Fixes #661`, `Refs #644 #645 #573`.
