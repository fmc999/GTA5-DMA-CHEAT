$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\GTA5_DMA'

function Read-Source([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $sourceRoot $relativePath) -Raw -Encoding UTF8
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "NoIdleKick lifecycle contract failed: $message"
    }
}

$header = Read-Source 'Features\NoIdleKick.h'
$implementation = Read-Source 'Features\NoIdleKick.cpp'
$tunables = Read-Source 'Features\Tunables.cpp'
$table = Read-Source 'Features\TunableTable.h'
$dma = Read-Source 'Core\DMA.cpp'
$menu = Read-Source 'UI\MenuManager.cpp'

# 1) 生命周期 API 仍在
foreach ($symbol in @('Resolve', 'RestoreAll', 'PrepareForClose', 'OnDMAFrame')) {
    Require ($header -match "\b$symbol\b") "header is missing $symbol"
    Require ($implementation -match "NoIdleKick::$symbol\b") "implementation is missing $symbol"
}

# 2) 本轮起索引一律走 Tunables —— 旧的 0x80056 / 0x82132 硬编码必须消失
#    （旧值比真身 0x40055 / 0x42131 正好多加了 TUNABLE_BASE_ADDRESS 0x40001 一遍）
Require ($implementation -notmatch '0x80056') 'implementation still hardcodes the bogus 0x80056 index'
Require ($implementation -notmatch '0x82132') 'implementation still hardcodes the bogus 0x82132 index'
Require ($header -match 'TUNABLE_BASE_ADDRESS') 'header does not document the historical index bug'
Require ($implementation -match 'Tunables::Find\(') 'NoIdleKick does not resolve its entries through Tunables'
Require ($implementation -match 'Tunables::IsResolved') 'NoIdleKick does not check resolution state'
Require ($implementation -match 'Tunables::Write\(') 'NoIdleKick does not write through Tunables'
Require ($implementation -match 'Tunables::ReadLive') 'NoIdleKick does not read the live value before writing'
Require ($implementation -match 'Tunables::GetOriginal') 'NoIdleKick does not use the recorded original value for restore'

$restoreStart = $implementation.IndexOf('bool NoIdleKick::RestoreAll()')
$restoreEnd = $implementation.IndexOf('void NoIdleKick::Reset()', $restoreStart)
Require ($restoreStart -ge 0 -and $restoreEnd -gt $restoreStart) 'Restore/Prepare body could not be inspected'
$restoreBody = $implementation.Substring($restoreStart, $restoreEnd - $restoreStart)
Require ($restoreBody -match 'return Tunables::RestoreAll\(\)') 'RestoreAll does not delegate to Tunables'
Require ($restoreBody -match 'return Tunables::PrepareForClose\(\)') 'PrepareForClose does not delegate to Tunables'

# 3) 8 个槽位名必须与表里的条目一一对应
foreach ($name in @('IDLEKICK_WARNING1', 'IDLEKICK_WARNING2', 'IDLEKICK_WARNING3', 'IDLEKICK_KICK',
                    'ConstrainedKick_Warning1', 'ConstrainedKick_Warning2', 'ConstrainedKick_Warning3',
                    'ConstrainedKick_Kick')) {
    Require ($implementation -match [regex]::Escape($name)) "slot table is missing $name"
    Require ($table -match [regex]::Escape($name)) "TunableTable is missing $name"
}

# 4) 开启时顶到上限、关闭时还原；写前有节流
$frameStart = $implementation.IndexOf('void NoIdleKick::OnDMAFrame()')
$frameEnd = $implementation.IndexOf('bool NoIdleKick::RestoreAll()', $frameStart)
Require ($frameStart -ge 0 -and $frameEnd -gt $frameStart) 'OnDMAFrame body could not be inspected'
$frameBody = $implementation.Substring($frameStart, $frameEnd - $frameStart)
Require ($frameBody -match 'INT_MAX') 'OnDMAFrame does not raise the kick timers to INT_MAX'
Require ($frameBody -match 'bEnable\.load\(\)') 'OnDMAFrame ignores the enable switch'
Require ($frameBody -match 'g_lastAttempt') 'OnDMAFrame has no per-slot throttle'
Require ($frameBody -match 'want \? INT_MAX : original') 'OnDMAFrame does not restore the original timer when disabled'

# 5) Tunables 侧：动态定位 + 默认值体检 + 写入守卫
Require ($tunables -match 'FindAnchorElement') 'Tunables has no value-anchor locator'
Require ($tunables -match '体检未通过') 'Tunables does not validate entries against expected defaults'
Require ($tunables -match '拒绝写入') 'Tunables has no write guard'

# 6) 接线：DMA 初始化解析 / 主循环 / 退出还原 + UI 开关
$initStart = $dma.IndexOf('bool DMA::Initialize()')
$initEnd = $dma.IndexOf('bool DMA::DMAThreadEntry()', $initStart)
Require ($initStart -ge 0 -and $initEnd -gt $initStart) 'DMA::Initialize body could not be inspected'
$initBody = $dma.Substring($initStart, $initEnd - $initStart)
Require ($initBody -match 'NoIdleKick::Resolve\(\);') 'NoIdleKick is not resolved after attach'
Require ($initBody -match 'Tunables::Resolve\(\);') 'Tunables are not resolved after attach'

$threadStart = $dma.IndexOf('bool DMA::DMAThreadEntry()')
$threadEnd = $dma.IndexOf('bool DMA::Close()', $threadStart)
$threadBody = $dma.Substring($threadStart, $threadEnd - $threadStart)
Require ($threadBody -match 'NoIdleKick::OnDMAFrame\(\);') 'the DMA loop does not run NoIdleKick::OnDMAFrame'

$closeStart = $dma.IndexOf('bool DMA::Close()')
$closeEnd = $dma.IndexOf('bool DMA::UpdateVehicleInformation()', $closeStart)
Require ($closeStart -ge 0 -and $closeEnd -gt $closeStart) 'DMA::Close body could not be inspected'
$closeBody = $dma.Substring($closeStart, $closeEnd - $closeStart)
$prepareIndex = $closeBody.IndexOf('NoIdleKick::PrepareForClose()')
$memoryResetIndex = $closeBody.IndexOf('Memory().Reset()')
Require ($prepareIndex -ge 0 -and $memoryResetIndex -gt $prepareIndex) 'DMA::Close must restore NoIdleKick before Memory().Reset()'

Require ($menu -match 'NoIdleKick::bEnable') 'player protection UI is not wired to NoIdleKick'
Require ($menu -match 'no_idle_kick') 'player protection toggle is missing'

Write-Output 'NoIdleKick lifecycle contract passed.'
