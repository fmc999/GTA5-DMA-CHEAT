#pragma once

class HealthManager {
public:
    // 静态成员变量
    static bool bLockHealth;
    static float currentHealth;
    
    // 主要功能函数
    static bool UpdateHealth();
    static bool SetHealth(float value);
    static bool OnDMAFrame();
    
private:
    // 内存读取相关
    static bool ReadHealthFromMemory(float& value);
    static bool WriteHealthToMemory(float value);
};