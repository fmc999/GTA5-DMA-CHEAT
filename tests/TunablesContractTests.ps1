$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\\GTA5_DMA'

function Read-Source([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $sourceRoot $relativePath) -Raw -Encoding UTF8
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "Tunables contract failed: $message"
    }
}

$tunablesH = Read-Source 'Features\\Tunables.h'
$tunables  = Read-Source 'Features\\Tunables.cpp'
$table     = Read-Source 'Features\\TunableTable.h'
$idleH     = Read-Source 'Features\\NoIdleKick.h'
$idle      = Read-Source 'Features\\NoIdleKick.cpp'
$progress  = Read-Source 'Features\\ProgressFeatures.cpp'
$dma       = Read-Source 'Core\\DMA.cpp'
$menu      = Read-Source 'UI\\MenuManager.cpp'
$shell     = Read-Source 'UI\\ConsoleShell.cpp'
$gen       = Get-Content -LiteralPath (Join-Path $root 'GTA5_DMA\\GTA5_DMA\\tools\\gen_tunables.py') -Raw -Encoding UTF8
$project   = Get-Content -LiteralPath (Join-Path $root 'GTA5_DMA\\GTA5_DMA\\GTA5_DMA.vcxproj') -Raw -Encoding UTF8

# 1) 动态定位：候选绝对位置 + 四连组校验（第17轮起移除单值邻域搜索）
Require ($tunablesH -match 'class Tunables') 'Tunables is not declared'
Require ($tunables -match 'FindAnchorElement') 'value-anchor locator is missing'
Require ($tunables -match 'FindIdleKickAnchorElement') 'idle-kick anchor locator is missing'
Require ($tunables -match 'ShapeQuad' ) 'ratio-shape matcher is missing'
# 单值邻域搜索已移除：实机 2026-09-17 观测到它会静默错定位（8 条踢出计时错 4 条），
# 现在只认「候选绝对位置值匹配」与「四连组校验」，两者都不成立就判未定位、拒绝写入。
Require ($tunables -notmatch 'LocateByValue') 'single-value neighborhood search must be removed'
Require ($tunables -match 'VerifyOrFindQuadDirect') 'quad-group verifier is missing'
# 第18轮：金额/额度类（抢劫收益、挑战奖励）—— 区间体检 + 倍率写入
Require ($table -match 'Kind::Value') 'Kind::Value is missing from the tunable table'
Require ($table -match 'IH_PRIMARY_TARGET_VALUE_TEQUILA') 'heist primary target value entry is missing'
Require ($table -match '0x47392u') 'tequila global index drifted'
Require ($table -match '0x47397u') 'sapphire panther statue global index drifted'
Require ($table -match 'SKYDIVING_CHALLENGE_CASH_REWARD_ALL_CHECKPOINTS_COLLECTED') 'skydive reward entry is missing'
Require ($tunables -match 'IsValueKind') 'Tunables has no value-kind accessor'
Require ($tunables -match 'MatchesExpectedBits\(entry, bits\)') 'resolve loop must validate through MatchesExpectedBits (range for Value kind)'
Require ($table -match 'static_assert') 'generated table must keep its compile-time assertions'
Require ($tunables -match 'ReadBlock') 'tunable block paged reader is missing'
Require ($tunables -match 'kPageSize') 'paged read does not use fixed pages'
Require ($tunables -match '块不可用') 'block-unavailable diagnostic is missing'

# 2) 每条都要过默认值体检；写入要有读回校验与拒绝逻辑
Require ($tunables -match '体检未通过') 'entries are not validated against expected defaults'
Require ($tunables -match '拒绝写入') 'write guard is missing'
Require ($tunables -match '读回校验失败') 'write read-back verification is missing'
Require ($tunables -match 'RestoreAll') 'restore path is missing'

# 3) 表里必须有实机对账过的 11 条 + 关键索引/默认值
foreach ($pair in @(
    @('IDLEKICK_WARNING1', '0x40055u', '120000'),
    @('IDLEKICK_KICK', '0x40058u', '900000'),
    @('ConstrainedKick_Warning1', '0x42131u', '30000'),
    @('ConstrainedKick_Kick', '0x42134u', '120000'),
    @('XP_MULTIPLIER', '0x40002u', '1065353216'),
    @('CHARACTER_APPEARANCE_COOLDOWN', '0x44A45u', '2880000'),
    @('CHARACTER_APPEARANCE_CHARGE', '0x44A44u', '100000'))) {
    $name, $index, $default = $pair
    $q = [char]34
    # 表结构：{ "原名", "中文短名", 0x哈希, 0x索引, Kind::X, 默认值, min, max, runId, runOffset, "用途" }
    $pattern = '\{\s*' + $q + $name + $q + ',\s*' + $q + '[^' + $q + ']*' + $q + ',\s*0x[0-9A-Fa-f]+u,\s*' + [regex]::Escape($index) + ',\s*Kind::\w+,\s*' + $default
    Require ($table -match $pattern) "TunableTable entry mismatch for $name (expect index $index default $default)"
}
Require ($table -match 'kTunableBaseAddress = 0x40001') 'tunable base address constant is missing'
Require ($table -match 'gen_tunables.py') 'generated table does not reference its generator'
Require ($gen -match 'TUNABLE_BASE_ADDRESS|0x40001') 'generator does not document the tunable base'
Require ($gen -match 'joaat') 'generator does not compute joaat hashes'

# 4) NoIdleKick 不再硬编码错误索引
Require ($idle -notmatch '0x80056') 'NoIdleKick still hardcodes the wrong 0x80056 index'
Require ($idle -notmatch '0x82132') 'NoIdleKick still hardcodes the wrong 0x82132 index'
Require ($idle -match 'Tunables::Find') 'NoIdleKick does not resolve indices through Tunables'
Require ($idle -match 'Tunables::Write') 'NoIdleKick does not write through Tunables'
Require ($idleH -match 'TUNABLE_BASE_ADDRESS') 'header does not document the historical index bug'

# 5) 进度类功能 + 页面接线
Require ($progress -match 'XP_MULTIPLIER') 'RP multiplier is not wired to its tunable'
Require ($progress -match 'CHARACTER_APPEARANCE_COOLDOWN') 'appearance cooldown tunable is not used'
Require ($progress -match 'CHARACTER_APPEARANCE_CHARGE') 'appearance charge tunable is not used'
Require ($dma -match 'Tunables::Resolve\(\)') 'DMA::Initialize does not resolve tunables'
Require ($dma -match 'ProgressFeatures::OnDMAFrame\(\)') 'DMA thread does not apply progress features'
Require ($dma -match 'NoIdleKick::PrepareForClose\(\)[\s\S]*Tunables::PrepareForClose\(\)|Tunables::PrepareForClose\(\)') 'tunables are not restored on close'
Require ($menu -match 'RenderProgressPageContent') 'progress page is not implemented'
Require ($menu -match 'ProgressFeatures::bRpMultiplier') 'progress page is not wired to the RP multiplier'
Require ($menu -match 'Tunables::ReadLive') 'progress page does not show live tunable values'
Require ($shell -match 'MenuPage::PROGRESS') 'progress page is not registered in the sidebar'
foreach ($name in @('Tunables.cpp', 'ProgressFeatures.cpp')) {
    Require ($project -match [regex]::Escape($name)) "vcxproj is missing $name"
}

Write-Output 'Tunables contract passed.'
