$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\GTA5_DMA'

function Read-Source([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $sourceRoot $relativePath) -Raw -Encoding UTF8
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "Bring contract failed: $message"
    }
}

$header = Read-Source 'Features\PlayerList.h'
$implementation = Read-Source 'Features\PlayerList.cpp'
$menu = Read-Source 'UI\MenuManager.cpp'

Require ($header -match 'struct BringReport') 'BringReport is not declared'
Require ($header -match '\bRequestBring\b') 'RequestBring is not declared'
Require ($header -match '\bGetLastBring\b') 'GetLastBring is not declared'
Require ($header -match '\bBringPlayerToMe\b') 'BringPlayerToMe worker is not declared'

Require ($implementation -match 'PlayerList::RequestBring\b') 'RequestBring is not implemented'
Require ($implementation -match 'PlayerList::GetLastBring\b') 'GetLastBring is not implemented'
Require ($implementation -match 'PlayerList::BringPlayerToMe\b') 'BringPlayerToMe is not implemented'
Require ($implementation -match 'g_PendingBring\.exchange\(0xFF\)') 'OnDMAFrame does not consume the bring request'
Require ($implementation -match 'g_PendingBring\.store\(playerIndex\)') 'RequestBring does not queue the request'

# 方向契约：只写目标玩家的导航位置；本地玩家的位置在 Bring 函数体内绝不被写入
$bringStart = $implementation.IndexOf('void PlayerList::BringPlayerToMe(')
$bringEnd = $implementation.IndexOf('void PlayerList::ApplyPedAction(', $bringStart)
if ($bringEnd -lt 0) {
    $bringEnd = $implementation.Length
}
Require ($bringStart -ge 0 -and $bringEnd -gt $bringStart) 'BringPlayerToMe body could not be inspected'
$bringBody = $implementation.Substring($bringStart, $bringEnd - $bringStart)

Require ($bringBody -match 'offsetof\(PED, pCNavigation\)') 'BringPlayerToMe does not read the target navigation pointer'
Require ($bringBody -match 'DMA::Memory\(\)\.Write\(navigation \+ offsetof\(CNavigation, Position\)') 'BringPlayerToMe does not write the target navigation position'
Require ($bringBody -match 'readback') 'BringPlayerToMe does not read the position back'
Require ($bringBody -match 'std::fabs\(readback\.x - target\.x\)') 'BringPlayerToMe does not verify the write'
Require ($bringBody -match 'target\.x \+= 2\.0f') 'BringPlayerToMe does not offset the landing spot by 2 m'

# 只许有唯一一次写入，且写入对象必须是目标玩家的导航位置（不是本地玩家的）
$writeCount = ([regex]::Matches($bringBody, 'DMA::Memory\(\)\.Write\(')).Count
Require ($writeCount -eq 1) "BringPlayerToMe must perform exactly one write, found $writeCount"
Require ($bringBody -notmatch 'DMA::NavigationAddress \+ offsetof') 'BringPlayerToMe must not write the local player navigation object'
$localRefs = ([regex]::Matches($bringBody, 'DMA::LocalPlayerLocation')).Count
Require ($localRefs -eq 1) "local player position must only be read once, found $localRefs references"

# UI 接线
Require ($menu -match 'PlayerList::RequestBring\(selected\.PlayerIndex\)') 'session page is missing the bring action'
Require ($menu -match '"拉到我这里"') 'bring button label is missing'
Require ($menu -match 'PlayerList::GetLastBring\(\)') 'session page does not show the bring self-check report'

Write-Output 'Bring contract passed.'
