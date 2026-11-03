/**
 * @file test_lus_hardening.cpp
 * @brief Lock for the libultraship hardening batch (#1384) on the parts that run headless.
 *
 * LusHardening_RunHeadless is the ROM-free `lus-hardening` row. It is its own translation unit, not a file-scope
 * include of test_runner.cpp, because fast/interpreter.h brings in the whole LUS GBI. No Ship::Context, no
 * window, no archive. Legs:
 *   - config-atomic-save (LUS-9): Ship::Config::Save used to truncate the config in place, so a crash mid-write
 *     left shipofharkinian.json empty and the next launch reset every setting for both games. Save now stages
 *     "<config>.tmp" and renames it over the config. With the temporary path blocked by a directory, a save
 *     must leave the live file byte-identical; once unblocked it must land and leave no ".tmp" behind.
 *   - texture-null-address (LUS-2): a tile whose TMEM slot and the other slot are both empty must not be
 *     cached under a null key or uploaded.
 *   - texture-tmem-fallback (LUS-2): a tile on an empty slot imports the other slot's texture, and the cache
 *     key, the decoded bytes and the uploaded size all come from that slot; the tile's own slot is restored.
 *   - image-signature-bounds (LUS-3): gfx_check_image_signature returns 0 for a low address and, on 64-bit,
 *     one above the 48-bit user range, without reading it. It runs last: against a libultraship without the
 *     filter it dereferences a null Ship::Context, so the row fails by crashing there, after the other legs
 *     have printed their verdicts.
 *   - audio-player-virtual-dtor (LUS-12, compile time): ~AudioPlayer is virtual, so deleting a player through an
 *     AudioPlayer pointer runs the subclass destructor that stops the WASAPI stream and unregisters its device
 *     callback. Audio's make_shared already records the subclass; this keeps any other owner safe. A
 *     static_assert below holds it, so a libultraship without the change does not compile this file.
 * The DX11, DXGI, WASAPI and Windows crash-filter halves of the batch (LUS-6, LUS-10, LUS-12) run only on
 * Windows desktop paths the headless tiers never reach, so they have no runtime leg here.
 */

// Config.h brings in the Windows SDK through ship/audio (WASAPI). Include it the way OTRGlobals.cpp and
// BenPort.cpp do: NOMINMAX, and the Fast3D window, its GUI and the interpreter before any Windows header.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fast/Fast3dWindow.h>
// interpreter.h is self-contained: gfx_rendering_api.h includes LUS's patched imconfig.h, which defines ImTextureID.
#include <fast/interpreter.h>
#include <ship/config/Config.h>
#include <ship/audio/AudioPlayer.h>

#include "../test_scratch.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

// N64 texture format and size codes (G_IM_FMT_RGBA, G_IM_SIZ_16b).
constexpr uint8_t kLhFmtRgba = 0;
constexpr uint8_t kLhSiz16b = 2;

// Records the uploads the import paths make; everything else is inert.
class LhStubRapi final : public Fast::GfxRenderingAPI {
  public:
    uint32_t mNextTexture = 1;
    int mUploads = 0;
    uint32_t mLastWidth = 0;
    uint32_t mLastHeight = 0;
    uint8_t mLastFirstTexel[4] = { 0, 0, 0, 0 };

