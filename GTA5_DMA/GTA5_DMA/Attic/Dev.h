#pragma once

class Dev
{
public:
	static bool Render();

public:
	static inline int DesiredGlobalIndex;
	static inline bool bSpawnCar = false;
	static inline bool bEnable = true; // 添加bEnable成员
	static inline bool bShowAllMenus = true; // 控制是否显示主菜单（不包括开发工具面板和活动功能面板）
};