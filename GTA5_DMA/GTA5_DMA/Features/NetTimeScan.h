#pragma once

// NetTimeScan —— 扫出游戏的「网络时间基准」脚本全局槽（每秒 +1 的那个）。
// 用途：雷达隐身（OffTheRadar）要写「隐身到 <网络时间>」字段。

class NetTimeScan
{
public:
    // 返回 0 = 找到候选；1 = 没找到
    static int Scan();
};
