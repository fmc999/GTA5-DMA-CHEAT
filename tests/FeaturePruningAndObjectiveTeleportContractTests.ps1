$ErrorActionPreference = 'Stop'

$consoleShell = Get-Content -Raw 'GTA5_DMA/GTA5_DMA/UI/ConsoleShell.cpp'
$dma = Get-Content -Raw 'GTA5_DMA/GTA5_DMA/Core/DMA.cpp'
$menu = Get-Content -Raw 'GTA5_DMA/GTA5_DMA/UI/MenuManager.cpp'
$teleport = Get-Content -Raw 'GTA5_DMA/GTA5_DMA/Features/Teleport.cpp'
$teleportHeader = Get-Content -Raw 'GTA5_DMA/GTA5_DMA/Features/Teleport.h'
$frame = Get-Content -Raw 'GTA5_DMA/GTA5_DMA/UI/MyImGui.cpp'

$activeConsole = $consoleShell -replace '(?m)^\s*//.*$', ''
$activeDma = $dma -replace '(?m)^\s*//.*$', ''
$activeMenu = $menu -replace '(?m)^\s*//.*$', ''

if ($activeConsole -match 'NavItem\("时间"' -or $activeConsole -match 'NavItem\("任务"') {
    throw '时间或任务导航入口仍处于启用状态。'
}
if ($activeConsole -match 'case MenuPage::TIME: menu.RenderTimePageContent' -or $activeConsole -match 'case MenuPage::HEIST_DIVIDEND: menu.RenderHeistDividendPageContent') {
    throw '时间或任务页面路由仍处于启用状态。'
}
if ($activeDma -match 'TimeControl::OnDMAFrame\(\)' -or $activeDma -match 'PlayerChaser::OnDMAFrame\(\)' -or $activeDma -match 'HeistDividend::OnDMAFrame\(\)') {
    throw '已停用功能仍在 DMA 主循环执行。'
}
if ($activeMenu -match 'ImGui::Checkbox\("启用追战局"') {
    throw '追战局设置入口仍处于启用状态。'
}
foreach ($source in @('TimeControl.cpp', 'HeistDividend.cpp', 'PlayerChaser.cpp')) {
    if (-not (Test-Path "GTA5_DMA/GTA5_DMA/Attic/$source")) {
        throw "停用功能源码被删除: $source"
    }
}
if ($consoleShell -notmatch 'retained' -or $dma -notmatch 'retained' -or $menu -notmatch 'retained') {
    throw '停用调用点缺少保留源码的恢复说明。'
}

if ($teleportHeader -notmatch 'RequestObjectiveTeleport' -or $teleportHeader -notmatch 'GetObjectiveCoords') {
    throw 'Teleport 缺少任务点传送接口。'
}
if ($teleport -notmatch 'ImGui::Button\(.*F6' -or $teleport -notmatch 'ImGui::SameLine\(\)') {
    throw '任务点按钮未放在标记点按钮旁边。'
}
if ($teleport -notmatch 'GameType::GTA5_Enhanced') {
    throw '任务点传送缺少增强版限制。'
}
foreach ($id in @(1, 225, 427, 478, 501, 523, 556, 432, 443)) {
    if ($teleport -notmatch "\b$id\b") {
        throw "任务点图标筛选缺少 ID $id。"
    }
}
if ($teleport -notmatch '0x48' -or $teleport -notmatch 'Color') {
    throw '任务点扫描未读取 Blip 颜色。'
}
if ($frame -notmatch 'VK_F6' -or $frame -notmatch 'Teleport::RequestObjectiveTeleport\(\)') {
    throw 'F6 没有绑定到任务点传送请求。'
}

Write-Host 'Feature pruning/objective teleport contract passed.'
