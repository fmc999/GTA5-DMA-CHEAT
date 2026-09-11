#pragma once

#include "imgui.h"

struct ID3D11Device;

// Portfolio #8 风格背景：壁纸与毛玻璃副本全部编译进二进制（UI/EmbeddedAssets.h），
// 运行时不读取任何外部文件路径，单 exe 交付即可。
namespace Backdrop
{
    bool Init(ID3D11Device* device);
    void Shutdown();

    bool Ready();
    ImTextureID Sharp();          // 全屏壁纸（清晰）
    ImTextureID Blurred();        // 毛玻璃副本（面板磨砂）
    ImVec2 SharpSize();
    ImVec2 BlurredSize();
}
