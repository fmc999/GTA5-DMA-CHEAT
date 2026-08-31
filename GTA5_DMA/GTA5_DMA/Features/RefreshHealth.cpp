#include "pch.h"
#include "RefreshHealth.h"

bool RefreshHealth::OnDMAFrame()
{
	if (bEnable)
		UpdatePlayerHealth();

	return 1;
}

bool RefreshHealth::UpdatePlayerHealth()
{

	DWORD BytesRead = 0x0;

	uintptr_t CurrentHealthPtr = DMA::LocalPlayerAddress + offsetof(PED,CurrentHealth);
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, CurrentHealthPtr, (BYTE*)&PlayerHealth, sizeof(Health), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(Health))
	{
		printf("%d %s\n", __LINE__, __FUNCTION__);
		return 0;;
	}

	//printf("%llx %f\n", CurrentHealthPtr, health.CurrentHealth);
	float HealThreshold = PlayerHealth.MaxHealth * HealThresholdPercent;
	if (PlayerHealth.CurrentHealth < HealThreshold)
	{
		VMMDLL_MemWrite(DMA::vmh, DMA::PID, CurrentHealthPtr, (BYTE*)&PlayerHealth.MaxHealth, sizeof(float));
		puts("Healing player.");
	}

	return 1;
}