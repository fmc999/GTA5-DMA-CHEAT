$ErrorActionPreference = 'Stop'

$shell = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/UI/ConsoleShell.cpp'
$menu = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/UI/MenuManager.cpp'
$invisibility = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/Features/Invisibility.cpp'

if ($shell -notmatch 'GodMode::bPlayerGodMode\.load\(\)' -or
    $shell -notmatch 'GodMode::bPlayerGodMode\.store\(playerGod\)') {
    throw 'Quick player god mode is not bound to the persistent state.'
}

if ($shell -match 'bool playerGod = GodMode::bRequestedGodmode\.load\(\)') {
    throw 'Quick player god mode still reads the one-shot request state.'
}

if ($shell -notmatch 'Invisibility::bInvisibility\.load\(\)' -or
    $shell -notmatch 'Invisibility::bInvisibility\.store\(invisible\)') {
    throw 'Quick invisibility is not bound to the persistent state.'
}

if ($menu -notmatch 'Invisibility::bInvisibility\.load\(\)' -or
    $menu -notmatch 'Invisibility::bInvisibility\.store\(invisible\)') {
    throw 'Player-page invisibility is not bound to the persistent state.'
}

if ($invisibility -notmatch 'DMA::LocalPlayerAddress\s*\+\s*offsetof\(PED, InvisibilityFlag\)') {
    throw 'Invisibility does not use the validated local PED address.'
}

if ($invisibility -notmatch '0x27') {
    throw 'Invisibility normal value was not restored to 0x27.'
}

if ($shell -match 'TextUnformatted\("GTA5 DMA"\)' -or $shell -match 'TextDisabled\("/') {
    throw 'The duplicate console heading is still rendered.'
}

if ($shell -notmatch 'bilibili') {
    throw 'The free-release notice is missing from the console title.'
}

Write-Host 'Quick controls/invisibility contract passed.'
