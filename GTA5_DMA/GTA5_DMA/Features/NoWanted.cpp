#include "pch.h"
#include "NoWanted.h"

bool NoWanted::OnDMAFrame()
{
	if (bEnable)
		ClearWantedLevel();

	return 1;
}

bool NoWanted::ClearWantedLevel()
{
	DWORD BytesRead = 0x0;

	uintptr_t WantedComponentPtr = DMA::LocalPlayerAddress + 0x10A8;
	uintptr_t WantedComponentAddress = 0x0;
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, WantedComponentPtr, (BYTE*)&WantedComponentAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(uintptr_t))
	{
		printf("%d %s\n", __LINE__, __FUNCTION__);
		return 0;
	}

	uintptr_t WantedLevelAddress = WantedComponentAddress + 0x8E8;
	DWORD WantedLevel = 0x0;
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, WantedLevelAddress, (BYTE*)&WantedLevel, sizeof(DWORD), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(DWORD))
	{
		printf("%d %s\n", __LINE__, __FUNCTION__);
		return 0;
	}

	if (WantedLevel)
	{
		DWORD NewWantedLevel = 0;
		VMMDLL_MemWrite(DMA::vmh, DMA::PID, WantedLevelAddress, (BYTE*)&NewWantedLevel, sizeof(DWORD));
		puts("Cleared Wanted Level.");
	}

	return 1;
}