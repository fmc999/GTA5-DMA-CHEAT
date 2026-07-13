#include "pch.h"
#include "ModeSelector.h"
#include <iostream>
#include <string>
#include <thread>
#include <conio.h>

#include "MyImGui.h"
#include "MyMenu.h"
#include "DMA.h"
#include "AppRuntime.h"


ModeSelector::Mode ModeSelector::SelectMode() {
    std::cout << "选择模式:" << std::endl;
    std::cout << "\t[1] 普通模式" << std::endl;
    std::cout << "\t[2] 融合器模式" << std::endl;
    std::cout << "\t[3] 全屏模式" << std::endl;
    std::cout << "请输入选择 (1-3): ";

    std::string choiceStr;
    std::getline(std::cin, choiceStr);

    if (choiceStr == "1") return ModeSelector::NORMAL;
    if (choiceStr == "2") return ModeSelector::FUSION;
    if (choiceStr == "3") return ModeSelector::FULLSCREEN;

    std::cout << "无效选择，使用默认普通模式..." << std::endl;
    return ModeSelector::NORMAL;
}



void ModeSelector::RunMode(Mode mode) {
    switch (mode) {
    case NORMAL:
        std::cout << "启动普通模式..." << std::endl;
        MyMenu::bFusionMode = false;
        MyMenu::bFullscreenMode = false;
        break;
    case FUSION:
        std::cout << "启动融合器模式..." << std::endl;
        MyMenu::bFusionMode = true;
        MyMenu::bFullscreenMode = false;
        break;
    case FULLSCREEN:
        std::cout << "启动全屏模式..." << std::endl;
        MyMenu::bFusionMode = false;
        MyMenu::bFullscreenMode = true;
        break;
    }

    // 初始化ImGui
    if (!MyImGui::Initialize()) {
        std::cout << "ImGui初始化失败!" << std::endl;
        return;
    }

    AppRuntime::Reset();
    std::thread dmaThread([]() {
        if (!DMA::Initialize()) {
            std::cout << "DMA初始化失败!" << std::endl;
			return;
        }
        DMA::DMAThreadEntry();
    });

    // 主循环
    while (AppRuntime::IsRunning()) {
        // 监听多个退出按键
        if (GetAsyncKeyState(VK_END) & 1 || GetAsyncKeyState(VK_ESCAPE) & 1) {
            AppRuntime::RequestStop();
            std::cout << "检测到退出按键，正在关闭..." << std::endl;
        }

        MyImGui::OnFrame();
    }

    if (dmaThread.joinable()) {
        dmaThread.join();
    }

    MyImGui::Close();
    std::cout << "程序已退出。" << std::endl;
}