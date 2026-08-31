#pragma once
#include "DMA.h"

struct Health
{
	float CurrentHealth;
	float MaxHealth;
};

class RefreshHealth
{
public:
	static inline bool bEnable = false;
	static inline float HealThresholdPercent = 0.75f;

private:
	static inline Health PlayerHealth{0.0f,0.0f};

public:
	static bool OnDMAFrame();

private:
	static bool UpdatePlayerHealth();
};