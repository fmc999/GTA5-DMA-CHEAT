#pragma once

#include <cmath>

class ArmorManager {
public:
    // 静态成员变量
    static bool bLockArmor;
    static inline bool bAutoRefreshArmor = false;
    static inline float ArmorRefreshThreshold = 70.0f;
    static inline float ArmorRefreshValue = 200.0f;
    static float currentArmor;
    
    // 主要功能函数
    static bool UpdateArmor();
    static bool SetArmor(float value);
    static bool OnDMAFrame();
    static bool ShouldAutoRefresh(float armor) noexcept
    {
        return std::isfinite(armor) && armor < ArmorRefreshThreshold;
    }
    
private:
    // 内存读取相关
    static bool ReadArmorFromMemory(float& value);
    static bool WriteArmorToMemory(float value);
};
