#include "pch.h"
#include "Invisibility.h"
#include "DMA.h"
#include "Offsets.h"

void Invisibility::OnDMAFrame()
{
	UpdateInvisibility();
}

void Invisibility::UpdateInvisibility()
{
	if (!DMA::vmh || !DMA::PID || !DMA::LocalPlayerAddress) {
		return;
	}

	const uintptr_t invisibilityAddress = DMA::LocalPlayerAddress + offsetof(PED, InvisibilityFlag);
	BYTE desiredValue = bInvisibility.load() ? 0x01 : 0x27;
	BYTE currentValue = 0;
	DWORD bytesRead = 0;
	if (!VMMDLL_MemReadEx(DMA::vmh, DMA::PID, invisibilityAddress, &currentValue, sizeof(currentValue), &bytesRead, VMMDLL_FLAG_NOCACHE) ||
		bytesRead != sizeof(currentValue)) {
		return;
	}

	if (currentValue != desiredValue) {
		VMMDLL_MemWrite(DMA::vmh, DMA::PID, invisibilityAddress, &desiredValue, sizeof(desiredValue));
	}
}
