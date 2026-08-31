#include "pch.h"
#include "HeistDividend.h"
#include "DMA.h"
#include "Offsets.h"
#include <vector>
#include <string>
#include <iostream>
#include <mutex>

// 初始化静态成员
std::vector<DividendData> HeistDividend::dividendDataList;
std::vector<DividendData> HeistDividend::casinoDividends;
std::vector<DividendData> HeistDividend::cayoDividends;

void HeistDividend::InitializeDividendData()
{
    dividendDataList.clear();
    casinoDividends.clear();
    cayoDividends.clear();
    
    // 赌场豪劫分红数据 (基于CT文件)
    // 地址: "GTA5_Enhanced.exe"+03F8E970, 偏移量: 112828/112830/112838/112840
    casinoDividends.push_back({ HeistType::Casino, 1, &casinoDividend1P, &casinoDividend1PUI, { 0x112828 }, "赌场豪劫分红1P" });
    casinoDividends.push_back({ HeistType::Casino, 2, &casinoDividend2P, &casinoDividend2PUI, { 0x112830 }, "赌场豪劫分红2P" });
    casinoDividends.push_back({ HeistType::Casino, 3, &casinoDividend3P, &casinoDividend3PUI, { 0x112838 }, "赌场豪劫分红3P" });
    casinoDividends.push_back({ HeistType::Casino, 4, &casinoDividend4P, &casinoDividend4PUI, { 0x112840 }, "赌场豪劫分红4P" });
    
    // 佩里科岛分红数据 (基于CT文件)
    // 地址: "GTA5_Enhanced.exe"+03F8E970, 偏移量: 11CFD8/11CFE0/11CFE8/11CFF0
    cayoDividends.push_back({ HeistType::Cayo, 1, &cayoDividend1P, &cayoDividend1PUI, { 0x11CFD8 }, "佩里科岛分红1P" });
    cayoDividends.push_back({ HeistType::Cayo, 2, &cayoDividend2P, &cayoDividend2PUI, { 0x11CFE0 }, "佩里科岛分红2P" });
    cayoDividends.push_back({ HeistType::Cayo, 3, &cayoDividend3P, &cayoDividend3PUI, { 0x11CFE8 }, "佩里科岛分红3P" });
    cayoDividends.push_back({ HeistType::Cayo, 4, &cayoDividend4P, &cayoDividend4PUI, { 0x11CFF0 }, "佩里科岛分红4P" });
    
    // 合并所有分红数据
    dividendDataList.insert(dividendDataList.end(), casinoDividends.begin(), casinoDividends.end());
    dividendDataList.insert(dividendDataList.end(), cayoDividends.begin(), cayoDividends.end());
}

void HeistDividend::OnDMAFrame()
{
    static int frameCounter = 0;
    frameCounter++;
    
    // 检查游戏状态
    if (!CheckGameStatus()) {
        return;
    }
    
    // 只在第一次帧或功能启用时初始化数据
    static bool initialized = false;
    if (!initialized) {
        InitializeDividendData();
        initialized = true;
    }
    
    // 如果功能未启用，直接返回
    if (!bEnable.load())
        return;

    // 不再自动更新分红值，而是由用户通过应用按钮手动控制
    // 移除自动读取，避免覆盖用户输入
    
    // 同步原子变量到UI变量，确保UI显示最新值
    SyncUI();
}

// 读取分红值到UI变量
void HeistDividend::ReadCurrentDividends() {
    // 检查游戏状态
    if (!CheckGameStatus()) {
        return;
    }
    
    // 读取赌场豪劫分红
    ReadDividendValue(casinoDividends[0]);
    ReadDividendValue(casinoDividends[1]);
    ReadDividendValue(casinoDividends[2]);
    ReadDividendValue(casinoDividends[3]);
    
    // 读取佩里科岛分红
    ReadDividendValue(cayoDividends[0]);
    ReadDividendValue(cayoDividends[1]);
    ReadDividendValue(cayoDividends[2]);
    ReadDividendValue(cayoDividends[3]);
}

// 读取单个分红值
bool HeistDividend::ReadDividendValue(const DividendData& dividend) {
    // 基础地址: "GTA5_Enhanced.exe"+03F8E970
    uintptr_t baseAddr = DMA::BaseAddress + BASE_ADDRESS_OFFSET;
    
    if (!baseAddr) return false;
    
    DWORD BytesRead = 0;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    if (!VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE) || BytesRead != sizeof(uintptr_t) || !addr1) {
        return false;
    }
    
    // 计算最终地址: 基址 + 偏移量
    uintptr_t finalAddr = addr1 + dividend.offsets[0];
    
    // 读取分红值
    int value = 0;
    if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(int), &BytesRead, VMMDLL_FLAG_NOCACHE) && BytesRead == sizeof(int)) {
        // 更新UI变量
        *dividend.uiValue = value;
        // 同步到原子变量
        dividend.value->store(value);
        return true;
    }
    
    return false;
}

