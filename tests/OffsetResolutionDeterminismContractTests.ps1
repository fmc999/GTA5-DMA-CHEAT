$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\GTA5_DMA'

function Read-Source([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $sourceRoot $relativePath) -Raw -Encoding UTF8
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "Offset resolution determinism contract failed: $message"
    }
}

$resolverH = Read-Source 'Core\OffsetResolver.h'
$resolver  = Read-Source 'Core\OffsetResolver.cpp'
$scannerH  = Read-Source 'Core\PatternScanner.h'
$scanner   = Read-Source 'Core\PatternScanner.cpp'
$dma       = Read-Source 'Core\DMA.cpp'
$offsets   = Read-Source 'Core\Offsets.h'
$idle      = Read-Source 'Features\NoIdleKick.cpp'

# 1) 全量匹配能力（FindUnique 在多命中时会放弃，无法做候选挑选）
Require ($scannerH -match '\bFindAll\b') 'PatternScanner::FindAll is not declared'
Require ($scanner -match 'FindAllResult FindAll\(') 'PatternScanner::FindAll is not implemented'

# 2) 候选校验器 + 每条备用特征码自己的 rel32 位移 / 指令长度
Require ($resolverH -match 'using CandidateValidator') 'CandidateValidator is not declared'
Require ($resolverH -match 'alternativeDisplacementOffset') 'per-alternative displacement offsets are missing'
Require ($resolverH -match 'alternativeInstructionSize') 'per-alternative instruction sizes are missing'
Require ($resolver -match 'alternativeDisplacementOffset\[i\] != 0') 'alternatives ignore their own displacement offset'
Require ($resolver -match 'alternativeInstructionSize\[i\] != 0') 'alternatives ignore their own instruction size'
Require ($resolver -match 'validator\(\*target, &reason\)') 'ResolveOne never runs the candidate validator'
Require ($resolver -match 'no usable candidate for') 'fallback diagnostic does not name the rejected candidates'

# 3) GlobalPtr 目录项：保留 YimMenuV2 主特征码，移除老版 GTA5.exe 备用签名
$globalRow = [regex]::Match($resolver, '(?s)\{"GlobalPtr".*?\},')
Require ($globalRow.Success) 'GlobalPtr catalog entry is missing'
Require ($globalRow.Value -match ',\s*10,\s*14,\s*0,') 'GlobalPtr must keep the YimMenuV2 primary (disp=10/insn=14) with no alternative'
Require ($resolver -notmatch '1, \{\"48 8B 0D \? \? \? \? 0F 1F 44 00\"\}\},') 'the legacy GTA5.exe GlobalPtr alternative is still registered in the catalog'
Require ($resolver -match '老版 GTA5.exe') 'the removal of the legacy GlobalPtr alternative is not documented'

# 4) 静态兜底值 = YimMenuV2 ScriptGlobals 解析值
Require ($offsets -match 'GlobalPtr_Enhanced = 0x3ED15A8') 'static GlobalPtr_Enhanced must stay 0x3ED15A8'

# 5) DMA 侧的 ScriptGlobals 校验器与零填充洞告警
Require ($dma -match 'validateScriptGlobals') 'DMA does not define the ScriptGlobals validator'
Require ($dma -match 'spec\.name == "GlobalPtr"') 'the validator is not wired to GlobalPtr'
Require ($dma -match 'kMinNonZeroChunks') 'the chunk-table check has no minimum-nonzero rule'
Require ($dma -match '\(chunk & 0xF\) != 0') 'the chunk-table check does not require 16-byte alignment'
Require ($dma -match 'DMA::Memory\(\)\.Read\(chunk, &probe') 'the chunk-table check does not probe chunk readability'
Require ($dma -match 'ZEROPAD') 'the zero-filled-page warning is missing'
Require ($dma -match 'result\.validated') 'the resolution log does not report candidate validation'

# 6) tunable 块的不可用信息要能区分「GlobalPtr 解析错」与「分块未分配」
#    （第16轮起 NoIdleKick 改走 Tunables，这段诊断搬进了 Features/Tunables.cpp）
$tunablesSource = Read-Source 'Features\\Tunables.cpp'
Require ($tunablesSource -match '块不可用') 'Tunables does not report the unavailable tunable block'
Require ($tunablesSource -match 'Offsets::GlobalPtr') 'Tunables does not derive the global block from GlobalPtr'
Require ($tunablesSource -match '分块指针') 'Tunables does not print the chunk pointer'

Write-Output 'Offset resolution determinism contract passed.'
