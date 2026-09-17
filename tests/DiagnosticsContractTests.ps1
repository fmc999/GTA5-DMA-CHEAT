$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\GTA5_DMA'

function Read-Source([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $sourceRoot $relativePath) -Raw -Encoding UTF8
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "Diagnostics contract failed: $message"
    }
}

$header = Read-Source 'Core\Diagnostics.h'
$impl = Read-Source 'Core\Diagnostics.cpp'
$dma = Read-Source 'Core\DMA.cpp'
$main = Read-Source 'Core\main.cpp'
$menu = Read-Source 'UI\MenuManager.cpp'

# 1) 诊断落盘的三个入口：启动自动 / --diag / UI 按钮
Require ($impl -match 'WriteReport') 'Diagnostics::WriteReport is missing'
Require ($impl -match 'GTA5_DMA_diag.txt') 'default diagnostics file name is missing'
Require ($impl -match 'GetModuleFileNameA') 'diagnostics path must be derived from the exe directory'
Require ($impl -match 'ofstream') 'diagnostics must be written with a file stream'

$initStart = $dma.IndexOf('bool DMA::Initialize()')
$initEnd = $dma.IndexOf('bool DMA::DMAThreadEntry()', $initStart)
Require ($initStart -ge 0 -and $initEnd -gt $initStart) 'DMA::Initialize body could not be inspected'
$initBody = $dma.Substring($initStart, $initEnd - $initStart)
Require ($initBody -match 'Diagnostics::WriteReport\(\);') 'the report is not written automatically after attach'

Require ($main -match '"--diag"') 'the --diag command line mode is missing'
Require ($menu -match 'Diagnostics::WriteReport\(nullptr\)') 'the UI export button is not wired'

# 2) 报告内容：偏移 / tunable / 全局动作格 / 线程 / 块范围 / 经济状态
foreach ($needle in @('已解析偏移', 'GlobalPtr', 'LocalScriptsPtr(线程表)', '脚本 tunable',
                      '脚本全局动作格', '脚本线程', '可读范围探测', '经济功能', '构建标记')) {
    Require ($impl -match [regex]::Escape($needle)) "report section is missing: $needle"
}
Require ($impl -match 'Tunables::IsValueKind') 'report does not distinguish value-kind tunables'
Require ($impl -match 'GetLocatedBy') 'report does not print how each tunable was located'
Require ($impl -match '运行时表') 'report must include the runtime-tables section'
Require ($impl -match 'RuntimeTables::GetReport') 'runtime tables report is not wired in'

# 3) 常驻实例约束必须写清楚（避免后人又去做外部探针）
Require ($header -match '独占') 'header must document the exclusive-device reason for file logging'

Write-Output 'Diagnostics contract passed.'