    const char* GetName() override {
        return "lus-hardening stub";
    }
    int GetMaxTextureSize() override {
        return 8192;
    }
    Fast::GfxClipParameters GetClipParameters() override {
        return { false, false };
    }
    void UnloadShader(Fast::ShaderProgram* oldPrg) override {
    }
    void LoadShader(Fast::ShaderProgram* newPrg) override {
    }
    Fast::ShaderProgram* CreateAndLoadNewShader(uint64_t shaderId0, uint32_t shaderId1) override {
        return nullptr;
    }
    Fast::ShaderProgram* LookupShader(uint64_t shaderId0, uint32_t shaderId1) override {
        return nullptr;
    }
    void ShaderGetInfo(Fast::ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) override {
        if (numInputs != nullptr) {
            *numInputs = 0;
        }
        if (usedTextures != nullptr) {
            usedTextures[0] = false;
            usedTextures[1] = false;
        }
    }
    uint32_t NewTexture() override {
        return mNextTexture++;
    }
    void SelectTexture(int tile, uint32_t textureId) override {
    }
    void UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) override {
        mUploads++;
        mLastWidth = width;
        mLastHeight = height;
        if (rgba32Buf != nullptr && width != 0 && height != 0) {
            for (int c = 0; c < 4; c++) {
                mLastFirstTexel[c] = rgba32Buf[c];
            }
        }
    }
    void SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) override {
    }
    void SetDepthTestAndMask(bool depth_test, bool z_upd) override {
    }
    void SetZmodeDecal(bool decal) override {
    }
    void SetViewport(int x, int y, int width, int height) override {
    }
    void SetScissor(int x, int y, int width, int height) override {
    }
    void SetUseAlpha(bool useAlpha) override {
    }
    void DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) override {
    }
    void Init() override {
    }
    void OnResize() override {
    }
    void StartFrame() override {
    }
    void EndFrame() override {
    }
    void FinishRender() override {
    }
    int CreateFramebuffer() override {
        return 0;
    }
    void UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                     bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                     bool can_extract_depth) override {
    }
    void StartDrawToFramebuffer(int fbId, float noiseScale) override {
    }
    void CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0,
                         int dstX1, int dstY1) override {
    }
    void ClearFramebuffer(bool color, bool depth) override {
    }
    void ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) override {
    }
    void ResolveMSAAColorBuffer(int fbIdTarger, int fbIdSrc) override {
    }
    std::unordered_map<std::pair<float, float>, uint16_t, Fast::hash_pair_ff>
    GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) override {
        return {};
    }
    void* GetFramebufferTextureId(int fbId) override {
        return nullptr;
    }
    void SelectTextureFb(int fbId) override {
    }
    void DeleteTexture(uint32_t texId) override {
    }
    void SetTextureFilter(Fast::FilteringMode mode) override {
    }
    Fast::FilteringMode GetTextureFilter() override {
        return Fast::FILTER_NONE;
    }
    void SetSrgbMode() override {
    }
    // LUS's imconfig.h makes ImTextureID the macro `void*`, not a typedef, so `ImTextureID{}` does not parse;
    // `{}` value-initializes whatever the return type is.
    ImTextureID GetTextureById(int id) override {
        return {};
    }
};

