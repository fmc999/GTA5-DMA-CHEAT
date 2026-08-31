$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$armorHeader = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'GTA5_DMA/GTA5_DMA/Features/ArmorManager.h')
$armorSource = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'GTA5_DMA/GTA5_DMA/Features/ArmorManager.cpp')
$menu = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'GTA5_DMA/GTA5_DMA/UI/MenuManager.cpp')

function Require([bool]$condition, [string]$message) {
    if (-not $condition) { throw "Armor refresh contract failed: $message" }
}

Require $armorHeader.Contains('bAutoRefreshArmor') 'independent armor toggle is missing'
Require $armorHeader.Contains('ArmorRefreshThreshold') 'armor threshold state is missing'
Require $armorHeader.Contains('ArmorRefreshValue') 'armor target state is missing'
Require $armorHeader.Contains('ShouldAutoRefresh') 'pure threshold predicate is missing'
Require $armorSource.Contains('bLockArmor') 'existing lock behavior is missing'
Require $armorSource.Contains('bAutoRefreshArmor') 'auto-refresh branch is missing'
Require $armorSource.Contains('ArmorRefreshValue') 'auto-refresh does not use the configured target'
Require $armorSource.Contains('ShouldAutoRefresh') 'strict threshold predicate is not used'
Require $armorSource.Contains('DMA::Memory().Read') 'armor reads bypass the checked backend'
Require $armorSource.Contains('DMA::Memory().Write') 'armor writes bypass the checked backend'

$healthIndex = $menu.IndexOf('refresh_health')
$armorIndex = $menu.IndexOf('refresh_armor')
Require ($healthIndex -ge 0 -and $armorIndex -gt $healthIndex) 'armor toggle is not below health toggle'
Require $menu.Contains('refresh_armor') 'armor toggle id is missing'
Require $menu.Contains('ArmorManager::bAutoRefreshArmor') 'armor toggle is not bound to ArmorManager'

Write-Host 'Armor refresh contract passed.'
