$ErrorActionPreference = 'Stop'

$reclass = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/Core/Reclass.h'
$dmaHeader = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/Core/DMA.h'
$weapon = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/Features/WeaponInspector.cpp'
$vehicleHeader = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/Features/VehicleEditor.h'
$vehicle = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/Features/VehicleEditor.cpp'
$menu = Get-Content -Raw -Encoding UTF8 'GTA5_DMA/GTA5_DMA/UI/MenuManager.cpp'

foreach ($field in @('WeaponAccuracy', 'WeaponMoveAccuracy', 'WeaponLockRange', 'VehicleState', 'FreezeFlag', 'VehicleWeaponAmmo', 'ModelHash')) {
    if ($reclass -notmatch "\b$field\b") {
        throw "Missing mapped CT field: $field"
    }
}

$offsetContracts = @(
    @('PED', 'pCModelInfo', '0x20'),
    @('WeaponInfo', 'WeaponAccuracy', '0x74'),
    @('WeaponInfo', 'WeaponMoveAccuracy', '0x80'),
    @('WeaponInfo', 'WeaponLockRange', '0x288'),
    @('CVehicle', 'EntityModelHash', '0x18'),
    @('CVehicle', 'pCModelInfo', '0x20'),
    @('CVehicle', 'VehicleState', '0xD8'),
    @('CVehicle', 'FreezeFlag', '0x978'),
    @('CVehicle', 'VehicleWeaponAmmo', '0x12E4'),
    @('CModelInfo', 'ModelHash', '0x18')
)

foreach ($contract in $offsetContracts) {
    $type, $field, $offset = $contract
    $pattern = "static_assert\(offsetof\($type,\s*$field\)\s*==\s*$offset\)"
    if ($reclass -notmatch $pattern) {
        throw "Missing exact CT offset guard: $type.$field == $offset"
    }
}

foreach ($field in @('WeaponAccuracy', 'WeaponMoveAccuracy', 'WeaponLockRange')) {
    if ($weapon -notmatch "DesiredWepInfo\.$field" -or $weapon -notmatch "LocalWeaponInfo\.$field") {
        throw "Weapon field is not fully wired for read/write: $field"
    }
}

if ($dmaHeader -notmatch 'LocalPlayerModelHash' -or $menu -notmatch 'LocalPlayerModelHash') {
    throw 'Player model hash is not exposed in the player page.'
}

foreach ($name in @('currentVehicleModelHash', 'currentVehicleState', 'currentVehicleWeaponAmmo', 'SetVehicleFrozen', 'SetVehicleWeaponAmmo')) {
    if ($vehicleHeader -notmatch "\b$name\b" -or $vehicle -notmatch "\b$name\b") {
        throw "Vehicle diagnostic/control is not fully wired: $name"
    }
}

$currentAmmoPattern = 'ImGui::Text\([^;\r\n]*currentVehicleWeaponAmmo'
$targetAmmoPattern = 'ImGui::InputInt\("[^"]*##target_vehicle_weapon_ammo"[^;\r\n]*desiredVehicleWeaponAmmo'
if ($vehicle -notmatch $currentAmmoPattern -or $vehicle -notmatch $targetAmmoPattern) {
    throw 'Vehicle weapon ammo UI must distinguish the current read value from the write target.'
}

Write-Host 'CT feature parity contract passed.'