bool LhReadFile(const fs::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// One save of `value` under the probe key, through a fresh Config the way a launch would load it.
void LhSaveProbe(const fs::path& cfgPath, const char* value) {
    Ship::Config cfg(cfgPath.string());
    cfg.SetString("LusHardening.Probe", value);
    cfg.Save();
}

int LhConfigAtomicSaveLeg() {
    int failures = 0;
    std::error_code ec;
    const fs::path dir = rsbs::test::ScratchDir("lus-hardening");
    const fs::path cfgPath = dir / "lus-hardening.json";
    const fs::path tmpPath = dir / "lus-hardening.json.tmp";

    LhSaveProbe(cfgPath, "first");
    std::string before;
    if (!LhReadFile(cfgPath, before) || before.find("first") == std::string::npos) {
        printf("[lus-hardening] FAIL: config-atomic-save setup: the first save did not write \"first\" to %s\n",
               cfgPath.string().c_str());
        return 1;
    }
    if (fs::exists(tmpPath, ec)) {
        printf("[lus-hardening] FAIL: config-atomic-save: a completed save left %s behind\n", tmpPath.string().c_str());
        failures++;
    }

    // A directory where the temporary file goes: the save cannot stage its write, so it must not touch the
    // live config. A save that writes the config in place goes straight through and replaces it.
    fs::create_directory(tmpPath, ec);
    if (!fs::is_directory(tmpPath, ec)) {
        printf("[lus-hardening] FAIL: config-atomic-save setup: could not block %s with a directory\n",
               tmpPath.string().c_str());
        return failures + 1;
    }
    LhSaveProbe(cfgPath, "second");
    std::string blocked;
    if (!LhReadFile(cfgPath, blocked)) {
        printf("[lus-hardening] FAIL: config-atomic-save: the live config is gone after a save that could not "
               "stage its temporary file\n");
        failures++;
    } else if (blocked != before) {
        printf("[lus-hardening] FAIL: config-atomic-save: the live config changed (%s) although the save could "
               "not stage its temporary file; Save writes the config in place, so a crash mid-write loses it\n",
               blocked.find("second") != std::string::npos ? "it now holds \"second\"" : "its bytes differ");
        failures++;
    }
    fs::remove(tmpPath, ec);

    LhSaveProbe(cfgPath, "third");
    std::string after;
    if (!LhReadFile(cfgPath, after) || after.find("third") == std::string::npos) {
        printf("[lus-hardening] FAIL: config-atomic-save: once the temporary path was free again, the save did "
               "not land \"third\" in the config\n");
        failures++;
    }
    if (fs::exists(tmpPath, ec)) {
        printf("[lus-hardening] FAIL: config-atomic-save: the last save left %s behind\n", tmpPath.string().c_str());
        failures++;
    }
    {
        Ship::Config reloaded(cfgPath.string());
        if (reloaded.GetString("LusHardening.Probe") != "third") {
            printf("[lus-hardening] FAIL: config-atomic-save: a fresh Config does not read back \"third\"\n");
            failures++;
        }
    }
    return failures;
}

void LhEmptySlot(Fast::Interpreter& gfx, int slot) {
    gfx.mRdp->loaded_texture[slot].addr = nullptr;
    gfx.mRdp->loaded_texture[slot].orig_size_bytes = 0;
    gfx.mRdp->loaded_texture[slot].size_bytes = 0;
    gfx.mRdp->loaded_texture[slot].full_image_line_size_bytes = 0;
    gfx.mRdp->loaded_texture[slot].line_size_bytes = 0;
    gfx.mRdp->loaded_texture[slot].tex_flags = 0;
    gfx.mRdp->loaded_texture[slot].raw_tex_metadata = {};
    gfx.mRdp->loaded_texture[slot].masked = false;
    gfx.mRdp->loaded_texture[slot].blended = false;
}

// Tile 0 is a 4x4 RGBA16 tile (8 bytes a line) on TMEM slot 0; both slots start empty.
void LhResetTextureState(Fast::Interpreter& gfx, LhStubRapi& rapi) {
    gfx.TextureCacheClear();
    LhEmptySlot(gfx, 0);
    LhEmptySlot(gfx, 1);
    gfx.mRdp->texture_tile[0].fmt = kLhFmtRgba;
    gfx.mRdp->texture_tile[0].siz = kLhSiz16b;
    gfx.mRdp->texture_tile[0].line_size_bytes = 8;
    gfx.mRdp->texture_tile[0].palette = 0;
    gfx.mRdp->texture_tile[0].tmem_index = 0;
    rapi.mUploads = 0;
    rapi.mLastWidth = 0;
    rapi.mLastHeight = 0;
}

int LhTextureNullAddressLeg(Fast::Interpreter& gfx, LhStubRapi& rapi) {
    int failures = 0;
    LhResetTextureState(gfx, rapi);

    gfx.ImportTexture(0, 0, false);
    if (!gfx.mTextureCache.map.empty()) {
        printf("[lus-hardening] FAIL: texture-null-address: importing a tile with both TMEM slots empty cached %zu "
               "entr%s (a null-address key)\n",
               gfx.mTextureCache.map.size(), gfx.mTextureCache.map.size() == 1 ? "y" : "ies");
        failures++;
    }
    if (rapi.mUploads != 0) {
        printf("[lus-hardening] FAIL: texture-null-address: importing a tile with both TMEM slots empty uploaded "
               "%d texture(s) (last %ux%u)\n",
               rapi.mUploads, rapi.mLastWidth, rapi.mLastHeight);
        failures++;
    }
    return failures;
}

int LhTextureTmemFallbackLeg(Fast::Interpreter& gfx, LhStubRapi& rapi) {
    int failures = 0;
    LhResetTextureState(gfx, rapi);

    // 4x4 RGBA16, big-endian. The first texel is opaque pure red (0xF801); the rest are zero.
    std::vector<uint8_t> texels(32, 0);
    texels[0] = 0xF8;
    texels[1] = 0x01;
    gfx.mRdp->loaded_texture[1].addr = texels.data();
    gfx.mRdp->loaded_texture[1].orig_size_bytes = 32;
    gfx.mRdp->loaded_texture[1].size_bytes = 32;
    gfx.mRdp->loaded_texture[1].full_image_line_size_bytes = 8;
    gfx.mRdp->loaded_texture[1].line_size_bytes = 8;

    gfx.ImportTexture(0, 0, false);

    if (gfx.mTextureCache.map.size() != 1) {
        printf("[lus-hardening] FAIL: texture-tmem-fallback: expected exactly one cache entry, found %zu\n",
               gfx.mTextureCache.map.size());
        failures++;
    } else {
        const Fast::TextureCacheKey& key = gfx.mTextureCache.map.begin()->first;
        if (key.texture_addr != texels.data() || key.size_bytes != 32) {
            printf("[lus-hardening] FAIL: texture-tmem-fallback: the cache key is %s with size %u, not slot 1's "
                   "texture with size 32\n",
                   key.texture_addr == nullptr ? "a null address" : "another address", (unsigned)key.size_bytes);
            failures++;
        }
    }
    if (rapi.mUploads != 1 || rapi.mLastWidth != 4 || rapi.mLastHeight != 4) {
        printf("[lus-hardening] FAIL: texture-tmem-fallback: expected one 4x4 upload decoded from slot 1, got %d "
               "upload(s), last %ux%u\n",
               rapi.mUploads, rapi.mLastWidth, rapi.mLastHeight);
        failures++;
    } else if (rapi.mLastFirstTexel[0] != 255 || rapi.mLastFirstTexel[1] != 0 || rapi.mLastFirstTexel[2] != 0 ||
               rapi.mLastFirstTexel[3] != 255) {
        printf("[lus-hardening] FAIL: texture-tmem-fallback: the first uploaded texel is %u,%u,%u,%u, not slot 1's "
               "opaque red 255,0,0,255\n",
               (unsigned)rapi.mLastFirstTexel[0], (unsigned)rapi.mLastFirstTexel[1], (unsigned)rapi.mLastFirstTexel[2],
               (unsigned)rapi.mLastFirstTexel[3]);
        failures++;
    }
    if (gfx.mRdp->texture_tile[0].tmem_index != 0) {
        printf("[lus-hardening] FAIL: texture-tmem-fallback: tile 0 was left on TMEM slot %u; the import must "
               "restore it to slot 0\n",
               (unsigned)gfx.mRdp->texture_tile[0].tmem_index);
        failures++;
    }

    // Nothing may keep pointing at `texels` once it goes out of scope.
    LhResetTextureState(gfx, rapi);
    return failures;
}

int LhImageSignatureBoundsLeg() {
    int failures = 0;
    // Flushed first: without the filter, the low address below is read through a null Ship::Context.
    fflush(stdout);
    if (Fast::gfx_check_image_signature(reinterpret_cast<const char*>(static_cast<uintptr_t>(0x1000))) != 0) {
        printf("[lus-hardening] FAIL: image-signature-bounds: address 0x1000 passed the signature check\n");
        failures++;
    }
#if UINTPTR_MAX > 0xFFFFFFFFu
    const char* aboveUserRange = reinterpret_cast<const char*>(static_cast<uintptr_t>(0x0001000000000000ull));
    if (Fast::gfx_check_image_signature(aboveUserRange) != 0) {
        printf("[lus-hardening] FAIL: image-signature-bounds: address 0x0001000000000000 passed the signature "
               "check\n");
        failures++;
    }
#endif
    return failures;
}

// audio-player-virtual-dtor (LUS-12).
static_assert(std::has_virtual_destructor_v<Ship::AudioPlayer>,
              "~AudioPlayer must be virtual so a player deleted through its base runs the subclass destructor");

} // namespace

