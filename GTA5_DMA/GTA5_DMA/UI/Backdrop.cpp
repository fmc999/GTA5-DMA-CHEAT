#include "pch.h"

#include "Backdrop.h"

#include "EmbeddedAssets.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "../external/stb_image.h"

#include <cstdio>

namespace
{
ID3D11ShaderResourceView* g_sharp = nullptr;
ID3D11ShaderResourceView* g_blurred = nullptr;
ImVec2 g_sharpSize = ImVec2(0.0f, 0.0f);
ImVec2 g_blurredSize = ImVec2(0.0f, 0.0f);

// 从内嵌内存解码并上传纹理（不依赖任何外部文件路径）
ID3D11ShaderResourceView* CreateTextureFromMemory(ID3D11Device* device,
                                                  const unsigned char* data, std::size_t size,
                                                  ImVec2& sizeOut)
{
    if (!device || !data || size == 0)
        return nullptr;

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, 4);
    if (!pixels)
        return nullptr;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sub = {};
    sub.pSysMem = pixels;
    sub.SysMemPitch = static_cast<UINT>(width) * 4u;

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, &sub, &texture);
    stbi_image_free(pixels);
    if (FAILED(hr) || !texture)
        return nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    viewDesc.Format = desc.Format;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* view = nullptr;
    hr = device->CreateShaderResourceView(texture, &viewDesc, &view);
    texture->Release();
    if (FAILED(hr) || !view)
        return nullptr;

    sizeOut = ImVec2(static_cast<float>(width), static_cast<float>(height));
    return view;
}
} // namespace

bool Backdrop::Init(ID3D11Device* device)
{
    if (g_sharp && g_blurred)
        return true;

    if (!g_sharp)
        g_sharp = CreateTextureFromMemory(device, embedded_assets::backdrop_jpg,
                                          embedded_assets::backdrop_jpg_size, g_sharpSize);
    if (!g_blurred)
        g_blurred = CreateTextureFromMemory(device, embedded_assets::backdrop_blur_jpg,
                                            embedded_assets::backdrop_blur_jpg_size, g_blurredSize);
    return g_sharp != nullptr || g_blurred != nullptr;
}

void Backdrop::Shutdown()
{
    if (g_sharp) { g_sharp->Release(); g_sharp = nullptr; }
    if (g_blurred) { g_blurred->Release(); g_blurred = nullptr; }
    g_sharpSize = ImVec2(0.0f, 0.0f);
    g_blurredSize = ImVec2(0.0f, 0.0f);
}

bool Backdrop::Ready() { return g_sharp != nullptr; }
ImTextureID Backdrop::Sharp() { return (ImTextureID)(void*)g_sharp; }
ImTextureID Backdrop::Blurred() { return (ImTextureID)(void*)(g_blurred ? g_blurred : g_sharp); }
ImVec2 Backdrop::SharpSize() { return g_sharpSize; }
ImVec2 Backdrop::BlurredSize() { return g_blurred ? g_blurredSize : g_sharpSize; }
