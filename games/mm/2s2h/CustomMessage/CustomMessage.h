#ifndef CUSTOM_MESSAGE_H
#define CUSTOM_MESSAGE_H

// Not really sure what the best ID is for this, but it needs to be between 0-255
// because it's used as a u8 somewhere in the chain
#define CUSTOM_MESSAGE_ID 0x004B
#define BUFFER_SIZE 1280
#define MESSAGE_HEADER_SIZE 11

#ifdef __cplusplus

extern "C" {
#include "variables.h"
}

#include <string>

#ifdef RSBS_SINGLE_EXECUTABLE
// Single-exe symbol split (Lane C0, #392): OoT ships a GLOBAL-SCOPE
// `class CustomMessage` (soh/Enhancements/custom-message/
// CustomMessageManager.h) whose static member LoadVanillaMessageTableEntry
// (uint16_t) mangles IDENTICALLY to this namespace's free function under the
// Itanium ABI (return types don't participate), with a different return
// type — a silent cross-bind on Linux and a divergent resolution on MSVC.
// MM's namespace therefore nests under S2H, with a namespace alias so
// unqualified upstream MM callers (`CustomMessage::StartTextbox(...)`)
// compile unchanged. Only MM is renamed, per repo convention.
namespace S2H {
#endif

namespace CustomMessage {
struct Entry {
    uint8_t textboxType = 0;
    uint8_t textboxYPos = 0;
    uint8_t icon = 0xFE; // No Icon
    uint16_t nextMessageID = 0xFFFF;
    uint16_t firstItemCost = 0xFFFF;
    uint16_t secondItemCost = 0xFFFF;
    bool autoFormat = true;
    std::string msg = "";
};

void RegisterHooks();
void StartTextbox(std::string msg, Entry options = {});
void SetActiveCustomMessage(std::string msg, Entry options = {});

// Helpers
std::string RemoveColorCodes(const std::string& input);
void Replace(std::string* msg, const std::string& placeholder, const std::string& value);
void AddLineBreaks(std::string* msg);
void ReplaceColorChars(std::string* msg);
void EnsureMessageEnd(std::string* msg);
Entry LoadVanillaMessageTableEntry(u16 textId);
void LoadCustomMessageIntoFont(Entry entry);
} // namespace CustomMessage

#ifdef RSBS_SINGLE_EXECUTABLE
} // namespace S2H

namespace CustomMessage = S2H::CustomMessage;
#endif

#endif // __cplusplus
#endif // CUSTOM_MESSAGE_H