extern "C" int LusHardening_RunHeadless(void) {
    int failures = 0;

    failures += LhConfigAtomicSaveLeg();
    printf("[lus-hardening] config-atomic-save done (%d failure(s) so far)\n", failures);
    fflush(stdout);

    {
        // The stub and the upload buffer outlive the interpreter that points at them.
        LhStubRapi rapi;
        std::vector<uint8_t> uploadBuffer(4096, 0);
        auto gfx = std::make_unique<Fast::Interpreter>();
        gfx->mRapi = &rapi;
        gfx->mTexUploadBuffer = uploadBuffer.data();

        failures += LhTextureNullAddressLeg(*gfx, rapi);
        failures += LhTextureTmemFallbackLeg(*gfx, rapi);

        gfx->TextureCacheClear();
        gfx->mTexUploadBuffer = nullptr;
        gfx->mRapi = nullptr;
    }
    printf("[lus-hardening] texture legs done (%d failure(s) so far)\n", failures);
    fflush(stdout);

    failures += LhImageSignatureBoundsLeg();

    if (failures != 0) {
        printf("[lus-hardening] %d assertion(s) failed\n", failures);
        fflush(stdout);
        return 1;
    }
    printf("[lus-hardening] PASS: a blocked config save leaves the live file intact and an unblocked one lands "
           "atomically; an empty tile caches nothing and an empty slot imports the other slot's texture; the "
           "signature check rejects out-of-range addresses\n");
    fflush(stdout);
    return 0;
}
