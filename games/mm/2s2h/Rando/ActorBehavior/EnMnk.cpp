#include "ActorBehavior.h"

// This interaction is skipped by the SkipLearningSonataOfAwakening and forced on for rando for now, this file simply
// handles queuing up the checks to be given.
void Rando::ActorBehavior::InitEnMnkBehavior() {
    COND_VB_SHOULD(VB_GIVE_ITEM_FROM_MNK, IS_RANDO, {
        *should = false;

        RANDO_SAVE_CHECKS[RC_DEKU_KINGS_CHAMBER_MONKEY].eligible = true;
    });

#ifdef RSBS_SINGLE_EXECUTABLE
    // #1203: unloading the tied-up monkey after its choice (the lesson skip's
    // warp-out does) teaches the vanilla Sonata in EnMnk_Destroy. The check above
    // is the seed's item for this spot, so the song is never given here. Answers
    // only; it does not re-arm the check (a second arm at unload is the #1220 class).
    COND_VB_SHOULD(VB_GIVE_ITEM_FROM_MNK_ON_DESTROY, IS_RANDO, { *should = false; });
#endif
}
