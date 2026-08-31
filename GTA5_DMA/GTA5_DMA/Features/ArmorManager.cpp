#include "pch.h"
#include "ArmorManager.h"
#include "DMA.h"
#include "Offsets.h"

// 静态成员变量定义
bool ArmorManager::bLockArmor = false;
float ArmorManager::currentArmor = 0.0f;

// 更新防弹衣值
bool ArmorManager::UpdateArmor()
{
    float value = 0.0f;
    if (ReadArmorFromMemory(value)) {
        currentArmor = value;
        return true;
    }
    return false;
}

// 设置防弹衣值
bool ArmorManager::SetArmor(float value)
{
    return WriteArmorToMemory(value);
}

// 每帧处理函数
bool ArmorManager::OnDMAFrame()
{
    if (bLockArmor)
    {
        if (SetArmor(ArmorRefreshValue))
        {
            currentArmor = ArmorRefreshValue;
        }
        return true;
    }

    if (bAutoRefreshArmor)
    {
        if (!UpdateArmor())
        {
            return false;
        }

        if (ShouldAutoRefresh(currentArmor) && SetArmor(ArmorRefreshValue))
        {
            currentArmor = ArmorRefreshValue;
        }
        return true;
    }

    return UpdateArmor();
}

// 从内存读取防弹衣值
bool ArmorManager::ReadArmorFromMemory(float& value)
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置读取防弹衣值:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0x150C
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    if (!DMA::Memory().Read(baseAddr, &addr1, sizeof(addr1)) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    if (!DMA::Memory().Read(addr1 + 0x8, &addr2, sizeof(addr2)) || !addr2) return false;
    
    // 最终地址: +0x150C
    uintptr_t finalAddr = addr2 + 0x150C;
    
    // 读取防弹衣值
    return DMA::Memory().Read(finalAddr, &value, sizeof(value));
}

// 向内存写入防弹衣值
bool ArmorManager::WriteArmorToMemory(float value)
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置设置防弹衣值:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0x150C
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    if (!DMA::Memory().Read(baseAddr, &addr1, sizeof(addr1)) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    if (!DMA::Memory().Read(addr1 + 0x8, &addr2, sizeof(addr2)) || !addr2) return false;
    
    // 最终地址: +0x150C
    uintptr_t finalAddr = addr2 + 0x150C;
    
    // 写入防弹衣值
    return DMA::Memory().Write(finalAddr, &value, sizeof(value));
}
