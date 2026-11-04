/**
 * @file test_texture_cache_freed_node.cpp
 * @brief Lock for the Fast3D texture cache leaving no rendering-state slot on a node it freed (#1352).
 *
 * Fast::Interpreter::mRenderingState.mTextures[] holds raw TextureCacheNode pointers into
 * mTextureCache.map. TextureCacheClear (G_INVALIDATETEXCACHE 0, gfx_texture_cache_clear), TextureCacheDelete
 * (G_INVALIDATETEXCACHE <addr>, which OoT issues every frame a textbox draws) and the LRU eviction in
 * TextureCacheLookup free map nodes. Before libultraship 62d22224 none of them cleared the slots, so when a
 * TMEM slot was reused without a reload GfxSpTri1 read, and on a filter or clamp change wrote, sampler state
 * through a freed node: a render-thread heap corruption that crashes later and elsewhere (#1063's "worked for
 * a while, then crashed"). The LRU list also stores map iterators, and the map was never reserved, so it
 * rehashed while it filled.
 *
 * TextureCacheFreedNode_RunHeadless is the ROM-free `texture-cache-freed-node` row. It is its own translation
 * unit, not a file-scope include of test_runner.cpp, because fast/interpreter.h brings in the whole LUS GBI.
 * It builds a standalone Fast::Interpreter (no Init, no window, no Ship::Context) over a stub rendering API
 * and drives the cache's own entry points:
 *   - delete:   two slots on one node and a third on another; TextureCacheDelete of the first node's address
 *               must null both of its slots and leave the third alone.
 *   - eviction: slot 0 on the least recently used node; misses through slot 1 until that node is evicted.
 *               Slot 0 must read null, and the bucket count must not change while the cache fills to its cap
 *               and evicts (no rehash, so the LRU's iterators stay valid).
 *   - clear:    slots on live nodes; TextureCacheClear must empty the map and null every slot.
 * Only pointer values are compared; nothing reads through a freed node, so the row fails cleanly, not by
 * crashing, against a libultraship without the fix.
 */

// interpreter.h is self-contained: gfx_rendering_api.h includes LUS's patched imconfig.h, which defines ImTextureID.
#include <fast/interpreter.h>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

// More synthetic textures than any cache cap libultraship has used (1024 in the fork and upstream, 500 before #1416).
constexpr size_t kTcfnKeyCount = 8192;

// TextureCacheLookup on a miss asks the rendering API for a texture id, selects it and resets its sampler.
// Nothing else is reached from the three cache entry points this row drives.
class TcfnStubRapi final : public Fast::GfxRenderingAPI {
  public:
    uint32_t mNextTexture = 1;
    int mNewTextureCalls = 0;

