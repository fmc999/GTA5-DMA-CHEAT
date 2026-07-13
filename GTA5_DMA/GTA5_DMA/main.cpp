#include "pch.h"
#include <print>
#include <thread>

#include "MyImGui.h"
#include "AppRuntime.h"
#include "InputManager.h"


int main(int, char**)
{
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