// 更新分红值
void HeistDividend::UpdateDividends() {
    // 检查游戏状态
    if (!CheckGameStatus()) {
        return;
    }
    
    // 写入赌场豪劫分红
    WriteDividendValue(casinoDividends[0]);
    WriteDividendValue(casinoDividends[1]);
    WriteDividendValue(casinoDividends[2]);
    WriteDividendValue(casinoDividends[3]);
    
    // 写入佩里科岛分红
    WriteDividendValue(cayoDividends[0]);
    WriteDividendValue(cayoDividends[1]);
    WriteDividendValue(cayoDividends[2]);
    WriteDividendValue(cayoDividends[3]);
}

// 写入单个分红值
bool HeistDividend::WriteDividendValue(const DividendData& dividend) {
    // 基础地址: "GTA5_Enhanced.exe"+03F8E970
    uintptr_t baseAddr = DMA::BaseAddress + BASE_ADDRESS_OFFSET;
    
    if (!baseAddr) return false;
    
    DWORD BytesRead = 0;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    if (!VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE) || BytesRead != sizeof(uintptr_t) || !addr1) {
        return false;
    }
    
    // 计算最终地址: 基址 + 偏移量
    uintptr_t finalAddr = addr1 + dividend.offsets[0];
    
    // 获取要写入的值
    int value = dividend.value->load();
    
    // 写入分红值
    return VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(int)) == VMMDLL_STATUS_SUCCESS;
}

// 同步UI值
void HeistDividend::SyncUI() {
    // 只同步原子变量到UI变量，避免冗余同步和可能的竞态条件
    bEnableUI = bEnable.load();
    casinoDividend1PUI = casinoDividend1P.load();
    casinoDividend2PUI = casinoDividend2P.load();
    casinoDividend3PUI = casinoDividend3P.load();
    casinoDividend4PUI = casinoDividend4P.load();
    cayoDividend1PUI = cayoDividend1P.load();
    cayoDividend2PUI = cayoDividend2P.load();
    cayoDividend3PUI = cayoDividend3P.load();
    cayoDividend4PUI = cayoDividend4P.load();
}

// 写入赌场分红值
void HeistDividend::WriteCasinoDividend(int playerIndex, int value) {
    // 检查玩家索引是否有效
    if (playerIndex < 1 || playerIndex > 4) {
        puts("[ERROR] Invalid player index for casino dividend");
        return;
    }
    
    // 检查游戏状态
    if (!CheckGameStatus()) {
        return;
    }
    
    // 确保分红数据已初始化
    static bool initialized = false;
    if (!initialized) {
        InitializeDividendData();
        initialized = true;
    }
    
    // 找到对应玩家的分红数据并写入
    for (auto& dividend : casinoDividends) {
        if (dividend.playerIndex == playerIndex) {
            // 更新原子变量
            *dividend.value = value;
            *dividend.uiValue = value;
            
            // 写入游戏内存
            WriteDividendValue(dividend);
            
            printf("[INFO] Written casino dividend for player %d: %d%%\n", playerIndex, value);
            return;
        }
    }
    
    puts("[ERROR] Could not find casino dividend data for player index");
}

// 写入佩里科岛分红值
void HeistDividend::WriteCayoDividend(int playerIndex, int value) {
    // 检查玩家索引是否有效
    if (playerIndex < 1 || playerIndex > 4) {
        puts("[ERROR] Invalid player index for cayo dividend");
        return;
    }
    
    // 检查游戏状态
    if (!CheckGameStatus()) {
        return;
    }
    
    // 确保分红数据已初始化
    static bool initialized = false;
    if (!initialized) {
        InitializeDividendData();
        initialized = true;
    }
    
    // 找到对应玩家的分红数据并写入
    for (auto& dividend : cayoDividends) {
        if (dividend.playerIndex == playerIndex) {
            // 更新原子变量
            *dividend.value = value;
            *dividend.uiValue = value;
            
            // 写入游戏内存
            WriteDividendValue(dividend);
            
            printf("[INFO] Written cayo dividend for player %d: %d%%\n", playerIndex, value);
            return;
        }
    }
    
    puts("[ERROR] Could not find cayo dividend data for player index");
}
    




// 检查游戏状态
bool HeistDividend::CheckGameStatus() {
    // 检查基础地址是否有效
    if (DMA::BaseAddress == 0) {
        return false;
    }
    
    // 检查游戏是否在运行
    if (DMA::PID == 0) {
        return false;
    }
    
    // 这里可以添加更多特定于游戏状态的检查
    // 例如检查玩家是否在抢劫任务中
    
    return true;
}

// 设置所有赌场豪劫分红为85%
void HeistDividend::SetAllTo85() {
    // 设置UI变量
    casinoDividend1PUI = 85;
    casinoDividend2PUI = 85;
    casinoDividend3PUI = 85;
    casinoDividend4PUI = 85;
    
    // 设置存储变量
    casinoDividends[0].value->store(85);
    casinoDividends[1].value->store(85);
    casinoDividends[2].value->store(85);
    casinoDividends[3].value->store(85);
    
    // 调用UpdateDividends写入游戏内存
    UpdateDividends();
}

