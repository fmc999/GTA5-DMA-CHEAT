#pragma once

// ============================================================================
//  Diagnostics —— 把运行期诊断写成文件，供外部（或更新后对账）阅读
//
//  存在理由：FPGA 设备是**独占**的。工具常驻运行时，外部再来一个进程（探针/第二个 exe）
//  无法初始化 vmm（表现为 VMMDLL_Initialize 失败或枚举不到进程）。所以诊断不能靠外部探测，
//  只能由**运行中的实例自己**落盘 —— 加了这个模块之后：
//    · 每次启动自动写一份 <exe 同目录>\GTA5_DMA_diag.txt（覆盖）
//    · 无界面模式 `--diag [路径]` 主动写一份带时间戳的
//    · UI 里也可以点「导出诊断」再写一份
//
//  报告内容：构建标记 / 进程与基址 / 已解析偏移 / tunable 全部条目（索引-elem-实读值-定位方式）/
//  脚本全局动作格 / 脚本线程清单 / 经济功能状态。游戏更新后，看这份报告就知道哪些条目"未定位"。
// ============================================================================

class Diagnostics
{
public:
	static bool WriteReport(const char* path = nullptr);   // nullptr → exe 同目录 GTA5_DMA_diag.txt
	static const char* GetDefaultPath();
	static const char* GetLastPath();
	static bool HasWritten();
};
