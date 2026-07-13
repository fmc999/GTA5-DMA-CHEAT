#pragma once

class Teleport
{
public:
	static bool Render();
	static bool RenderContent();
	static bool OnDMAFrame();
	static bool RequestWaypointTeleport();
	static bool RequestObjectiveTeleport();

public:
	static inline bool bEnable = false;
	static inline bool bRequestedTeleport = false;
	static inline Vec3 StartingLocation = { 0.0f,0.0f,0.0f };
	static inline Vec3 DesiredLocation = { 0.0f,0.0f,0.0f };

private:
	static Vec3 GetWaypointCoords();
	static Vec3 GetObjectiveCoords();
	
	static bool UpdatePlayerStartingLocation();
	static void OverwriteLocation(Vec3 Location);
	static void PreparePlayerWrites(VMMDLL_SCATTER_HANDLE vmsh);
	static void PrepareVehicleWrites(VMMDLL_SCATTER_HANDLE vmsh);
};