#pragma once
#include <atomic>

class VehicleEditor {
public:
    // 载具检查器功能
    static inline std::atomic<bool> bVehicleInspector = false;
    

    
    // 载具编辑器功能
    static inline std::atomic<bool> bVehicleEditor = false;
    static inline std::atomic<float> vehicleAcceleration = 1.0f;
    static inline std::atomic<float> vehicleMass = 1.0f;
    
    // 新载具功能
    static inline std::atomic<bool> bVehicleGodMode = false;
    static inline std::atomic<float> vehicleHealth = 1000.0f;
    static inline std::atomic<float> engineHealthValue = 1000.0f;
    static inline std::atomic<float> vehicleBoost = 0.0f;
    static inline std::atomic<bool> bBoostFrozen = false;
    static inline std::atomic<int> vehicleMods = 0;
    static inline std::atomic<bool> vehicleParachute = false;
    static inline std::atomic<bool> bApplyVehicleMods = false; // Checkbox for direct application
    static inline std::atomic<bool> bAutoApplyVehicleMods = false; // Checkbox for auto application
    
    // 界面方法
    static bool Render();
    static bool RenderContent();
    static bool OnDMAFrame();
    
    // 载具编辑器函数
    static bool ApplyVehicleMods();
    
    // 载具检查器函数
    static bool UpdateVehicleInfo();
    
    // 界面变量
    static inline bool bEnable = true;
    
private:
    static DWORD BytesRead;
    
    // 私有函数
    static void UpdateVehicleFeatures();
    
    // 载具信息的私有变量
    static float currentAcceleration;
    static float currentMass;
    static float currentHealth;
    static float maxHealth;
    static float currentEngineHealth;
    static float currentBoost;
    static int currentMods;
    static bool currentParachute;
    static uint32_t currentVehicleModelHash;
    static uint32_t currentVehicleEntityModelHash;
    static uint8_t currentVehicleState;
    static uint8_t currentVehicleFreezeFlag;
    static uint8_t currentVehicleWeaponAmmo;
    static uintptr_t trackedVehicleAddress;
    static bool bFreezeVehicle;
    static int desiredVehicleWeaponAmmo;
    static float desiredAcceleration;
    static float desiredMass;
    static bool bNeedsOverwrite;
    static bool bRequestedCopyToDesired;
    static bool bAutoApplyRequested;
    
    // 载具操控数据的私有变量
    static float currentDragCoefficient;
    static float currentBuoyancy;
    static float currentDriveInertia;
    static float currentInitialDriveForce;
    static float currentBrakeForce;
    static float currentHandbrakeForce;
    static float currentTractionCurveMax;
    static float currentTractionCurveMin;
    static float currentCollisionMultiplier;
    static float currentWeaponMultiplier;
    static float currentDeformationMultiplier;
    static float currentEngine;
    static float currentThrust;
    static float desiredDragCoefficient;
    static float desiredBuoyancy;
    static float desiredDriveInertia;
    static float desiredInitialDriveForce;
    static float desiredBrakeForce;
    static float desiredHandbrakeForce;
    static float desiredTractionCurveMax;
    static float desiredTractionCurveMin;
    static float desiredCollisionMultiplier;
    static float desiredWeaponMultiplier;
    static float desiredDeformationMultiplier;
    static float desiredEngine;
    static float desiredThrust;
    
    // 自动应用标志
    static bool bAutoApplyVehicleHealth;
    static bool bAutoApplyEngineHealth;
    
    // 从参考代码添加的变量
    static float currentVehicleHealthAlt;  // 车辆健康 (新)
    static float currentBodyHealth;        // 车身健康 (新)
    static float currentTankHealth;        // 车辆油箱健康 (新)
    static uint8_t currentParachuteStatus;  // 降落伞状态
    
    // 新增变量
    static float currentJetCharge;         // 喷气充能进度
    static float currentVehicleHealth;      // 载具血量
    static float currentVehicleMaxHealth;  // 载具最大血量
    static bool bInfiniteJet;              // 无限喷气开关
    static int vehicleAddonMode;            // 载具附加功能模式
    static bool bVehicleAddonApply;         // 载具附加功能自动应用
    static bool bParachuteEnabled;         // 降落伞开关
    static bool bAutoRepair;                // 自动修复开关
    static bool bRepairTriggered;           // 修复触发标志
    static bool bVehicleRepair18;           // 18修复载具开关
    
    // 导弹相关功能
    static float currentLockOnRange;       // 当前导弹锁定范围
    static float currentWeaponRange;       // 当前导弹有效距离
    static float desiredLockOnRange;       // 目标导弹锁定范围
    static float desiredWeaponRange;       // 目标导弹有效距离
    static bool bEnableMissileMods;        // 启用导弹修改
    static bool bAutoApplyMissileMods;     // 自动应用导弹修改
    
    // 跳跃恢复速度相关功能
    static float currentJumpRecoverySpeed; // 当前跳跃恢复速度
    static bool bLockJumpRecoverySpeed;    // 锁定跳跃恢复速度开关
    
    // 喷气恢复速度相关功能
    static float currentJetRecoverySpeed; // 当前喷气恢复速度
    static bool bLockJetRecoverySpeed;    // 锁定喷气恢复速度开关
    
    // 安全带相关功能
    static bool bSeatBeltEnabled;         // 安全带开关
    
    // 函数声明
    static bool UpdateJetChargeValue();
    static bool SetJetChargeValue(float value);
    static bool UpdateJetRecoverySpeed();
    static bool SetJetRecoverySpeedValue(float value);
    static bool UpdateVehicleAddon();
    static bool SetVehicleAddon(int mode);
    static bool SetParachute(bool enabled);
    static bool UpdateVehicleHealth();
    static bool UpdateEngineHealth();
    static bool UpdateVehicleMaxHealth();
    static bool UpdateParachuteStatus();
    static bool UpdateVehicleHealthAlt();
    static bool UpdateBodyHealth();
    static bool UpdateTankHealth();
    static bool RepairVehicle();
    static bool RepairEngine();
    static bool RepairBody();
    static bool RepairTank();
    static bool RepairVehicleHealthAlt();
    static bool RepairVehiclePart18();
    static bool RepairAll();
    
    // 导弹相关函数
    static bool UpdateLockOnRange();
    static bool SetLockOnRange(float value);
    static bool UpdateWeaponRange();
    static bool SetWeaponRange(float value);
    
    // 跳跃恢复速度相关函数
    static bool UpdateJumpRecoverySpeed();
    static bool SetJumpRecoverySpeed(float value);
    
    // 安全带相关函数
    static bool UpdateSeatBeltStatus();
    static bool SetSeatBelt(bool enabled);
    static bool SetVehicleFrozen(bool enabled);
    static bool SetVehicleWeaponAmmo(uint8_t value);
};
