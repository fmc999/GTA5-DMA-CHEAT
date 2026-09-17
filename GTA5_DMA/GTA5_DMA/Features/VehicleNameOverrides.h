#pragma once

// ============================================================================
// VehicleNameOverrides —— 运行时「补充载具名」表（免重编译）
//
// 背景：内置表 VehicleNames.h 来自公开数据 dump（921 辆），**比最新 DLC 旧**。
//       新 DLC 车（如「科茨中心抢劫」的 Grotti Veleno GT / velenogt）会显示哈希。
//       本模块让用户/我们在 exe 同目录写一个纯文本文件即可补名，不必重编译：
//
//           vehicle_names_extra.txt
//           # 一行一条：模型名 = 显示名（中文/任意）
//           velenogt = 维利诺 GT
//           somecar  = 某某车（分类）
//
//       模型名会按 joaat 算出哈希（与游戏一致），显示名原样使用。
//       井号开头为注释；空行忽略；重复哈希后写覆盖先写。
// ============================================================================

#include <cstdint>

#include "VehicleNames.h"

// 先查内置表，再查补充表（补充表优先于内置：同哈希时以补充表为准）
const VehicleNameEntry* LookupVehicleNameEx(uint32_t hash);

// 补充表条目数（诊断用）
int GetExtraVehicleNameCount();
