$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\GTA5_DMA'
$menuPath = Join-Path $sourceRoot 'UI\MenuManager.cpp'
$raw = Get-Content -LiteralPath $menuPath -Raw -Encoding UTF8
$menu = $raw -replace "`r`n", "`n"
$lines = $menu -split "`n"

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "UI layout contract failed: $message"
    }
}

# 真实事故（用户截图反馈）：
#   ① 金额卡片声明 12 行、内容 20 行（9 条 × 2 行）→ 溢出压住下一个框
#   ② 自检框声明 420 像素、却只 Advance 了 TitledBoxHeight(1) → 下面的「说明」框直接叠在表格上
# 这两类都必须用静态检查挡住。

$rowHelpers = @('ToggleRow', 'SliderRow', 'TextRow', 'TextRow2', 'ButtonRow', 'SliderRow2')
$knownLoopCounts = @{
    'ProgressFeatures::kValueSlotCount' = 9
    'Tunables::kSlotCount'              = 20
    'ScriptGlobals::kSlotCount'         = 12
    'EconomyFeatures::kSafeCount'       = 7
}

$problems = @()

for ($i = 0; $i -lt $lines.Count; $i++) {
    $m = [regex]::Match($lines[$i], 'ConsoleTheme::BoxBegin\(\s*"([^"]+)"\s*,\s*(\d+)\s*,')
    if (-not $m.Success) { continue }

    $boxId = $m.Groups[1].Value
    $declared = [int]$m.Groups[2].Value

    # 收集该框的 body（到 BoxEnd() 且括号配平）
    $depth = 0
    $body = @()
    for ($j = $i; $j -lt $lines.Count; $j++) {
        $body += $lines[$j]
        $depth += ([regex]::Matches($lines[$j], '\{')).Count - ([regex]::Matches($lines[$j], '\}')).Count
        if ($j -gt $i -and $lines[$j] -match 'BoxEnd\(\)') { break }
    }

    # 统计“行”数：普通行调用 1 行；处于 for 循环体内的行按已知常量倍数计
    # （必须按花括号深度判断“真的在循环体内”，否则框内计数用的 for 会被误当成渲染循环）
    $count = 0
    $depth = 0
    $loopStack = New-Object System.Collections.ArrayList
    for ($k = 0; $k -lt $body.Count; $k++) {
        $line = $body[$k]

        # 先处理本行的循环头（压栈）
        if ($line -match '^\s*for\s*\(' -or $line -match '\bfor\s*\(') {
            $mult = 1
            foreach ($key in $knownLoopCounts.Keys) {
                if ($line -match [regex]::Escape($key)) { $mult = $knownLoopCounts[$key] }
            }
            [void]$loopStack.Add(@{ Depth = $depth; Mult = $mult })
        }

        $isRow = $false
        foreach ($helper in $rowHelpers) {
            if ($line -match ("ConsoleTheme::" + $helper + "\(")) { $isRow = $true; break }
        }
        if ($isRow) {
            $mult = 1
            foreach ($frame in $loopStack) { $mult = $mult * $frame.Mult }
            $count += $mult
        }

        $opens = ([regex]::Matches($line, '\{')).Count
        $closes = ([regex]::Matches($line, '\}')).Count
        $depth += $opens - $closes
        # 退出已结束的循环
        while ($loopStack.Count -gt 0 -and $depth -le $loopStack[$loopStack.Count - 1].Depth) {
            $loopStack.RemoveAt($loopStack.Count - 1)
        }
    }

    if ($count -gt $declared) {
        $problems += "$boxId 声明 $declared 行但内容约 $count 行"
    }
}

Require ($problems.Count -eq 0) ("box height smaller than content: " + ($problems -join ' | '))

# 像素框必须用 TitledBoxPixels 推进同样的高度
$pixelMismatch = @()
for ($i = 0; $i -lt $lines.Count; $i++) {
    $m = [regex]::Match($lines[$i], 'ConsoleTheme::BoxBeginPixels\(\s*"([^"]+)"\s*,\s*([0-9.]+)f')
    if (-not $m.Success) { continue }
    $boxId = $m.Groups[1].Value
    $px = $m.Groups[2].Value

    $advance = $null
    for ($j = $i; $j -lt [Math]::Min($i + 200, $lines.Count); $j++) {
        if ($lines[$j] -match 'BoxEnd\(\)') {
            for ($k = $j; $k -lt [Math]::Min($j + 4, $lines.Count); $k++) {
                $am = [regex]::Match($lines[$k], 'Advance\([^,]+,\s*([^\)]+)\)')
                if ($am.Success) { $advance = $am.Groups[1].Value.Trim(); break }
            }
            break
        }
    }
    if ($null -eq $advance -or $advance -notmatch ("TitledBoxPixels\(\s*" + [regex]::Escape($px) + "f")) {
        $pixelMismatch += "$boxId（声明 ${px}px，实际推进 '$advance'）"
    }
}
Require ($pixelMismatch.Count -eq 0) ("pixel boxes not advanced by TitledBoxPixels(same px): " + ($pixelMismatch -join ' | '))

# 金额卡片必须按实际条数算高度（不许再写死数字）
Require ($menu -match 'const int valueRows = 2 \+ static_cast<int>\(ProgressFeatures::kValueSlotCount\)') `
    'value card must compute its height from the slot count'

Write-Output 'UI layout contract passed.'
