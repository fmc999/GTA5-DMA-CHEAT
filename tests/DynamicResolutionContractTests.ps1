$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\GTA5_DMA'

function Read-Source([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $sourceRoot $relativePath) -Raw
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "Dynamic resolution contract failed: $message"
    }
}

$dma = Read-Source 'Core\DMA.cpp'
$dmaHeader = Read-Source 'Core\DMA.h'
$memoryBackend = Read-Source 'Core\MemoryBackend.h'
$memoryBackendSource = Read-Source 'Core\MemoryBackend.cpp'
$offsets = Read-Source 'Core\Offsets.h'
$project = Read-Source 'GTA5_DMA.vcxproj'

Require (-not $dma.Contains('OffsetResolver.h')) 'dynamic resolver include is still active'
Require (-not $dma.Contains('ResolveRuntimeOffsets')) 'dynamic resolver dispatch is still active'
Require (-not $dma.Contains('LoadExecutableSection')) 'PE section scanning is still active in DMA'
Require ($dmaHeader.Contains('MemoryBackend')) 'DMA does not expose the checked backend'
$initializeMatch = [regex]::Match($dma, '(?s)bool DMA::Initialize\(\).*?bool DMA::DMAThreadEntry\(\)')
Require ($initializeMatch.Success) 'DMA::Initialize body was not found'
$initialize = $initializeMatch.Value
Require ($initialize.Contains('Offsets::SetOffsetsByPackageName')) 'static offset selection is missing'
Require ([regex]::IsMatch($initialize, 'Offsets::SetOffsetsByPackageName\("GTA5_Enhanced\.exe"\);\s*Memory\(\)\.Attach\(vmh, PID\);')) 'Enhanced static selection is not retained'
Require ([regex]::IsMatch($initialize, 'Offsets::SetOffsetsByPackageName\("GTA5\.exe"\);\s*Memory\(\)\.Attach\(vmh, PID\);')) 'Legacy static selection is not retained'

Require ($dmaHeader.Contains('GetGlobalValue') -and $dmaHeader.Contains('Memory().Read')) 'GetGlobalValue is not routed through MemoryBackend'
Require ($dmaHeader.Contains('SetGlobalValue') -and $dmaHeader.Contains('Memory().Write')) 'SetGlobalValue is not routed through MemoryBackend'
Require ($dma.Contains('GetGlobalAddress') -and $dma.Contains('Memory().Read')) 'GetGlobalAddress is not routed through MemoryBackend'
Require ($dma.Contains('UpdateEssentials') -and $dma.Contains('BeginScatter')) 'UpdateEssentials does not use ScatterBatch'

Require ($memoryBackend.Contains('ScatterBatch') -and $memoryBackendSource.Contains('VMMDLL_Scatter_Initialize')) 'ScatterBatch is not backed by MemProcFS'
foreach ($file in @('Core\MemoryBackend.cpp', 'Core\MemoryBackend.h')) {
    Require ($project.Contains("Include=""$file""")) "project does not register $file"
}
foreach ($file in @('Core\PatternScanner.cpp', 'Core\PatternScanner.h', 'Core\OffsetResolver.cpp', 'Core\OffsetResolver.h')) {
    Require (-not $project.Contains("Include=""$file""")) "dynamic resolver file remains registered: $file"
}

foreach ($constant in @('WorldPtr_Enhanced', 'WorldPtr_Original', 'GlobalPtr_Enhanced', 'GlobalPtr_Original', 'BlipPtr_Enhanced', 'BlipPtr_Original')) {
    Require ($offsets.Contains($constant)) "static fallback constant was removed: $constant"
}

Require ($offsets -match 'PlayerMgrPtr_Enhanced\s*=\s*0x048591D8') 'Enhanced CNetworkPlayerMgrPtr offset is stale'
Require ($offsets -match 'AimCPedPtr_Enhanced\s*=\s*0x03EDC060') 'Enhanced AimCPedPtr offset is stale'

Write-Output 'Static offset contract passed.'
