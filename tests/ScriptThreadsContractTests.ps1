$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\GTA5_DMA'

function Read-Source([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $sourceRoot $relativePath) -Raw -Encoding UTF8
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "ScriptThreads contract failed: $message"
    }
}

$header = Read-Source 'Features\ScriptThreads.h'
$impl = Read-Source 'Features\ScriptThreads.cpp'
$resolver = Read-Source 'Core\OffsetResolver.cpp'
$dma = Read-Source 'Core\DMA.cpp'
$menu = Read-Source 'UI\MenuManager.cpp'

# 1) 地址来源：必须是偏移目录里那条 ScriptThreads 签名（与 YimMenuV2 pointers/Pointers.cpp 同名条目一致）
Require ($resolver -match 'LocalScriptsPtr') 'the offsets catalog lost the LocalScriptsPtr entry'
Require ($resolver -match '48 8B 05 \? \? \? \? 48 89 34 F8 48 FF C7 48 39 FB 75 97') `
    'LocalScriptsPtr signature drifted from the reference ScriptThreads pattern'
Require ($impl -match 'Offsets::LocalScriptsPtr') 'ScriptThreads must resolve through Offsets::LocalScriptsPtr'

# 2) atArray 头布局：data@0x00 / count@0x08 / capacity@0x0A
Require ($impl -match 'array \+ 0x08') 'atArray count offset (0x08) is not read'
Require ($impl -match 'array \+ 0x0A') 'atArray capacity offset (0x0A) is not read'
Require ($header -match 'kMaxThreads') 'thread cap constant is missing'

# 3) 线程结构体偏移（Enhanced，逐条对照 YimMenuV2 types/script/scrThread.hpp）
Require ($header -match 'kStackOffset = 0xB8') 'm_Stack offset must be 0xB8'
Require ($header -match 'kHashOffset = 0x150') 'm_ScriptHash offset must be 0x150'
Require ($header -match 'kNameOffset = 0x154') 'm_ScriptName offset must be 0x154'
Require ($impl -match 'kHashOffset') 'thread hash is not read at the declared offset'
Require ($impl -match 'kStackOffset') 'thread stack is not read at the declared offset'
Require ($impl -match 'kNameOffset') 'thread name is not read at the declared offset'

# 4) 本轮只提供只读通道（写入留给下一轮，必须没有写调用）
Require ($impl -notmatch 'Memory\(\)\.Write') 'ScriptThreads must stay read-only this round'
Require ($impl -match 'GetLocalAddress') 'locals addressing helper is missing'
Require ($impl -match 'localIndex\) \* 8') 'locals addressing must be stack + index*8'

# 5) 查询 API
foreach ($symbol in @('Resolve', 'Reset', 'IsReady', 'Count', 'GetThreadAddress', 'Get', 'CopyAll', 'FindByHash',
                      'FindByName', 'GetLocalAddress', 'GetFreemodeHash', 'LogRunningScripts', 'GetSummary')) {
    Require ($header -match "\b$symbol\b") "ScriptThreads is missing $symbol"
}

# 6) 接线：初始化后打印一次运行脚本清单；退出时重置
$initStart = $dma.IndexOf('bool DMA::Initialize()')
$initEnd = $dma.IndexOf('bool DMA::DMAThreadEntry()', $initStart)
Require ($initStart -ge 0 -and $initEnd -gt $initStart) 'DMA::Initialize body could not be inspected'
$initBody = $dma.Substring($initStart, $initEnd - $initStart)
Require ($initBody -match 'ScriptThreads::LogRunningScripts') 'the script-thread listing is not produced after attach'

$closeStart = $dma.IndexOf('bool DMA::Close()')
$closeEnd = $dma.IndexOf('bool DMA::UpdateVehicleInformation()', $closeStart)
Require ($closeStart -ge 0 -and $closeEnd -gt $closeStart) 'DMA::Close body could not be inspected'
$closeBody = $dma.Substring($closeStart, $closeEnd - $closeStart)
Require ($closeBody -match 'ScriptThreads::Reset\(\)') 'DMA::Close does not reset the script-thread state'

Require ($menu -match 'ScriptThreads::GetSummary\(\)') 'the script-thread self-check card is not wired'
Require ($menu -match 'ScriptThreads::Get\(i, info\)') 'the script-thread table does not enumerate threads'

Write-Output 'ScriptThreads contract passed.'