    const char* GetName() override {
        return "texture-cache-freed-node stub";
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
        mNewTextureCalls++;
        return mNextTexture++;
    }
    void SelectTexture(int tile, uint32_t textureId) override {
    }
    void UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) override {
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

// A synthetic texture's identity is its address; one byte each, so every key hashes apart.
Fast::TextureCacheKey TcfnKey(const uint8_t* addr) {
    Fast::TextureCacheKey key{};
    key.texture_addr = addr;
    key.palette_addrs[0] = nullptr;
    key.palette_addrs[1] = nullptr;
    key.fmt = 0;
    key.siz = 2;
    key.palette_index = 0;
    key.size_bytes = 32;
    return key;
}

bool TcfnCached(Fast::Interpreter& gfx, const uint8_t* addr) {
    return gfx.mTextureCache.map.find(TcfnKey(addr)) != gfx.mTextureCache.map.end();
}

// The slot's node is live and names `addr`. Only call while that node is known to be cached.
bool TcfnSlotOn(Fast::Interpreter& gfx, int slot, const uint8_t* addr) {
    const Fast::TextureCacheNode* node = gfx.mRenderingState.mTextures[slot];
    return node != nullptr && node->first.texture_addr == addr;
}

int TcfnDeleteLeg(Fast::Interpreter& gfx, const std::vector<uint8_t>& bytes) {
    int failures = 0;
    const int lastSlot = SHADER_MAX_TEXTURES - 1;
    const uint8_t* a = &bytes[0];
    const uint8_t* b = &bytes[1];

    gfx.TextureCacheClear();
    gfx.TextureCacheLookup(0, TcfnKey(a));
    gfx.TextureCacheLookup(1, TcfnKey(b));
    // A hit through another slot: two slots now share A's node, as the replacement and mask slots can.
    gfx.TextureCacheLookup(lastSlot, TcfnKey(a));
    if (!TcfnSlotOn(gfx, 0, a) || !TcfnSlotOn(gfx, lastSlot, a) || !TcfnSlotOn(gfx, 1, b)) {
        printf("[texture-cache-freed-node] FAIL: delete leg setup: slots 0/%d/1 do not name A/A/B after the lookups\n",
               lastSlot);
        return 1;
    }
    const Fast::TextureCacheNode* nodeB = gfx.mRenderingState.mTextures[1];

    gfx.TextureCacheDelete(a);
    if (TcfnCached(gfx, a)) {
        printf("[texture-cache-freed-node] FAIL: delete leg: TextureCacheDelete left A in the cache, so the leg "
               "locks nothing\n");
        return 1;
    }
    if (gfx.mRenderingState.mTextures[0] != nullptr) {
        printf("[texture-cache-freed-node] FAIL: delete leg: slot 0 still points at A's freed node after "
               "TextureCacheDelete; GfxSpTri1 would write sampler state through it\n");
        failures++;
    }
    if (gfx.mRenderingState.mTextures[lastSlot] != nullptr) {
        printf("[texture-cache-freed-node] FAIL: delete leg: slot %d (the second slot on A) still points at A's "
               "freed node after TextureCacheDelete\n",
               lastSlot);
        failures++;
    }
    if (!TcfnCached(gfx, b) || gfx.mRenderingState.mTextures[1] != nodeB) {
        printf("[texture-cache-freed-node] FAIL: delete leg: deleting A also dropped B or cleared B's slot\n");
        failures++;
    }
    return failures;
}

int TcfnEvictionLeg(Fast::Interpreter& gfx, const std::vector<uint8_t>& bytes) {
    int failures = 0;
    const uint8_t* a = &bytes[0];

    // TextureCacheClear is what Init and every invalidate-all leave behind: the state the cache fills from.
    gfx.TextureCacheClear();
    gfx.TextureCacheLookup(0, TcfnKey(a));
    if (!TcfnSlotOn(gfx, 0, a)) {
        printf("[texture-cache-freed-node] FAIL: eviction leg setup: slot 0 does not name A after its lookup\n");
        return 1;
    }

    const size_t bucketsAtStart = gfx.mTextureCache.map.bucket_count();
    size_t rehashedAtSize = 0;
    size_t capacity = 0;
    size_t lookups = 0;
    const uint8_t* last = nullptr;
    for (size_t k = 1; k < bytes.size(); k++) {
        last = &bytes[k];
        gfx.TextureCacheLookup(1, TcfnKey(last));
        lookups++;
        if (rehashedAtSize == 0 && gfx.mTextureCache.map.bucket_count() != bucketsAtStart) {
            rehashedAtSize = gfx.mTextureCache.map.size();
        }
        if (!TcfnCached(gfx, a)) {
            capacity = gfx.mTextureCache.map.size();
            break;
        }
    }
    if (capacity == 0) {
        printf("[texture-cache-freed-node] FAIL: eviction leg: %zu misses never evicted the least recently used "
               "node, so the leg locks nothing\n",
               lookups);
        return 1;
    }
    printf("[texture-cache-freed-node] eviction leg: A evicted by miss %zu at cache size %zu; buckets %zu at start, "
           "%zu at the end\n",
           lookups, capacity, bucketsAtStart, gfx.mTextureCache.map.bucket_count());

    if (gfx.mRenderingState.mTextures[0] != nullptr) {
        printf("[texture-cache-freed-node] FAIL: eviction leg: slot 0 still points at A's node after the LRU "
               "evicted it; GfxSpTri1 would write sampler state through it\n");
        failures++;
    }
    if (!TcfnSlotOn(gfx, 1, last)) {
        printf("[texture-cache-freed-node] FAIL: eviction leg: slot 1 does not name the texture that caused the "
               "eviction\n");
        failures++;
    }
    if (rehashedAtSize != 0) {
        printf("[texture-cache-freed-node] FAIL: eviction leg: the map rehashed at size %zu (buckets %zu -> %zu) "
               "while filling to its cap of %zu; a rehash invalidates the iterators the LRU list holds\n",
               rehashedAtSize, bucketsAtStart, gfx.mTextureCache.map.bucket_count(), capacity);
        failures++;
    }
    return failures;
}

int TcfnClearLeg(Fast::Interpreter& gfx, const std::vector<uint8_t>& bytes) {
    int failures = 0;
    const int lastSlot = SHADER_MAX_TEXTURES - 1;
    const uint8_t* a = &bytes[0];
    const uint8_t* b = &bytes[1];

    gfx.TextureCacheClear();
    gfx.TextureCacheLookup(0, TcfnKey(a));
    gfx.TextureCacheLookup(1, TcfnKey(b));
    gfx.TextureCacheLookup(lastSlot, TcfnKey(b));
    if (!TcfnSlotOn(gfx, 0, a) || !TcfnSlotOn(gfx, 1, b) || !TcfnSlotOn(gfx, lastSlot, b)) {
        printf("[texture-cache-freed-node] FAIL: clear leg setup: slots 0/1/%d do not name A/B/B after the lookups\n",
               lastSlot);
        return 1;
    }

    gfx.TextureCacheClear();
    if (!gfx.mTextureCache.map.empty()) {
        printf("[texture-cache-freed-node] FAIL: clear leg: TextureCacheClear left %zu node(s), so the leg locks "
               "nothing\n",
               gfx.mTextureCache.map.size());
        return 1;
    }
    for (int slot = 0; slot < SHADER_MAX_TEXTURES; slot++) {
        if (gfx.mRenderingState.mTextures[slot] != nullptr) {
            printf("[texture-cache-freed-node] FAIL: clear leg: slot %d still points into the cleared map\n", slot);
            failures++;
        }
    }
    return failures;
}

} // namespace

extern "C" int TextureCacheFreedNode_RunHeadless(void) {
    // The stub outlives the interpreter that points at it.
    TcfnStubRapi rapi;
    auto gfx = std::make_unique<Fast::Interpreter>();
    gfx->mRapi = &rapi;
    const std::vector<uint8_t> bytes(kTcfnKeyCount, 0);

    int failures = 0;
    failures += TcfnDeleteLeg(*gfx, bytes);
    failures += TcfnEvictionLeg(*gfx, bytes);
    failures += TcfnClearLeg(*gfx, bytes);

    // Leave nothing pointing at the stub or at `bytes` once they go out of scope.
    gfx->TextureCacheClear();
    gfx->mRapi = nullptr;

    if (rapi.mNewTextureCalls == 0) {
        printf("[texture-cache-freed-node] FAIL: no lookup ever missed, so no leg reached the cache's insert path\n");
        failures++;
    }
    if (failures != 0) {
        printf("[texture-cache-freed-node] %d assertion(s) failed\n", failures);
        fflush(stdout);
        return 1;
    }
    printf("[texture-cache-freed-node] PASS: delete, LRU eviction and clear leave no slot on a freed node, and the "
           "map never rehashed while it filled\n");
    fflush(stdout);
    return 0;
}
