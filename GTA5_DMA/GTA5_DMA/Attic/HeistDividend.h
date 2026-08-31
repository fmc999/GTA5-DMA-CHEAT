#pragma once
#include <atomic>
#include <vector>
#include <mutex>

// 分红类型枚举
enum class HeistType {
    Casino,     // 赌场豪劫
    Cayo        // 佩里科岛
};

// 分红数据结构
struct DividendData {
    HeistType type;
    int playerIndex;    // 玩家索引 (1-4)
    std::atomic<int>* value;     // 原子变量引用
    int* uiValue;       // UI变量引用
    std::vector<size_t> offsets; // 偏移量路径
    std::string name;    // 显示名称
};

class HeistDividend
{
public:
    // 启用状态
    static inline std::atomic<bool> bEnable = false;
    static inline bool bEnableUI = false;
    
    // 赌场豪劫分红 (1P-4P)
    static inline std::atomic<int> casinoDividend1P = 40;
    static inline std::atomic<int> casinoDividend2P = 40;
    static inline std::atomic<int> casinoDividend3P = 40;
    static inline std::atomic<int> casinoDividend4P = 40;
    
    // 佩里科岛分红 (1P-4P)
    static inline std::atomic<int> cayoDividend1P = 40;
    static inline std::atomic<int> cayoDividend2P = 40;
    static inline std::atomic<int> cayoDividend3P = 40;
    static inline std::atomic<int> cayoDividend4P = 40;
    
    // UI版本
    static inline int casinoDividend1PUI = 40;
    static inline int casinoDividend2PUI = 40;
    static inline int casinoDividend3PUI = 40;
    static inline int casinoDividend4PUI = 40;
    static inline int cayoDividend1PUI = 40;
    static inline int cayoDividend2PUI = 40;
    static inline int cayoDividend3PUI = 40;
    static inline int cayoDividend4PUI = 40;

    // 主要功能函数
    static void OnDMAFrame();
    static void UpdateDividends();
    static void ReadCurrentDividends(); // 读取当前分红值
    static void SyncUI();
    static void SetAllTo85(); // 设置所有分红为85%
    
    // 初始化分红数据
    static void InitializeDividendData();
    
    // 写入单个分红值
    static void WriteCasinoDividend(int playerIndex, int value);
    static void WriteCayoDividend(int playerIndex, int value);
    
private:
    // 读取和写入分红值
    static bool ReadDividendValue(const DividendData& dividend);
    static bool WriteDividendValue(const DividendData& dividend);
    
    // 检查游戏状态
    static bool CheckGameStatus();
    
    // 分红数据列表
    static std::vector<DividendData> dividendDataList;
    static std::vector<DividendData> casinoDividends;  // 赌场豪劫分红数据
    static std::vector<DividendData> cayoDividends;     // 佩里科岛分红数据
    
    // 互斥锁
    static inline std::mutex mutex;
    
    // 基础地址
    static const uintptr_t BASE_ADDRESS_OFFSET = 0x03F8E970;
    
    // 调试标志
    static inline bool debugMode = true;
};