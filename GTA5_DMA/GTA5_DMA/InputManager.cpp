#include "pch.h"
#include "InputManager.h"
#include "DMA.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

namespace
{
constexpr uint64_t kMinimumKernelAddress = 0x7FFFFFFFFFFFULL;
constexpr DWORD kKernelMemoryFlag = VMMDLL_PID_PROCESS_WITH_KERNELMEMORY;

bool IsKernelAddress(uint64_t address)
{
    return address > kMinimumKernelAddress;
}
}

bool c_keys::ReadTarget(DWORD pid, uint64_t address, void* buffer, DWORD size) const
{
    if (!DMA::vmh || !pid || !IsKernelAddress(address) || !buffer || !size)
        return false;

    DWORD bytes_read = 0;
    return VMMDLL_MemReadEx(
        DMA::vmh,
        pid,
        address,
        static_cast<PBYTE>(buffer),
        size,
        &bytes_read,
        VMMDLL_FLAG_NOCACHE) && bytes_read == size;
}

bool c_keys::InitKeyboard()
{
    initialized.store(false, std::memory_order_release);
    gafAsyncKeyStateExport = 0;
    keyboard_pid = 0;
    state_bitmap.fill(0);
    previous_key_state_bitmap.fill(0);
    pressed_bitmap.fill(0);

    if (!DMA::vmh)
        return false;

    ULONG64 windows_build = 0;
    if (!VMMDLL_ConfigGet(DMA::vmh, VMMDLL_OPT_WIN_VERSION_BUILD, &windows_build)) {
        std::cerr << "Unable to read target Windows build." << std::endl;
        return false;
    }

    const bool ready = windows_build > 22000 ? InitWindows11() : InitWindows10();
    initialized.store(ready, std::memory_order_release);

    if (ready) {
        last_update = std::chrono::steady_clock::now() - std::chrono::milliseconds(20);
        std::cout << "Target keyboard initialized for Windows build " << windows_build << std::endl;
    } else {
        std::cerr << "Target keyboard initialization failed for Windows build " << windows_build << std::endl;
    }

    return ready;
}

bool c_keys::InitWindows11()
{
    PVMMDLL_PROCESS_INFORMATION processes = nullptr;
    DWORD process_count = 0;
    if (!VMMDLL_ProcessGetInformationAll(DMA::vmh, &processes, &process_count) || !processes)
        return false;

    bool found = false;

    for (DWORD index = 0; index < process_count && !found; ++index) {
        const auto& process = processes[index];
        if (_stricmp(process.szName, "csrss.exe") != 0)
            continue;

        const DWORD pid = process.dwPID | kKernelMemoryFlag;
        PVMMDLL_MAP_MODULEENTRY win32k_base = nullptr;
        PVMMDLL_MAP_MODULEENTRY win32k = nullptr;

        if (!VMMDLL_Map_GetModuleFromNameW(DMA::vmh, pid, L"win32kbase.sys", &win32k_base, VMMDLL_MODULE_FLAG_NORMAL) || !win32k_base)
            continue;

        if (!VMMDLL_Map_GetModuleFromNameW(DMA::vmh, pid, L"win32ksgd.sys", &win32k, VMMDLL_MODULE_FLAG_NORMAL) || !win32k) {
            if (!VMMDLL_Map_GetModuleFromNameW(DMA::vmh, pid, L"win32k.sys", &win32k, VMMDLL_MODULE_FLAG_NORMAL) || !win32k) {
                VMMDLL_MemFree(win32k_base);
                continue;
            }
        }

        const uint64_t session_signature = FindSignature(pid, win32k->vaBase, win32k->cbImageSize, "48 8B 05 ? ? ? ? 48 8B 04 C8");
        const uint64_t fallback_signature = session_signature ? session_signature : FindSignature(pid, win32k->vaBase, win32k->cbImageSize, "48 8B 05 ? ? ? ? FF C9");

        int32_t session_relative = 0;
        uint64_t session_slots = 0;
        if (fallback_signature && ReadTarget(pid, fallback_signature + 3, &session_relative, sizeof(session_relative)))
            session_slots = fallback_signature + 7 + session_relative;

        uint64_t user_session_state = 0;
        if (IsKernelAddress(session_slots)) {
            for (int slot_index = 0; slot_index < 4 && !user_session_state; ++slot_index) {
                uint64_t slot = 0;
                uint64_t session = 0;
                uint64_t candidate = 0;
                if (!ReadTarget(pid, session_slots + sizeof(uint64_t) * slot_index, &slot, sizeof(slot)) || !IsKernelAddress(slot))
                    continue;
                if (!ReadTarget(pid, slot, &session, sizeof(session)) || !IsKernelAddress(session))
                    continue;
                if (ReadTarget(pid, session, &candidate, sizeof(candidate)) && IsKernelAddress(candidate))
                    user_session_state = candidate;
            }
        }

        const uint64_t key_signature = FindSignature(pid, win32k_base->vaBase, win32k_base->cbImageSize, "48 8D 90 ? ? ? ? E8 ? ? ? ? 0F 57 C0");
        uint32_t key_offset = 0;
        if (user_session_state && key_signature && ReadTarget(pid, key_signature + 3, &key_offset, sizeof(key_offset))) {
            const uint64_t candidate = user_session_state + key_offset;
            if (IsKernelAddress(candidate)) {
                std::array<uint8_t, 64> verification{};
                if (ReadTarget(pid, candidate, verification.data(), static_cast<DWORD>(verification.size()))) {
                    keyboard_pid = pid;
                    gafAsyncKeyStateExport = candidate;
                    found = true;
                }
            }
        }

        VMMDLL_MemFree(win32k);
        VMMDLL_MemFree(win32k_base);
    }

    VMMDLL_MemFree(processes);
    return found;
}

