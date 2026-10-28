#pragma once
// ============================================================
// rmlui_renderer_dx9.h -- RmlUi's render interface on the game's Direct3D 9 device (1.7.0, HUD skin step 1).
//
// RmlUi 6.3 ships no DX9 renderer. This one is modelled on RmlUi's fixed-function GL2 backend and on ImGui's
// imgui_impl_dx9.cpp, which already draws on this device: fixed pipeline, premultiplied alpha, scissor, an optional
// transform. Geometry stays in CPU memory and is drawn with DrawIndexedPrimitiveUP, and textures are D3DPOOL_MANAGED,
// so nothing has to be released or rebuilt around IDirect3DDevice9::Reset. Clip masks, layers, filters and shaders
// (render interface 2 extras) are not implemented: RmlUi falls back to drawing without them.
// ============================================================

#include <d3d9.h>
#include <RmlUi/Core/RenderInterface.h>

class RenderInterface_DX9 : public Rml::RenderInterface {
public:
    // The device the next frame draws on. The host calls Rml::ReleaseTextures() first when the device changes.
    void SetDevice(IDirect3DDevice9* device);
    IDirect3DDevice9* Device() const { return device_; }

    // Around Context::Render(): saves the game's device state, sets ours for a width x height target, restores.
    bool BeginFrame(int width, int height);
    void EndFrame();

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    void SetTransform(const Rml::Matrix4f* transform) override;

    int Textures() const { return textures_; }   // live texture count, for the log

private:
    IDirect3DDevice9* device_ = nullptr;
    IDirect3DStateBlock9* saved_ = nullptr;
    D3DMATRIX savedWorld_ = {}, savedView_ = {}, savedProj_ = {};
    D3DMATRIX transform_ = {};
    bool hasTransform_ = false;
    int textures_ = 0;
};
