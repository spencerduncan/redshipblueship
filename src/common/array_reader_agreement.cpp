/**
 * @file array_reader_agreement.cpp
 * @brief The #604 output comparison between OoT's and MM's 'OARR' Array readers.
 * See array_reader_agreement.h.
 */

#include "array_reader_agreement.h"
#include "f3dvtx_wire_layout.h"

#include <ship/resource/File.h>
#include <ship/resource/Resource.h>
#include <ship/resource/ResourceFactory.h>
#include <ship/utils/binarytools/BinaryReader.h>
#include <ship/utils/binarytools/MemoryStream.h>

#include <cstdio>
#include <cstring>
#include <exception>

// Each game's reader, handed over by its own TU (no game header here).
std::shared_ptr<Ship::ResourceFactory> OoT_CreateArrayFactory();
std::shared_ptr<Ship::ResourceFactory> MM_CreateArrayFactory();

namespace rsbs::array_readers {

namespace {

struct SyntheticVertex {
    int16_t ob[3];
    uint16_t flag;
    int16_t tc[2];
    uint8_t cn[4];
};

// Distinct everywhere: no two fields of one vertex share a value, tc[0] !=
// tc[1], ob[] mixes signs, flag has both bytes set, cn[] are four different
// bytes. Three vertices, so a reader that drifts by a byte per record shows up
// in the second and third rather than only as a short tail.
constexpr SyntheticVertex kSyntheticVertices[] = {
    { { 0x0123, -0x0456, 0x0789 }, 0x8A01, { 0x0B0C, -0x0D0E }, { 0x11, 0x22, 0x33, 0x44 } },
    { { -0x1357, 0x2468, -0x0ACE }, 0x7F02, { -0x1BDF, 0x2C3D }, { 0x55, 0x66, 0x77, 0x88 } },
    { { 0x7FFF, -0x8000, 0x0001 }, 0x0103, { 0x3E4F, -0x5061 }, { 0x99, 0xAA, 0xBB, 0xCC } },
};

void PutU16(std::vector<char>& out, uint16_t value) {
    out.push_back(static_cast<char>(value & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
}

void PutU32(std::vector<char>& out, uint32_t value) {
    PutU16(out, static_cast<uint16_t>(value & 0xFFFF));
    PutU16(out, static_cast<uint16_t>(value >> 16));
}

const char* FieldAt(size_t offsetInVertex) {
    if (offsetInVertex < 6) {
        return "ob";
    }
    if (offsetInVertex < 8) {
        return "flag";
    }
    if (offsetInVertex < 12) {
        return "tc";
    }
    return "cn";
}

} // namespace

std::vector<char> SyntheticVertexPayload() {
    std::vector<char> out;
    PutU32(out, kArrayTypeVertex);
    PutU32(out, static_cast<uint32_t>(SyntheticVertexCount()));
    for (const SyntheticVertex& v : kSyntheticVertices) {
        for (int16_t ob : v.ob) {
            PutU16(out, static_cast<uint16_t>(ob));
        }
        PutU16(out, v.flag);
        for (int16_t tc : v.tc) {
            PutU16(out, static_cast<uint16_t>(tc));
        }
        for (uint8_t cn : v.cn) {
            out.push_back(static_cast<char>(cn));
        }
    }
    return out;
}

size_t SyntheticVertexCount() {
    return sizeof(kSyntheticVertices) / sizeof(kSyntheticVertices[0]);
}

ParsedArray ParseArrayPayload(const std::shared_ptr<Ship::ResourceFactory>& factory, const std::vector<char>& payload,
                              Ship::Endianness byteOrder, const std::string& path) {
    ParsedArray parsed;
    if (factory == nullptr) {
        parsed.error = "no reader (creator returned null)";
        return parsed;
    }

    auto file = std::make_shared<Ship::File>();
    file->Buffer = std::make_shared<std::vector<char>>(payload);
    auto reader = std::make_shared<Ship::BinaryReader>(std::make_shared<Ship::MemoryStream>(file->Buffer));
    reader->SetEndianness(byteOrder);
    file->Reader = reader;
    file->IsLoaded = true;

    auto initData = std::make_shared<Ship::ResourceInitData>();
    initData->Path = path;
    initData->ByteOrder = byteOrder;
    initData->Type = 0x4F415252; // 'OARR'
    initData->ResourceVersion = 0;
    initData->Id = 0;
    initData->IsCustom = false;
    initData->Format = RESOURCE_FORMAT_BINARY;

    std::shared_ptr<Ship::IResource> resource;
    try {
        resource = factory->ReadResource(file, initData);
    } catch (const std::exception& e) {
        parsed.error = std::string("threw: ") + e.what();
        return parsed;
    } catch (...) {
        parsed.error = "threw a non-std exception";
        return parsed;
    }
    if (resource == nullptr) {
        parsed.error = "returned no resource";
        return parsed;
    }

    const size_t size = resource->GetPointerSize();
    const auto* bytes = static_cast<const uint8_t*>(resource->GetRawPointer());
    if (size > 0 && bytes == nullptr) {
        parsed.error = "reported " + std::to_string(size) + " bytes behind a null pointer";
        return parsed;
    }
    if (size > 0) {
        parsed.raw.assign(bytes, bytes + size);
    }
    parsed.ok = true;
    return parsed;
}

bool ReadersAgree(const std::shared_ptr<Ship::ResourceFactory>& a, const char* aName,
                  const std::shared_ptr<Ship::ResourceFactory>& b, const char* bName, const std::vector<char>& payload,
                  Ship::Endianness byteOrder, const std::string& path, std::string* diag) {
    auto say = [diag](const std::string& text) {
        if (diag != nullptr) {
            *diag = text;
        }
        return false;
    };

    const ParsedArray pa = ParseArrayPayload(a, payload, byteOrder, path);
    if (!pa.ok) {
        return say(std::string(aName) + " reader failed on " + path + ": " + pa.error);
    }
    const ParsedArray pb = ParseArrayPayload(b, payload, byteOrder, path);
    if (!pb.ok) {
        return say(std::string(bName) + " reader failed on " + path + ": " + pb.error);
    }
    // Non-vacuity: two readers that both produce nothing "agree" about nothing.
    if (pa.raw.empty()) {
        return say(std::string(aName) + " reader produced no vertex memory for " + path +
                   " (not a Vertex array, or the Vertex path is gone)");
    }
    if (pa.raw.size() % sizeof(Fast::F3DVtx) != 0) {
        return say(std::string(aName) + " reader produced " + std::to_string(pa.raw.size()) + " bytes for " + path +
                   ", not a whole number of 16-byte vertices");
    }
    if (pa.raw.size() != pb.raw.size()) {
        return say(path + ": " + aName + " produced " + std::to_string(pa.raw.size()) + " bytes, " + bName +
                   " produced " + std::to_string(pb.raw.size()));
    }
    for (size_t i = 0; i < pa.raw.size(); i++) {
        if (pa.raw[i] != pb.raw[i]) {
            char where[192];
            snprintf(where, sizeof(where), " first differ at byte %zu (vertex %zu, field %s): %s=0x%02X %s=0x%02X", i,
                     i / sizeof(Fast::F3DVtx), FieldAt(i % sizeof(Fast::F3DVtx)), aName, pa.raw[i], bName, pb.raw[i]);
            return say(path + ":" + where);
        }
    }
    if (diag != nullptr) {
        *diag = std::to_string(pa.raw.size() / sizeof(Fast::F3DVtx)) + " vertices, " + std::to_string(pa.raw.size()) +
                " bytes identical";
    }
    return true;
}

} // namespace rsbs::array_readers

extern "C" int Combo_ArrayReaders_BootCheck(void) {
    using namespace rsbs::array_readers;
    std::string diag;
    const bool agree =
        ReadersAgree(OoT_CreateArrayFactory(), "OoT", MM_CreateArrayFactory(), "MM", SyntheticVertexPayload(),
                     Ship::Endianness::Little, "rsbs/synthetic-vertex-array", &diag);
    if (!agree) {
        fprintf(stderr,
                "[RSBS] FATAL: OoT's and MM's 'OARR' Array readers no longer parse vertices identically (#604): %s. "
                "One process draws both games' models through whichever reader owns the loader slot, so this would "
                "garble geometry silently. Compare games/oot/soh/resource/importer/ArrayFactory.cpp with "
                "games/mm/2s2h/resource/importer/ArrayFactory.cpp.\n",
                diag.c_str());
        fflush(stderr);
        return 1;
    }
    return 0;
}
