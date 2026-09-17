$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\GTA5_DMA'

function Read-Source([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $sourceRoot $relativePath) -Raw -Encoding UTF8
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "ScriptGlobals contract failed: $message"
    }
}

$table = Read-Source 'Features\ScriptGlobalsTable.h'
$impl = Read-Source 'Features\ScriptGlobals.cpp'
$header = Read-Source 'Features\ScriptGlobals.h'
$econ = Read-Source 'Features\EconomyFeatures.cpp'
$menu = Read-Source 'UI\MenuManager.cpp'
$shell = Read-Source 'UI\ConsoleShell.cpp'
$dma = Read-Source 'Core\DMA.cpp'

# 1) 索引必须与参考实现逐条一致（改错一格就会把钱打到别的地方）
foreach ($index in @('2708943', '2708952', '2708961', '2708970', '2708979', '2708994', '2709001')) {
    Require ($table -match $index) "safe-claim global index $index is missing"
}
Require ($table -match '23040') 'phone call state global is missing'
Require ($table -match '23046') 'phone call in-progress global is missing'
Require ($table -match '23050') 'phone call incoming global is missing'
Require ($table -match '1970586') 'GTA+ enabled global is missing'
Require ($table -match '1970587') 'GTA+ bits global is missing'
Require ($table -match 'kGtaPlusBitsValue = 0x0A') 'GTA+ bits value must stay 0x0A ((1<<3)|(1<<1))'
Require ($table -match 'kPhoneSilencedState = 6') 'phone silenced state must stay 6'
Require ($table -match 'kEntryCount == 12') 'entry count static_assert is missing'
Require ($table -match 'static_assert') 'compile-time index assertions are missing'

# 2) 合法值区间：每条都要有 min/max，且解析时按区间体检
Require ($table -match 'Kind::Flag,  0, 1') 'flag entries must carry the [0,1] range'
Require ($impl -match 'ValueInRange') 'implementation does not validate against the legal range'
Require ($impl -match '体检未通过') 'missing the range-check diagnostic'
Require ($impl -match '全局块不可用') 'missing the global-block-unavailable diagnostic'

# 3) 写入安全：写前体检 + 写后读回 + 脉冲还原 + 拒写计数
Require ($impl -match '拒绝写入') 'missing the write guard diagnostic'
Require ($impl -match 'g_blockedWrites.fetch_add') 'blocked writes are not counted'
Require ($impl -match '写入校验失败') 'missing read-back verification diagnostic'
Require ($impl -match 'kTriggerHoldMs') 'pulse hold time constant is missing'
Require ($impl -match 'Trigger\(') 'pulse trigger API is missing'
Require ($impl -match 'OnFrame\(\)') 'pulse restore hook is missing'
Require ($impl -match 'RestoreAll') 'restore path is missing'
Require ($impl -match 'DMA::GetGlobalAddress') 'global addressing must go through DMA::GetGlobalAddress'
Require ($impl -notmatch 'invoker|Natives::|Call\(') 'script globals must not use native invocation'

# 4) 生命周期 API
foreach ($symbol in @('Resolve', 'Reset', 'RestoreAll', 'PrepareForClose', 'Find', 'IsResolved', 'ReadLive', 'Write',
                      'Trigger', 'OnFrame', 'GetResolvedCount', 'GetBlockedWriteCount')) {
    Require ($header -match "\b$symbol\b") "ScriptGlobals is missing $symbol"
}

# 5) 经济功能：三个开关 + 一次性动作
foreach ($symbol in @('bAutoClaimSafeEarnings', 'bAutoSilenceCalls', 'bUnlockGTAPlus', 'claimIntervalSeconds')) {
    Require ((Read-Source 'Features\EconomyFeatures.h') -match $symbol) "EconomyFeatures is missing $symbol"
}
foreach ($symbol in @('ClaimAllSafes', 'ClaimSafe', 'SilenceCurrentCall', 'SetGTAPlus', 'OnDMAFrame', 'RestoreAll',
                      'PrepareForClose')) {
    Require ($econ -match "EconomyFeatures::$symbol") "EconomyFeatures is missing $symbol"
}
Require ($econ -match 'PhoneCallWantsSilencing') 'silence logic does not read the phone state globals'
Require ($econ -match 'state == 0 \|\| state == 5 \|\| state == 6') 'silence logic must skip idle/already-silenced states'
Require ($econ -match 'Offsets::GTAPlusPtr') 'GTA+ unlock must also touch the engine-side flag'
Require ($econ -match 'g_gtaPlusOriginal') 'GTA+ engine flag original value is not retained for restore'

# 6) 接线：DMA 初始化 / 主循环 / 退出前还原 + UI 页面
$initStart = $dma.IndexOf('bool DMA::Initialize()')
$initEnd = $dma.IndexOf('bool DMA::DMAThreadEntry()', $initStart)
Require ($initStart -ge 0 -and $initEnd -gt $initStart) 'DMA::Initialize body could not be inspected'
$initBody = $dma.Substring($initStart, $initEnd - $initStart)
Require ($initBody -match 'EconomyFeatures::Resolve\(\);') 'EconomyFeatures is not resolved after attach'

$threadStart = $dma.IndexOf('bool DMA::DMAThreadEntry()')
$threadEnd = $dma.IndexOf('bool DMA::Close()', $threadStart)
$threadBody = $dma.Substring($threadStart, $threadEnd - $threadStart)
Require ($threadBody -match 'EconomyFeatures::OnDMAFrame\(\);') 'the DMA loop does not run EconomyFeatures::OnDMAFrame'

$closeStart = $dma.IndexOf('bool DMA::Close()')
$closeEnd = $dma.IndexOf('bool DMA::UpdateVehicleInformation()', $closeStart)
$closeBody = $dma.Substring($closeStart, $closeEnd - $closeStart)
$prepareIndex = $closeBody.IndexOf('EconomyFeatures::PrepareForClose()')
$resetIndex = $closeBody.IndexOf('Memory().Reset()')
Require ($prepareIndex -ge 0 -and $resetIndex -gt $prepareIndex) 'DMA::Close must restore economy actions before Memory().Reset()'

Require ($menu -match 'RenderEconomyPageContent') 'the economy page implementation is missing'
Require ($menu -match 'EconomyFeatures::ClaimAllSafes\(\)') 'the claim-all button is not wired'
Require ($shell -match 'MenuPage::ECONOMY') 'the economy page is not registered in the shell navigation'

Write-Output 'ScriptGlobals contract passed.'
