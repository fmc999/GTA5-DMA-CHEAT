#include "pch.h"
#include "HealthManager.h"
#include "DMA.h"
#include "Offsets.h"

// 静态成员变量定义
bool HealthManager::bLockHealth = false;
float HealthManager::currentHealth = 0.0f;

// 更新生命值
bool HealthManager::UpdateHealth()
{
    float value = 0.0f;
    if (ReadHealthFromMemory(value)) {
        currentHealth = value;
        return true;
    }
    return false;
}

// 设置生命值
bool HealthManager::SetHealth(float value)
{
    return WriteHealthToMemory(value);
}

// 每帧处理函数
bool HealthManager::OnDMAFrame()
{
    // 处理生命值锁定逻辑
    if (bLockHealth) {
        SetHealth(1000.0f);  // 设置为1000，实现假无敌
        currentHealth = 1000.0f;  // 同步显示值为锁定值
    } else {
        // 只有在未锁定时才更新生命值
        UpdateHealth();
    }
    
    return true;
}

// 从内存读取生命值
bool HealthManager::ReadHealthFromMemory(float& value)
{
    if (!DMA::vmh || !DMA::PID || !DMA::LocalPlayerAddress)
        return false;

    // 根据Cheat Engine配置读取生命值:
    // WorldPTR -> +8 -> +0x280 (当前生命值)
    uintptr_t healthAddress = DMA::LocalPlayerAddress + offsetof(PED, CurrentHealth);
    
    // 读取生命值
    DWORD bytesRead = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, healthAddress, (BYTE*)&value, sizeof(float), &bytesRead, VMMDLL_FLAG_NOCACHE);
    if (bytesRead != sizeof(float)) return false;
    
    return true;
}

// 向内存写入生命值
bool HealthManager::WriteHealthToMemory(float value)
{
    if (!DMA::vmh || !DMA::PID || !DMA::LocalPlayerAddress)
        return false;

    // 根据Cheat Engine配置设置生命值:
    // WorldPTR -> +8 -> +0x280 (当前生命值)
    uintptr_t healthAddress = DMA::LocalPlayerAddress + offsetof(PED, CurrentHealth);
    
    // 写入生命值
    VMMDLL_MemWrite(DMA::vmh, DMA::PID, healthAddress, (BYTE*)&value, sizeof(float));
    
    return true;
}