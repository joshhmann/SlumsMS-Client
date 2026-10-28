// ============================================================
// rmlui_renderer_dx9.cpp: RmlUi on the game's Direct3D 9 device (see rmlui_renderer_dx9.h).
// ============================================================

#include "pch.h"
#include "debug.h"
#include "rmlui_renderer_dx9.h"

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>

#include <wincodec.h>

#include <cstring>
#include <type_traits>

#pragma comment(lib, "windowscodecs.lib")

namespace {

struct Vtx {
    float x, y, z;
    D3DCOLOR colour;
    float u, v;
};
constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;

struct Geometry {
    std::vector<Vtx> vertices;
    std::vector<std::uint16_t> indices16;   // used when every index fits, the common case
    std::vector<std::uint32_t> indices32;
};

const D3DMATRIX kIdentity = {{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}};

D3DMATRIX Multiply(const D3DMATRIX& a, const D3DMATRIX& b) {
    D3DMATRIX r = {};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[i][k] * b.m[k][j];
            r.m[i][j] = s;
        }
    }
    return r;
}

// Premultiplied RGBA bytes -> a managed A8R8G8B8 texture (BGRA in memory).
IDirect3DTexture9* MakeTexture(IDirect3DDevice9* dev, const Rml::byte* rgba, int w, int h) {
    if (!dev || w <= 0 || h <= 0) return nullptr;
    IDirect3DTexture9* tex = nullptr;
    if (FAILED(dev->CreateTexture(static_cast<UINT>(w), static_cast<UINT>(h), 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
            &tex, nullptr))) {
        return nullptr;
    }
    D3DLOCKED_RECT lr = {};
    if (FAILED(tex->LockRect(0, &lr, nullptr, 0))) {
        tex->Release();
        return nullptr;
    }
    for (int y = 0; y < h; ++y) {
        const Rml::byte* src = rgba + static_cast<std::size_t>(y) * w * 4;
        auto* dst = static_cast<std::uint8_t*>(lr.pBits) + static_cast<std::size_t>(y) * lr.Pitch;
        for (int x = 0; x < w; ++x) {
            dst[x * 4 + 0] = src[x * 4 + 2];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = src[x * 4 + 3];
        }
    }
    tex->UnlockRect(0);
    return tex;
}

// PNG/JPEG/BMP bytes -> premultiplied RGBA through Windows Imaging Component (no image library to ship).
bool DecodeImage(const std::string& data, std::vector<Rml::byte>& rgba, int& w, int& h) {
    static bool s_comReady = false;
    if (!s_comReady) {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        s_comReady = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;   // the game thread may already have COM
    }
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
        return false;
    }
    bool ok = false;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    if (SUCCEEDED(factory->CreateStream(&stream)) &&
        SUCCEEDED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<char*>(data.data())),
            static_cast<DWORD>(data.size()))) &&
        SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(factory->CreateFormatConverter(&conv)) &&
        SUCCEEDED(conv->Initialize(frame, GUID_WICPixelFormat32bppPRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
            WICBitmapPaletteTypeCustom))) {
        UINT uw = 0, uh = 0;
        conv->GetSize(&uw, &uh);
        if (uw && uh && uw <= 8192 && uh <= 8192) {
            rgba.resize(static_cast<std::size_t>(uw) * uh * 4);
            ok = SUCCEEDED(conv->CopyPixels(nullptr, uw * 4, static_cast<UINT>(rgba.size()), rgba.data()));
            w = static_cast<int>(uw);
            h = static_cast<int>(uh);
        }
    }
    if (conv) conv->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    factory->Release();
    return ok;
}

}  // namespace

void RenderInterface_DX9::SetDevice(IDirect3DDevice9* device) { device_ = device; }

bool RenderInterface_DX9::BeginFrame(int width, int height) {
    IDirect3DDevice9* dev = device_;
    if (!dev || width <= 0 || height <= 0) return false;
    if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &saved_))) return false;
    if (FAILED(saved_->Capture())) {   // as ImGui does (its #3857): capture after create
        saved_->Release();
        saved_ = nullptr;
        return false;
    }
    dev->GetTransform(D3DTS_WORLD, &savedWorld_);
    dev->GetTransform(D3DTS_VIEW, &savedView_);
    dev->GetTransform(D3DTS_PROJECTION, &savedProj_);

    D3DVIEWPORT9 vp = {0, 0, static_cast<DWORD>(width), static_cast<DWORD>(height), 0.0f, 1.0f};
    dev->SetViewport(&vp);
    dev->SetPixelShader(nullptr);
    dev->SetVertexShader(nullptr);
    dev->SetFVF(kFvf);
    dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    dev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    // RmlUi colours and textures are premultiplied.
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
    dev->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetRenderState(D3DRS_CLIPPING, TRUE);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    // Pixel space, top-left origin, with D3D9's half-pixel offset (as imgui_impl_dx9).
    const float L = 0.5f, R = width + 0.5f, T = 0.5f, B = height + 0.5f;
    const D3DMATRIX proj = {{{2.0f / (R - L), 0, 0, 0, 0, 2.0f / (T - B), 0, 0, 0, 0, 0.5f, 0,
        (L + R) / (L - R), (T + B) / (B - T), 0.5f, 1.0f}}};
    dev->SetTransform(D3DTS_VIEW, &kIdentity);
    dev->SetTransform(D3DTS_PROJECTION, &proj);
    dev->SetTransform(D3DTS_WORLD, &kIdentity);
    hasTransform_ = false;
    return true;
}