bool c_keys::InitWindows10()
{
    DWORD winlogon_pid = 0;
    if (!VMMDLL_PidGetFromName(DMA::vmh, "winlogon.exe", &winlogon_pid) || !winlogon_pid)
        return false;

    keyboard_pid = winlogon_pid | kKernelMemoryFlag;

    PVMMDLL_MAP_EAT eat = nullptr;
    if (VMMDLL_Map_GetEATU(DMA::vmh, keyboard_pid, "win32kbase.sys", &eat) && eat) {
        for (DWORD index = 0; index < eat->cMap; ++index) {
            const auto& entry = eat->pMap[index];
            if (entry.uszFunction && std::strcmp(entry.uszFunction, "gafAsyncKeyState") == 0 && IsKernelAddress(entry.vaFunction)) {
                gafAsyncKeyStateExport = entry.vaFunction;
                break;
            }
        }
        VMMDLL_MemFree(eat);
    }

    if (!gafAsyncKeyStateExport) {
        PVMMDLL_MAP_MODULEENTRY module = nullptr;
        if (!VMMDLL_Map_GetModuleFromNameW(DMA::vmh, keyboard_pid, L"win32kbase.sys", &module, VMMDLL_MODULE_FLAG_NORMAL) || !module)
            return false;

        CHAR module_name[MAX_PATH]{};
        ULONG64 symbol_address = 0;
        if (VMMDLL_PdbLoad(DMA::vmh, keyboard_pid, module->vaBase, module_name) &&
            VMMDLL_PdbSymbolAddress(DMA::vmh, module_name, "gafAsyncKeyState", &symbol_address) &&
            IsKernelAddress(symbol_address)) {
            gafAsyncKeyStateExport = symbol_address;
        }
        VMMDLL_MemFree(module);
    }

    if (!gafAsyncKeyStateExport)
        return false;

    std::array<uint8_t, 64> verification{};
    return ReadTarget(keyboard_pid, gafAsyncKeyStateExport, verification.data(), static_cast<DWORD>(verification.size()));
}

uint64_t c_keys::FindSignature(DWORD pid, uint64_t start_address, uint64_t module_size, const std::string& signature)
{
    if (!pid || !IsKernelAddress(start_address) || module_size == 0 || module_size > 0x10000000)
        return 0;

    std::vector<int> pattern;
    std::istringstream stream(signature);
    std::string token;
    while (stream >> token) {
        if (token == "?" || token == "??")
            pattern.push_back(-1);
        else
            pattern.push_back(static_cast<int>(std::stoul(token, nullptr, 16)));
    }

    if (pattern.empty())
        return 0;

    constexpr size_t chunk_size = 0x10000;
    const size_t overlap = pattern.size() - 1;
    std::vector<uint8_t> buffer(chunk_size + overlap);

    for (uint64_t offset = 0; offset < module_size; offset += chunk_size) {
        const size_t remaining = static_cast<size_t>(module_size - offset);
        const size_t read_size = std::min(buffer.size(), remaining);
        if (read_size < pattern.size())
            break;

        if (!ReadTarget(pid, start_address + offset, buffer.data(), static_cast<DWORD>(read_size)))
            continue;

        for (size_t index = 0; index + pattern.size() <= read_size; ++index) {
            bool matches = true;
            for (size_t pattern_index = 0; pattern_index < pattern.size(); ++pattern_index) {
                if (pattern[pattern_index] >= 0 && buffer[index + pattern_index] != static_cast<uint8_t>(pattern[pattern_index])) {
                    matches = false;
                    break;
                }
            }
            if (matches)
                return start_address + offset + index;
        }
    }

    return 0;
}

bool c_keys::UpdateKeys()
{
    if (!IsReady())
        return false;

    previous_key_state_bitmap = state_bitmap;
    pressed_bitmap.fill(0);

    std::array<uint8_t, 64> updated{};
    if (!ReadTarget(keyboard_pid, gafAsyncKeyStateExport, updated.data(), static_cast<DWORD>(updated.size()))) {
        initialized.store(false, std::memory_order_release);
        return false;
    }

    state_bitmap = updated;
    for (uint32_t vk = 0; vk < 256; ++vk) {
        const uint8_t current_mask = static_cast<uint8_t>(1u << ((vk % 4) * 2));
        const size_t current_index = vk * 2 / 8;
        const bool is_down = (state_bitmap[current_index] & current_mask) != 0;
        const bool was_down = (previous_key_state_bitmap[current_index] & current_mask) != 0;
        if (is_down && !was_down)
            pressed_bitmap[vk / 8] |= static_cast<uint8_t>(1u << (vk % 8));
    }

    return true;
}

bool c_keys::IsKeyDown(uint32_t virtual_key_code)
{
    if (!IsReady() || virtual_key_code >= 256)
        return false;

    if (std::chrono::steady_clock::now() - last_update > std::chrono::milliseconds(16)) {
        UpdateKeys();
        last_update = std::chrono::steady_clock::now();
    }

    return (state_bitmap[virtual_key_code * 2 / 8] & (1u << ((virtual_key_code % 4) * 2))) != 0;
}

bool c_keys::IsKeyPressed(uint32_t virtual_key_code)
{
    if (!IsReady() || virtual_key_code >= 256)
        return false;

    if (std::chrono::steady_clock::now() - last_update > std::chrono::milliseconds(16)) {
        UpdateKeys();
        last_update = std::chrono::steady_clock::now();
    }

    const uint8_t mask = static_cast<uint8_t>(1u << (virtual_key_code % 8));
    uint8_t& state = pressed_bitmap[virtual_key_code / 8];
    const bool pressed = (state & mask) != 0;
    state &= static_cast<uint8_t>(~mask);
    return pressed;
}

c_keys g_inputManager;
