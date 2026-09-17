#pragma once

// ============================================================================
//  AimAid — 内存自瞄（辅助瞄准增强）
//
//  参考 YimMenuV2 src/game/features/self/Aimbot.cpp 的四处字节补丁，改用 DMA 写入实现。
//  DMA 写 .text 会绕过页保护（本机实测：48 -> 49 -> 48 往返成功），因此不需要注入 DLL，
//  也不需要远程线程——这也是本项目一贯的「纯 DMA」路线。
//
//  四处补丁（YimMenuV2 src/game/pointers/Pointers.cpp 的特征码 + 位移）：
//
//    A. ShouldNotTargetEntity      sig "F6 80 A9 14 00 00 01"  site = sig-0x53  写 B0 00 C3
//       （mov al,0; ret）让「是否允许锁定该实体」恒为 false —— 即不再排除目标
//    B. GetAssistedAimType         sig "FF E0 48 8D 86"        site = sig-0x15  写 BD 01 00 00 00
//       （mov ebp,1）把辅助瞄准类型锁定为 1
//    C. GetLockOnPos               sig "0F 29 74 24 ? 48 89 D6 48 89 CF 48 8B 05"
//                                                             site = sig+0x22  写 EB
//       （jmp short）跳过原判断 —— 瞄准点直接取头部（爆头）
//    D. ShouldAllowDriverLockOn    sig "75 ? 45 89 C7 49 89 CE" site = sig-0x2C 写 B0 01 C3
//       （mov al,1; ret）允许对载具驾驶员锁定
//
//  实机状态（2026-09-16，GTA5_Enhanced.exe，FPGA/DMA）：
//    四处特征码全部命中，补丁点当前字节均为函数序言/条件跳转，与预期语义一致：
//      A base+0xDA4AE0 : 56 57 55 53 48 81 EC A8   (push rsi; push rdi; push rbp; push rbx; sub rsp,..)
//      B base+0xDA1017 : 83 FD 03 77 1F 89 E8 48   (cmp ebp,3; ja +1F; mov eax,ebp)
//      C base+0x1B781B3: 75 1B 48 8B 83 70 13 00   (jne +1B; mov rax,[rbx+0x1370])
//      D base+0xD9C550 : 41 57 41 56 56 57 55 53   (push r15; push r14; push rsi; push rdi; push rbp; push rbx)
//
//  安全设计：
//    · 解析失败 = 该补丁不可用，绝不在未知地址上盲写。
//    · 每处补丁保存原始字节，关闭开关时原样写回（可逆）。
//    · 应用后立即读回校验，UI 显示实际生效状态。
//    · DMA 断开或程序退出前先写回原值，避免补丁残留在游戏进程。
//    · 同一目标进程重连时保留原值，不会把已应用的补丁误学成原始字节。
// ============================================================================

#include <atomic>
#include <cstdint>

class AimAid
{
public:
	// ---- 开关（UI 线程写，DMA 线程消费）----
	static inline std::atomic<bool> bAssistedAim{ false };    // A + B
	static inline std::atomic<bool> bAimForHead{ false };     // C
	static inline std::atomic<bool> bDriverLockOn{ false };   // D

	// ---- 生命周期 ----
	// 启动时解析四处特征码（OffsetResolver 加载的 .text 段），失败则标记该补丁不可用。
	static bool Resolve();

	// DMA 线程每帧调用：按开关状态应用/还原补丁
	static void OnDMAFrame();

	// DMA 断开前调用：尽力写回原始字节，但保留原值，供同一进程重连复用。
	static bool PrepareForClose();

	// 立即写回所有已记录的原始字节；未解析或没有原值的补丁会跳过。
	static bool RestoreAll();

	// ---- UI 只读状态 ----
	enum class PatchId { ShouldNotTarget = 0, AssistedAimType, LockOnPos, DriverLockOn, Count };

	static bool IsResolved(PatchId id);
	static bool IsApplied(PatchId id);
	static uintptr_t GetSite(PatchId id);
	static const char* GetName(PatchId id);
	static int GetResolvedCount();
	// 写入被第三方工具立即覆盖的次数（>0 表示该补丁点正在被别的工具争用）
	static int GetContentionCount(PatchId id);

	// 完整清空（确认换了目标进程时调用；会同时丢弃原始字节记录）
	static void Reset();
};
