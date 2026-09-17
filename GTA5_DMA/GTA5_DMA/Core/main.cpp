#include "pch.h"
#include <print>
#include <string>
#include <thread>

#include "MyImGui.h"
#include "AppRuntime.h"
#include "InputManager.h"
#include "DMA.h"
#include "Tunables.h"
#include "ScriptThreads.h"
#include "EconomyFeatures.h"
#include "Diagnostics.h"


int main(int argc, char** argv)
{
	// 无界面自检模式：用本体代码路径做 tunable 写入实证（--tunable-selftest）
	// 存在理由：python + ctypes 探针在设备被上一次强杀占用时会在 vmm 初始化里直接崩，
	//           而本体走的是同一条 DMA 写入路径，且能受控还原。
	if (argc > 1 && std::string(argv[1]) == "--tunable-selftest")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[selftest] DMA 初始化失败（设备被占用或未连接）" << std::endl;
			return 1;
		}
		const int rc = Tunables::SelfTest();
		DMA::Close();
		return rc;
	}

	// 经济动作格写入自检（--economy-selftest）：同一套 ScriptGlobals 写入路径
	if (argc > 1 && std::string(argv[1]) == "--economy-selftest")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[economy-selftest] DMA 初始化失败（设备被占用或未连接）" << std::endl;
			return 1;
		}
		ScriptThreads::LogRunningScripts(8);
		const int rc = EconomyFeatures::SelfTest();
		DMA::Close();
		return rc;
	}

	// 诊断导出模式（--diag [路径]）：把本次解析结果落盘，供外部阅读（设备独占，外部进程拿不到）
	if (argc > 1 && std::string(argv[1]) == "--diag")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[diag] DMA 初始化失败（设备被占用或未连接）" << std::endl;
			return 1;
		}
		const char* path = (argc > 2) ? argv[2] : nullptr;
		const bool ok = Diagnostics::WriteReport(path);
		DMA::Close();
		return ok ? 0 : 1;
	}

	AppRuntime::Reset();
	// Initialize ImGui
	if (!MyImGui::Initialize())
	{
		std::cerr << "Failed to initialize ImGui" << std::endl;
		return -1;
	}

	// Initialize DMA in a separate thread
	std::thread DMAThread([]() {
		if (!DMA::Initialize())
		{
			std::cerr << "Failed to initialize DMA" << std::endl;
			return;
		}

		if (!g_inputManager.InitKeyboard())
		{
			std::cerr << "Failed to initialize target keyboard input; local hotkeys remain available." << std::endl;
		}

		DMA::DMAThreadEntry();
	});

	// Main loop
	while (AppRuntime::IsRunning())
	{
		// Handle exit keys
		if ((GetAsyncKeyState(VK_END) & 1) || g_inputManager.IsKeyPressed(VK_END))
			AppRuntime::RequestStop();

		// Render ImGui frame
		MyImGui::OnFrame();
	}

	// Clean up
	if (DMAThread.joinable())
		DMAThread.join();

	MyImGui::Close();

	return 0;
}
