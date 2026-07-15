#include "BenPort.h"
#include <ship/resource/ResourceManager.h>
#include "2s2h/resource/type/Scene.h"
#include <ship/utils/StringHelper.h>
#include "2s2h/resource/type/TextMM.h"
#include <message_data_static.h>

extern "C" MessageTableEntry* sMessageTableNES;
extern "C" MessageTableEntry* sMessageTableCredits;

// static: OoT's z_message_OTR.cpp has its own OTRMessage_LoadTable (single-exe, #344)
static MessageTableEntry* OTRMessage_LoadTable(const char* filePath, bool isNES) {
    auto file = std::static_pointer_cast<S2H::TextMM>(
        Ship::Context::GetInstance()->GetResourceManager()->LoadResource(filePath));

    if (file == nullptr)
        return nullptr;

    // Allocate room for an additional message
    // OTRTODO: Should not be malloc'ing here. It's fine for now since we check elsewhere that the message table is
    // already null.
    MessageTableEntry* table = (MessageTableEntry*)malloc(sizeof(MessageTableEntry) * (file->messages.size() + 1));

    for (size_t i = 0; i < file->messages.size(); i++) {
        table[i].textId = file->messages[i].id;
        table[i].typePos = (file->messages[i].textboxType << 4) | file->messages[i].textboxYPos;
        table[i].segment = (const char*)malloc(file->messages[i].msg.size() + 11);

        auto segment = (char*)table[i].segment;

        segment[0] = file->messages[i].textboxType;
        segment[1] = file->messages[i].textboxYPos;
        segment[2] = file->messages[i].icon;
        segment[3] = (file->messages[i].nextMessageID & 0xFF00) >> 8;
        segment[4] = (file->messages[i].nextMessageID & 0x00FF);
        segment[5] = (file->messages[i].firstItemCost & 0xFF00) >> 8;
        segment[6] = (file->messages[i].firstItemCost & 0x00FF);
        segment[7] = (file->messages[i].secondItemCost & 0xFF00) >> 8;
        segment[8] = (file->messages[i].secondItemCost & 0x00FF);
        segment[9] = 0xFF;
        segment[10] = 0xFF;

        memcpy((void*)(&table[i].segment[11]), file->messages[i].msg.c_str(), file->messages[i].msg.size());

        table[i].msgSize = file->messages[i].msg.size() + 11;

        // if (isNES && file->messages[i].id == 0xFFFC)
        //_message_0xFFFC_nes = (char*)file->messages[i].msg.c_str();
    }

    return table;
}

// MM_-prefixed: OoT's z_message_OTR.cpp owns the extern "C" OTRMessage_Init symbol (single-exe, #344).
// Idempotent so MM_Game_Init can call it on every (re-)entry without leaking tables.
extern "C" void MM_OTRMessage_Init() {
    if (sMessageTableNES != NULL) {
        return;
    }

    sMessageTableNES = OTRMessage_LoadTable("text/message_data_static/message_data_static", true);
    if (sMessageTableNES == NULL) {
        // Every MM textbox (including scene title cards fired right after a
        // scene load, #344) dereferences this table — make the root cause
        // visible instead of crashing later in MM_Message_FindMessage.
        fprintf(stderr, "[MM] FATAL: failed to load message table "
                        "'text/message_data_static/message_data_static' — mm.o2r is incomplete\n");
        fflush(stderr);
        return;
    }

    auto file2 = std::static_pointer_cast<S2H::TextMM>(Ship::Context::GetInstance()->GetResourceManager()->LoadResource(
        "text/staff_message_data_static/staff_message_data_static"));
    if (file2 == nullptr) {
        fprintf(stderr, "[MM] WARNING: failed to load credits message table "
                        "'text/staff_message_data_static/staff_message_data_static'\n");
        fflush(stderr);
        return;
    }
    sMessageTableCredits = (MessageTableEntry*)malloc(sizeof(MessageTableEntry) * file2->messages.size());

    for (size_t i = 0; i < file2->messages.size(); i++) {
        sMessageTableCredits[i].textId = file2->messages[i].id;
        sMessageTableCredits[i].typePos = (file2->messages[i].textboxType << 4) | file2->messages[i].textboxYPos;
        sMessageTableCredits[i].segment = file2->messages[i].msg.c_str();
        sMessageTableCredits[i].msgSize = file2->messages[i].msg.size();
    }
}
