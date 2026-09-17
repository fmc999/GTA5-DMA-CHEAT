$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\GTA5_DMA'

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "UI label contract failed: $message"
    }
}

$menuPath = Join-Path $sourceRoot 'UI\MenuManager.cpp'
$menuRaw = Get-Content -LiteralPath $menuPath -Raw -Encoding UTF8
$menu = $menuRaw -replace "`r`n", "`n"
$table = Get-Content -LiteralPath (Join-Path $sourceRoot 'Features\TunableTable.h') -Raw -Encoding UTF8
$globals = Get-Content -LiteralPath (Join-Path $sourceRoot 'Features\ScriptGlobals.h') -Raw -Encoding UTF8

# 1) 中文显示名必须存在（用户看不懂英文原名）
Require ($table -match 'const char\* label;') 'TunableTable entries have no Chinese label field'
Require ($table -match '佩里克：龙舌兰') 'heist tequila label is missing'
Require ($table -match '佩里克：蓝宝石黑豹雕像') 'heist panther statue label is missing'
Require ($table -match '空闲踢出警告 1') 'idle-kick label is missing'
Require ($globals -match 'GetEntryPurpose') 'ScriptGlobals has no Chinese purpose accessor'
Require ($menuRaw -match 'Tunables::GetEntryLabel') 'progress page does not use the tunable Chinese label'
Require ($menuRaw -match 'ScriptGlobals::GetEntryPurpose') 'economy page does not use the script-global Chinese purpose'
Require ($menuRaw -match 'ProgressFeatures::GetValueSlotLabel') 'value card does not use the Chinese slot label'
Require ($menuRaw -match 'SetTooltip') 'raw internal names should remain reachable via tooltip'

# 2) 循环里用字符串字面量做行标签时必须 PushID（否则 ImGui 报 conflicting ID）
#    真实事故：金额倍率卡片 9 行都用 "   实读" → "5 visible items with conflicting ID!"
$lines = $menu -split "`n"
$stack = New-Object System.Collections.Stack
$depth = 0
$issues = @()
for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    $trimmed = $line.Trim()
    if ($trimmed -match '^(for|while)\s*\(') {
        $stack.Push(@($i, $depth))
    }
    if ($line.Contains('{')) { $depth += ([regex]::Matches($line, '\{')).Count }
    if ($line.Contains('}')) {
        $depth -= ([regex]::Matches($line, '\}')).Count
        while ($stack.Count -gt 0 -and $stack.Peek()[1] -ge $depth) {
            $entry = $stack.Pop()
            $start = $entry[0]
            $body = ($lines[$start..$i] -join "`n")
            if ($body -match 'ConsoleTheme::(TextRow|TextRow2|ToggleRow|SliderRow)\(\s*"') {
                if ($body -notmatch 'PushID') {
                    $issues += "line $($start + 1)"
                }
            }
        }
    }
}
Require ($issues.Count -eq 0) ("loops with literal row labels but no PushID: " + ($issues -join ', '))

Write-Output 'UI label contract passed.'
