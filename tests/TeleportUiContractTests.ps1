$ErrorActionPreference = 'Stop'

$menu = Get-Content -Raw 'GTA5_DMA/GTA5_DMA/UI/MenuManager.cpp'
$frame = Get-Content -Raw 'GTA5_DMA/GTA5_DMA/UI/MyImGui.cpp'
$teleportHeader = Get-Content -Raw 'GTA5_DMA/GTA5_DMA/Features/Teleport.h'

if ($menu -notmatch 'Teleport::RenderContent\(\)') {
    throw '传送页没有嵌入完整的传送菜单内容。'
}

if ($frame -notmatch 'VK_F5' -or $frame -notmatch 'Teleport::RequestWaypointTeleport\(\)') {
    throw 'F5 没有绑定到标点传送请求。'
}

if ($teleportHeader -notmatch 'RequestWaypointTeleport') {
    throw 'Teleport 缺少统一的标点传送请求入口。'
}

Write-Host 'Teleport UI/hotkey contract passed.'
