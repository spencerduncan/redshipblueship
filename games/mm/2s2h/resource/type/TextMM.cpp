#include "TextMM.h"

namespace S2H {
MessageEntryMM* TextMM::GetPointer() {
    return messages.data();
}

size_t TextMM::GetPointerSize() {
    return messages.size() * sizeof(MessageEntryMM);
}
} // namespace S2H
