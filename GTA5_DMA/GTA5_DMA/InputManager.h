#pragma once

#include <Windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

class c_keys
{
private:
    uint64_t gafAsyncKeyStateExport = 0;
    DWORD keyboard_pid = 0;
    std::array<uint8_t, 64> state_bitmap{};
    std::array<uint8_t, 64> previous_key_state_bitmap{};
    std::array<uint8_t, 256 / 8> pressed_bitmap{};
    std::chrono::time_point<std::chrono::steady_clock> last_update = std::chrono::steady_clock::now();
    std::atomic_bool initialized{ false };

    bool InitWindows10();
    bool InitWindows11();
    uint64_t FindSignature(DWORD pid, uint64_t start_address, uint64_t module_size, const std::string& signature);
    bool ReadTarget(DWORD pid, uint64_t address, void* buffer, DWORD size) const;

public:
    c_keys() = default;
    ~c_keys() = default;

    bool InitKeyboard();
    bool UpdateKeys();
    bool IsKeyDown(uint32_t virtual_key_code);
    bool IsKeyPressed(uint32_t virtual_key_code);
    bool IsReady() const noexcept { return initialized.load(std::memory_order_acquire); }
};

extern c_keys g_inputManager;
