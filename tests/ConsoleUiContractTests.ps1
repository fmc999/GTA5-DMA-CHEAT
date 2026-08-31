$ErrorActionPreference = 'Stop'

$themeHeader = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/UI/ConsoleTheme.h'
$theme = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/UI/ConsoleTheme.cpp'
$shell = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/UI/ConsoleShell.cpp'
$menu = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/UI/MenuManager.cpp'

if ($themeHeader -notmatch 'ToggleRow' -or $theme -notmatch 'InvisibleButton') {
    throw 'The console theme is missing the reusable toggle row.'
}

if ($themeHeader -notmatch 'SectionHeader' -or $menu -notmatch 'ConsoleTheme::SectionHeader') {
    throw 'Page sections are not using the shared visual hierarchy.'
}

if ($shell -notmatch 'RenderStatusHeader' -or $shell -notmatch 'AddRectFilled') {
    throw 'The console shell is missing the redesigned status header.'
}

if ($shell -notmatch 'kSidebarWidth\s*=\s*190\.0f' -or $shell -notmatch 'RenderQuickControls') {
    throw 'The sidebar was not upgraded to the compact control layout.'
}

# v2.2 UI redo: collapsible sidebar + toast feedback + window persistence
if ($shell -notmatch 'SidebarCollapsed' -or $shell -notmatch 'kSidebarSlim') {
    throw 'The collapsible sidebar layout is missing.'
}
if (-not (Test-Path 'GTA5_DMA/GTA5_DMA/UI/UiToast.cpp') -or -not (Test-Path 'GTA5_DMA/GTA5_DMA/UI/WindowState.cpp')) {
    throw 'Toast notifications or window state persistence files are missing.'
}

if ($menu -match 'RenderPageTitle\("玩家功能"\)') {
    throw 'The player page still renders a duplicate title.'
}

Write-Host 'Console UI contract passed.'
