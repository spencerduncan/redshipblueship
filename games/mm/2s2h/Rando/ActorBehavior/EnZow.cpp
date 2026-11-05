#include "ActorBehavior.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipUtils.h"
#include "2s2h/Rando/HintArea.h" // #575 H2: a hint names the check's area (scene name outside single-exe)
#ifdef RSBS_SINGLE_EXECUTABLE
#include "2s2h/Rando/HintSources.h" // #1475: the hint tracker's resolver and read hook
#endif

extern "C" {
#include "variables.h"
}

void ApplyHookshotHint(u16* textId, bool* loadFromMessageTable) {
#ifdef RSBS_SINGLE_EXECUTABLE
    // #1475: the combined Hint Tracker's resolver builds this text (Rando/
    // HintSources.h, upstream's expression moved there), so the tracker shows
    // exactly this; showing it records the Hookshot hint as read.
    std::string msg = Rando::HintSources::FixedText(RSBS_MM_HINT_SRC_HOOKSHOT, RANDO_SAVE_CHECKS, nullptr);
    Rando::HintSources::MarkRead(RSBS_MM_HINT_SRC_HOOKSHOT);
#else
    std::string msg = "I overheard those Pirates talk about treasure " RANDO_HINT_PREPOS("in")
                      " %y{{location}}%w that can help you reach the unreachable!";

    Rando::HintArea::ReplaceItemLocation(&msg, "{{location}}", RI_HOOKSHOT);
#endif

    CustomMessage::Entry entry = {
        .nextMessageID = (u16)0xFFFF,
        .msg = msg,
    };

    CustomMessage::LoadCustomMessageIntoFont(entry);
    *loadFromMessageTable = false;
}

void Rando::ActorBehavior::InitEnZowBehavior() {
    bool shouldRegister = IS_RANDO && RANDO_SAVE_OPTIONS[RO_HINTS_HOOKSHOT];

    COND_ID_HOOK(OnOpenText, 0x12FD, shouldRegister, ApplyHookshotHint);
    COND_ID_HOOK(OnOpenText, 0x12FA, shouldRegister, ApplyHookshotHint);
    COND_ID_HOOK(OnOpenText, 0x1301, shouldRegister, ApplyHookshotHint);
    COND_ID_HOOK(OnOpenText, 0x12FF, shouldRegister, ApplyHookshotHint);
    COND_ID_HOOK(OnOpenText, 0x12F8, shouldRegister, ApplyHookshotHint);
    COND_ID_HOOK(OnOpenText, 0x12F3, shouldRegister, ApplyHookshotHint);
}
