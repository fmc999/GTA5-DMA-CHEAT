#pragma once
#include "DMA.h"

class NoWanted
{
public:
	static inline bool bEnable = false;

public:
	static bool OnDMAFrame();

private:
	static bool ClearWantedLevel();
};