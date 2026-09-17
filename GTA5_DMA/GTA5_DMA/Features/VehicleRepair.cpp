#include "pch.h"

#include "VehicleRepair.h"

#include "DMA.h"
#include "Reclass.h"
#include "VehicleList.h"

#include <cmath>
#include <mutex>
#include <vector>

namespace
{
	// 四段健康值：偏移由 Core/Reclass.h 的 static_assert 钉死。
	struct RepairField
	{
		size_t      offset;
		const char* name;
	};

	constexpr RepairField kFields[] = {
		{ offsetof(CVehicle, Health),           "载具血量" },
		{ offsetof(CVehicle, BodyHealth),       "车身" },
		{ offsetof(CVehicle, PetrolTankHealth), "油箱" },
		{ offsetof(CVehicle, EngineHealth),     "引擎" },
	};
	static_assert(sizeof(kFields) / sizeof(kFields[0]) == VehicleRepair::kFieldCount,
	              "修复字段数必须与 VehicleRepair::kFieldCount 一致");

	constexpr float    kRepairValue          = 1000.0f;  // 与 VehicleEditor 的修复写入值一致
	constexpr float    kVerifyTolerance      = 0.5f;     // 读回校验容差
	constexpr float    kAutoRepairTrigger    = 800.0f;   // 血量低于 80% 才自动写
	constexpr uint32_t kAutoRepairIntervalMs = 1500;

	std::atomic<bool> g_PendingCurrent{ false };
	std::atomic<bool> g_PendingAll{ false };

	std::mutex          g_ReportMutex;
	VehicleRepairReport g_Report;
	uint32_t            g_LastAutoRepair = 0;

	// 修一辆车：逐字段「读旧值 → 写满 → 读回校验」，全部通过才算修好。
	bool RepairOne(uintptr_t vehicleAddress, VehicleRepairReport& report)
	{
		bool allFieldsOk = true;

		for (const RepairField& field : kFields)
		{
			++report.TotalFields;

			float before = 0.0f;
			if (!DMA::Memory().Read(vehicleAddress + field.offset, &before, sizeof(before)))
			{
				allFieldsOk = false;
				continue;
			}

			const float desired = kRepairValue;
			if (!DMA::Memory().Write(vehicleAddress + field.offset, &desired, sizeof(desired)))
			{
				allFieldsOk = false;
				continue;
			}

			float after = 0.0f;
			const bool verified = DMA::Memory().Read(vehicleAddress + field.offset, &after, sizeof(after)) &&
			                      std::fabs(after - desired) <= kVerifyTolerance;
			if (verified)
				++report.VerifiedFields;
			else
				allFieldsOk = false;

			if (field.offset == offsetof(CVehicle, Health))
			{
				report.HealthBefore = before;
				report.HealthAfter = verified ? after : before;
			}
		}

		return allFieldsOk;
	}

	void ProcessRequest(bool all, bool autoTriggered, VehicleRepairReport& report)
	{
		report.Sent = true;
		report.All = all;
		report.Auto = autoTriggered;

		std::vector<uintptr_t> targets;
		if (all)
		{
			for (const SessionVehicle& vehicle : VehicleList::GetSnapshot())
			{
				if (vehicle.Address != 0)
					targets.push_back(vehicle.Address);
			}
		}
		else if (DMA::VehicleAddress != 0)
		{
			targets.push_back(DMA::VehicleAddress);
		}

		report.Requested = static_cast<int>(targets.size());
		if (targets.empty())
		{
			std::println("[VehicleRepair] 无目标：{}",
			             all ? "战局载具未扫描到（需 50 米内有载具）" : "当前没有载具（未上车或载具地址未解析）");
			return;
		}

		int fixed = 0;
		for (uintptr_t address : targets)
		{
			if (RepairOne(address, report))
				++fixed;
		}

		report.Fixed = fixed;
		report.Vehicle = targets.front();

		std::println("[VehicleRepair] 模式={} 触发={} 目标={} 修复成功={} 字段校验 {}/{} 血量 {:.0f} -> {:.0f}",
		             all ? "全部" : "当前", autoTriggered ? "自动" : "手动", report.Requested, report.Fixed,
		             report.VerifiedFields, report.TotalFields, report.HealthBefore, report.HealthAfter);
	}

	void Publish(const VehicleRepairReport& report)
	{
		std::lock_guard<std::mutex> lock(g_ReportMutex);
		g_Report = report;
	}
}

void VehicleRepair::RequestRepairCurrent()
{
	g_PendingCurrent.store(true);
}

void VehicleRepair::RequestRepairAll()
{
	g_PendingAll.store(true);
}

void VehicleRepair::OnDMAFrame()
{
	if (!DMA::IsReady())
		return;

	// 常态零开销：没有请求时这里什么都不做。
	if (g_PendingCurrent.exchange(false))
	{
		VehicleRepairReport report;
		ProcessRequest(false, false, report);
		Publish(report);
	}

	if (g_PendingAll.exchange(false))
	{
		VehicleRepairReport report;
		ProcessRequest(true, false, report);
		Publish(report);
	}

	// 自动修复：只在车内、且血量掉到阈值以下才动手，并做节流。
	if (!bAutoRepair.load() || DMA::VehicleAddress == 0)
		return;

	const uint32_t now = GetTickCount();
	if (g_LastAutoRepair != 0 && (now - g_LastAutoRepair) < kAutoRepairIntervalMs)
		return;

	float health = 0.0f;
	if (!DMA::Memory().Read(DMA::VehicleAddress + offsetof(CVehicle, Health), &health, sizeof(health)))
		return;

	if (health >= kAutoRepairTrigger)
		return;

	g_LastAutoRepair = now;

	VehicleRepairReport report;
	ProcessRequest(false, true, report);
	Publish(report);
}

VehicleRepairReport VehicleRepair::GetLastReport()
{
	std::lock_guard<std::mutex> lock(g_ReportMutex);
	return g_Report;
}

void VehicleRepair::ResetReport()
{
	std::lock_guard<std::mutex> lock(g_ReportMutex);
	g_Report = {};
}
