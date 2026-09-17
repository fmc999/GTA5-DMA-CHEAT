$ErrorActionPreference = 'Stop'

$header = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/Features/AimAid.h'
$implementation = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/Features/AimAid.cpp'
$dma = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/Core/DMA.cpp'

foreach ($symbol in @('PrepareForClose', 'RestoreAll')) {
    if ($header -notmatch "\b$symbol\b" -or $implementation -notmatch "AimAid::$symbol\b") {
        throw "AimAid lifecycle API is missing: $symbol"
    }
}

foreach ($state in @('g_originalSite', 'g_processId', 'g_processBase', 'g_hasProcessIdentity')) {
    if ($implementation -notmatch "\b$state\b") {
        throw "AimAid process-original state is missing: $state"
    }
}

$resolveStart = $implementation.IndexOf('bool AimAid::Resolve()')
$resolveEnd = $implementation.IndexOf('bool AimAid::RestoreAll()', $resolveStart)
if ($resolveStart -lt 0 -or $resolveEnd -le $resolveStart) {
    throw 'AimAid::Resolve implementation could not be inspected.'
}
$resolveBody = $implementation.Substring($resolveStart, $resolveEnd - $resolveStart)
if ($resolveBody -notmatch '\bsameProcess\b' -or
    $resolveBody -notmatch 'g_originalSite\[i\]') {
    throw 'AimAid::Resolve does not preserve original bytes across same-process reconnects.'
}

$prepareStart = $implementation.IndexOf('bool AimAid::PrepareForClose()')
$prepareEnd = $implementation.IndexOf('void AimAid::OnDMAFrame()', $prepareStart)
if ($prepareStart -lt 0 -or $prepareEnd -le $prepareStart) {
    throw 'AimAid::PrepareForClose implementation could not be inspected.'
}
$prepareBody = $implementation.Substring($prepareStart, $prepareEnd - $prepareStart)
if ($prepareBody -notmatch 'RestoreAll\(\)' -or $prepareBody -notmatch 'ClearScanState\(\)') {
    throw 'AimAid::PrepareForClose must restore patches and clear only scan state.'
}

$resetStart = $implementation.IndexOf('void AimAid::Reset()')
if ($resetStart -lt 0) {
    throw 'AimAid::Reset implementation could not be inspected.'
}
$resetBody = $implementation.Substring($resetStart)
if ($resetBody -notmatch 'ClearScanState\(\)' -or
    $resetBody -notmatch 'g_haveOriginal' -or
    $resetBody -notmatch 'g_originalSite' -or
    $resetBody -notmatch 'g_hasProcessIdentity') {
    throw 'AimAid::Reset must fully clear scan and original-byte state.'
}

$closeStart = $dma.IndexOf('bool DMA::Close()')
$closeEnd = $dma.IndexOf('bool DMA::UpdateVehicleInformation()', $closeStart)
if ($closeStart -lt 0 -or $closeEnd -le $closeStart) {
    throw 'DMA::Close implementation could not be inspected.'
}
$closeBody = $dma.Substring($closeStart, $closeEnd - $closeStart)
$prepareIndex = $closeBody.IndexOf('AimAid::PrepareForClose()')
$memoryResetIndex = $closeBody.IndexOf('Memory().Reset()')
if ($prepareIndex -lt 0 -or $memoryResetIndex -lt 0 -or $prepareIndex -gt $memoryResetIndex) {
    throw 'DMA::Close must restore AimAid patches before resetting the memory backend.'
}

Write-Host 'AimAid lifecycle contract passed.'
