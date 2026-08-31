#include "pch.h"
#include "TimeControl.h"
#include "DMA.h"
#include "Offsets.h"

void TimeControl::OnDMAFrame()
{
	// Sync UI values to atomic values
	SyncUI();
	
	if (!bEnable.load())
		return;

	UpdateTime();
}

void TimeControl::UpdateTime()
{
	// Time structure in GTA5:
	// Day: GTA5_Enhanced.exe+47A3F70-0C (Byte)
	// Hour: GTA5_Enhanced.exe+47A3F70 (Byte)
	// Minute: GTA5_Enhanced.exe+47A3F70+4 (Byte)
	// Second: GTA5_Enhanced.exe+47A3F70+8 (Byte)

	// Calculate the base address for time
	uintptr_t timeBaseAddress = DMA::BaseAddress + Offsets::TimeBasePtr;

	// Write day value
	BYTE dayValue = static_cast<BYTE>(day.load());
	VMMDLL_MemWrite(DMA::vmh, DMA::PID, timeBaseAddress - 0x0C, (BYTE*)&dayValue, sizeof(BYTE));

	// Write hour value
	BYTE hourValue = static_cast<BYTE>(hour.load());
	VMMDLL_MemWrite(DMA::vmh, DMA::PID, timeBaseAddress, (BYTE*)&hourValue, sizeof(BYTE));

	// Write minute value
	BYTE minuteValue = static_cast<BYTE>(minute.load());
	VMMDLL_MemWrite(DMA::vmh, DMA::PID, timeBaseAddress + 0x04, (BYTE*)&minuteValue, sizeof(BYTE));

	// Write second value
	BYTE secondValue = static_cast<BYTE>(second.load());
	VMMDLL_MemWrite(DMA::vmh, DMA::PID, timeBaseAddress + 0x08, (BYTE*)&secondValue, sizeof(BYTE));
}

void TimeControl::SyncUI()
{
	// Sync from UI to atomic values
	bEnable.store(bEnableUI);
	day.store(dayUI);
	hour.store(hourUI);
	minute.store(minuteUI);
	second.store(secondUI);
}