void RenderInterface_DX9::EndFrame() {
    if (!saved_) return;
    device_->SetTransform(D3DTS_WORLD, &savedWorld_);
    device_->SetTransform(D3DTS_VIEW, &savedView_);
    device_->SetTransform(D3DTS_PROJECTION, &savedProj_);
    saved_->Apply();
    saved_->Release();
    saved_ = nullptr;
}

Rml::CompiledGeometryHandle RenderInterface_DX9::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
    Rml::Span<const int> indices) {
    auto* g = new Geometry();
    g->vertices.reserve(vertices.size());
    for (const Rml::Vertex& v : vertices) {
        g->vertices.push_back({v.position.x, v.position.y, 0.0f,
            D3DCOLOR_ARGB(v.colour.alpha, v.colour.red, v.colour.green, v.colour.blue), v.tex_coord.x, v.tex_coord.y});
    }
    if (vertices.size() <= 0xFFFF) {
        g->indices16.reserve(indices.size());
        for (int i : indices) g->indices16.push_back(static_cast<std::uint16_t>(i));
    } else {
        g->indices32.assign(indices.begin(), indices.end());
    }
    return reinterpret_cast<Rml::CompiledGeometryHandle>(g);
}

void RenderInterface_DX9::RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation,
    Rml::TextureHandle texture) {
    const auto* g = reinterpret_cast<const Geometry*>(handle);
    if (!g || !device_ || g->vertices.empty()) return;
    const std::size_t count = g->indices16.empty() ? g->indices32.size() : g->indices16.size();
    if (count < 3) return;
    // Row vectors: translate, then the element's transform (RmlUi's column-major data is already transposed for us).
    D3DMATRIX world = kIdentity;
    world._41 = translation.x;
    world._42 = translation.y;
    if (hasTransform_) world = Multiply(world, transform_);
    device_->SetTransform(D3DTS_WORLD, &world);

    auto* tex = reinterpret_cast<IDirect3DTexture9*>(texture);
    device_->SetTexture(0, tex);
    const DWORD op = tex ? D3DTOP_MODULATE : D3DTOP_SELECTARG2;   // no texture: the vertex colour alone
    device_->SetTextureStageState(0, D3DTSS_COLOROP, op);
    device_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device_->SetTextureStageState(0, D3DTSS_ALPHAOP, op);
    device_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    const bool idx16 = !g->indices16.empty();
    device_->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(g->vertices.size()),
        static_cast<UINT>(count / 3), idx16 ? static_cast<const void*>(g->indices16.data()) : g->indices32.data(),
        idx16 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, g->vertices.data(), sizeof(Vtx));
}

void RenderInterface_DX9::ReleaseGeometry(Rml::CompiledGeometryHandle handle) {
    delete reinterpret_cast<Geometry*>(handle);
}

Rml::TextureHandle RenderInterface_DX9::LoadTexture(Rml::Vector2i& dims, const Rml::String& source) {
    // "wz:" sources (stock sprites from the client's resource manager) come with the panels; not handled yet.
    if (source.rfind("wz:", 0) == 0) {
        Rml::Log::Message(Rml::Log::LT_WARNING, "RmlUi DX9: wz: textures are not supported yet (%s)", source.c_str());
        return 0;
    }
    std::string data;
    if (!Rml::GetFileInterface()->LoadFile(source, data) || data.empty()) return 0;
    std::vector<Rml::byte> rgba;
    int w = 0, h = 0;
    if (!DecodeImage(data, rgba, w, h)) {
        Rml::Log::Message(Rml::Log::LT_WARNING, "RmlUi DX9: could not decode %s", source.c_str());
        return 0;
    }
    IDirect3DTexture9* tex = MakeTexture(device_, rgba.data(), w, h);
    if (!tex) return 0;
    ++textures_;
    dims = {w, h};
    return reinterpret_cast<Rml::TextureHandle>(tex);
}

Rml::TextureHandle RenderInterface_DX9::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dims) {
    if (source.size() < static_cast<std::size_t>(dims.x) * dims.y * 4) return 0;
    IDirect3DTexture9* tex = MakeTexture(device_, source.data(), dims.x, dims.y);
    if (!tex) return 0;
    ++textures_;
    return reinterpret_cast<Rml::TextureHandle>(tex);
}

void RenderInterface_DX9::ReleaseTexture(Rml::TextureHandle texture) {
    if (auto* tex = reinterpret_cast<IDirect3DTexture9*>(texture)) {
        tex->Release();
        --textures_;
    }
}

void RenderInterface_DX9::EnableScissorRegion(bool enable) {
    if (device_) device_->SetRenderState(D3DRS_SCISSORTESTENABLE, enable ? TRUE : FALSE);
}

void RenderInterface_DX9::SetScissorRegion(Rml::Rectanglei region) {
    if (!device_) return;
    const RECT r = {region.Left(), region.Top(), region.Right(), region.Bottom()};
    device_->SetScissorRect(&r);
}

void RenderInterface_DX9::SetTransform(const Rml::Matrix4f* transform) {
    hasTransform_ = transform != nullptr;
    if (!transform) return;
    // Column-major storage of M is the row-major storage of M transposed: exactly D3D9's row-vector matrix.
    static_assert(std::is_same_v<Rml::Matrix4f, Rml::ColumnMajorMatrix4f>, "built without RMLUI_MATRIX_ROW_MAJOR");
    std::memcpy(&transform_, transform->data(), sizeof(transform_));
}
