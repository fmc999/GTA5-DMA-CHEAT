$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\GTA5_DMA'

function Read-Source([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $sourceRoot $relativePath) -Raw -Encoding UTF8
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "VehicleRepair contract failed: $message"
    }
}

$header = Read-Source 'Features\VehicleRepair.h'
$implementation = Read-Source 'Features\VehicleRepair.cpp'
$reclass = Read-Source 'Core\Reclass.h'
$features = Read-Source 'Features\Features.h'
$dma = Read-Source 'Core\DMA.cpp'
$menu = Read-Source 'UI\MenuManager.cpp'
$project = Get-Content -LiteralPath (Join-Path $root 'GTA5_DMA\GTA5_DMA\GTA5_DMA.vcxproj') -Raw -Encoding UTF8

foreach ($symbol in @('RequestRepairCurrent', 'RequestRepairAll', 'OnDMAFrame', 'GetLastReport', 'ResetReport')) {
    Require ($header -match "\b$symbol\b") "header is missing $symbol"
    Require ($implementation -match "VehicleRepair::$symbol\b") "implementation is missing $symbol"
}

# 四段健康值必须有钉死的偏移，且断言值来自 YimMenuV2 的 CVehicle 布局
$offsets = @(
    @('Health', '0x280'),
    @('BodyHealth', '0x830'),
    @('PetrolTankHealth', '0x834'),
    @('EngineHealth', '0x910')
)
foreach ($pair in $offsets) {
    $field, $offset = $pair
    $pattern = "static_assert\(offsetof\(CVehicle,\s*$field\)\s*==\s*$offset\)"
    Require ($reclass -match $pattern) "missing exact CVehicle offset guard: $field == $offset"
    Require ($implementation -match "offsetof\(CVehicle,\s*$field\)") "repair field table is missing $field"
}

# 写入值、读回校验、字段计数必须都在（避免出现只写不验的实现）
$repairStart = $implementation.IndexOf('bool RepairOne(')
$repairEnd = $implementation.IndexOf('void ProcessRequest(', $repairStart)
Require ($repairStart -ge 0 -and $repairEnd -gt $repairStart) 'RepairOne body could not be inspected'
$repairBody = $implementation.Substring($repairStart, $repairEnd - $repairStart)
Require ($repairBody -match 'DMA::Memory\(\)\.Read\(vehicleAddress \+ field\.offset') 'RepairOne does not read the previous value'
Require ($repairBody -match 'DMA::Memory\(\)\.Write\(vehicleAddress \+ field\.offset') 'RepairOne does not write through MemoryBackend'
Require ($repairBody -match 'kRepairValue') 'RepairOne does not use the shared repair value'
Require ($repairBody -match 'std::fabs\(after - desired\) <= kVerifyTolerance') 'RepairOne does not verify the write with a tolerance'
Require ($repairBody -match '\+\+report\.TotalFields') 'RepairOne does not count attempted fields'
Require ($repairBody -match '\+\+report\.VerifiedFields') 'RepairOne does not count verified fields'

# 一键修复必须取战局载具快照（不能只修当前载具）
$processStart = $repairEnd
$processEnd = $implementation.IndexOf('void Publish(', $processStart)
Require ($processStart -ge 0 -and $processEnd -gt $processStart) 'ProcessRequest body could not be inspected'
$processBody = $implementation.Substring($processStart, $processEnd - $processStart)
Require ($processBody -match 'VehicleList::GetSnapshot\(\)') 'repair-all does not read the session vehicle snapshot'
Require ($processBody -match 'DMA::VehicleAddress') 'repair-current does not use the local vehicle address'
Require ($processBody -match 'targets\.empty\(\)') 'ProcessRequest does not guard the empty-target case'
Require ($processBody -match 'report\.Fixed = fixed') 'ProcessRequest does not publish the fixed count'

# 自动修复必须有阈值与节流
$frameStart = $implementation.IndexOf('void VehicleRepair::OnDMAFrame()')
$frameEnd = $implementation.IndexOf('VehicleRepairReport VehicleRepair::GetLastReport()', $frameStart)
Require ($frameStart -ge 0 -and $frameEnd -gt $frameStart) 'OnDMAFrame body could not be inspected'
$frameBody = $implementation.Substring($frameStart, $frameEnd - $frameStart)
Require ($frameBody -match 'g_PendingCurrent\.exchange\(false\)') 'OnDMAFrame does not consume the current-vehicle request'
Require ($frameBody -match 'g_PendingAll\.exchange\(false\)') 'OnDMAFrame does not consume the repair-all request'
Require ($frameBody -match 'bAutoRepair\.load\(\)') 'OnDMAFrame ignores the auto-repair switch'
Require ($frameBody -match 'kAutoRepairTrigger') 'OnDMAFrame does not use the auto-repair threshold'
Require ($frameBody -match 'kAutoRepairIntervalMs') 'OnDMAFrame does not throttle auto repair'

# 接线：聚合头 / DMA 主循环 / 工程文件 / 页面
Require ($features -match '#include "VehicleRepair\.h"') 'Features.h does not include VehicleRepair.h'
Require ($dma -match 'VehicleRepair::OnDMAFrame\(\);') 'DMA thread does not run VehicleRepair::OnDMAFrame'
Require ($project -match 'Features\\VehicleRepair\.cpp') 'vcxproj is missing Features\VehicleRepair.cpp'
Require ($project -match 'Features\\VehicleRepair\.h') 'vcxproj is missing Features\VehicleRepair.h'
Require ($menu -match 'VehicleRepair::bAutoRepair') 'vehicle page is not wired to the auto-repair switch'
Require ($menu -match 'veh_auto_repair') 'auto-repair toggle id is missing'
Require ($menu -match 'VehicleRepair::RequestRepairCurrent\(\)') 'vehicle page is missing the repair-current action'
Require ($menu -match 'VehicleRepair::RequestRepairAll\(\)') 'vehicle page is missing the repair-all action'
Require ($menu -match 'VehicleRepair::GetLastReport\(\)') 'vehicle page does not show the repair self-check report'

Write-Output 'VehicleRepair contract passed.'
