$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $root 'GTA5_DMA\GTA5_DMA'

function Read-Source([string]$relativePath) {
    return Get-Content -LiteralPath (Join-Path $sourceRoot $relativePath) -Raw -Encoding UTF8
}

function Require([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "RuntimeTables contract failed: $message"
    }
}

$header = Read-Source 'Core\RuntimeTables.h'
$impl = Read-Source 'Core\RuntimeTables.cpp'
$dma = Read-Source 'Core\DMA.cpp'
$tunables = Read-Source 'Features\Tunables.cpp'
$resolver = Read-Source 'Core\OffsetResolver.cpp'
$resolverHeader = Read-Source 'Core\OffsetResolver.h'
$table = Read-Source 'Features\TunableTable.h'
$generator = Get-Content -LiteralPath (Join-Path $sourceRoot 'tools\gen_tunables.py') -Raw -Encoding UTF8
$readme = Get-Content -LiteralPath (Join-Path $root 'README.md') -Raw -Encoding UTF8
$gitignore = Get-Content -LiteralPath (Join-Path $root '.gitignore') -Raw -Encoding UTF8

# 1) 三条免重编译通道必须都在
Require ($header -match 'ResolveTunableIndex') 'tunable index resolution is missing'
Require ($header -match 'FindValueRunGlobalIndex') 'value-run self-discovery is missing'
Require ($header -match 'GetPatternOverrides') 'external pattern overrides are missing'
Require ($header -match '外部覆盖文件') 'the resolution priority chain must document the override file first'
Require ($impl -match 'GTA5_DMA_tunables\.txt') 'the tunable override file name is missing'
Require ($impl -match 'GTA5_DMA_patterns\.txt') 'the pattern override file name is missing'
Require ($impl -match 'GetModuleFileNameA') 'runtime files must live next to the exe'
Require ($impl -match 'tunables\.bin') 'the tunables.bin cache path is missing'

# 2) 覆盖文件必须支持自带 rel32 布局（主/备特征是不同指令，沿用主模式会算错地址）
Require ($impl -match 'hasLayout') 'pattern overrides must accept their own displacement/instruction size'
Require ($impl -match 'disp') 'pattern override layout keyword disp is missing'
Require ($impl -match 'insn') 'pattern override layout keyword insn is missing'
Require ($resolver -match 'g_externalProvider') 'the offset resolver does not consult the external pattern provider'
Require ($impl -match 'SetExternalPatternProvider') 'RuntimeTables never registers the external pattern provider'
Require ($resolverHeader -match 'ExternalPatternProvider') 'OffsetResolver has no provider type'

# 3) 初始化顺序：运行时表必须在解析偏移/tunable 之前
$initIndex = $dma.IndexOf('bool DMA::Initialize()')
$runtimeIndex = $dma.IndexOf('RuntimeTables::Initialize()')
$resolveIndex = $dma.IndexOf('ResolveRuntimeOffsets()')
Require ($initIndex -ge 0 -and $runtimeIndex -gt $initIndex) 'RuntimeTables::Initialize is not called from DMA::Initialize'
Require ($resolveIndex -gt $runtimeIndex) 'runtime tables must be loaded BEFORE offsets are resolved'
Require ($dma -match 'RuntimeTables::Shutdown\(\)') 'RuntimeTables is never shut down'

# 4) tunable 解析链 + 值序列自发现 + 唯一性判据
Require ($tunables -match 'IndexOfEntry') 'Tunables::Resolve does not consult the runtime index'
Require ($tunables -match 'ResolveTunableIndex') 'Tunables::Resolve does not use RuntimeTables'
Require ($tunables -match '值序列自发现') 'value-run self-discovery is not wired into Tunables::Resolve'
Require ($tunables -match 'occurrences == 1') 'self-discovery must require a unique occurrence'
Require ($tunables -match 'MatchesExpectedBits\(entry, retryBits\)') 'self-discovered candidates must pass the medical check again'

# 5) 生成器与表：值锚组 + 每条条目的分组信息
Require ($generator -match 'GROUPS') 'the generator has no value-run groups'
Require ($generator -match 'RUN_LOOKUP') 'the generator does not map entries to run groups'
Require ($table -match 'struct ValueRun') 'the generated table has no ValueRun type'
Require ($table -match 'kRuns\[\]') 'the generated table has no run table'
Require ($table -match 'kRunHeistPrimaryValue') 'the heist value run is missing'
Require ($table -match 'kRunIdleKick') 'the idle-kick run is missing'
Require ($table -match 'kRunAppearance') 'the appearance run is missing'
Require ($table -match 'kRunSkydiveReward') 'the skydive reward run is missing'
Require ($table -match 'kRunConstrainedKick') 'the constrained-kick run is missing'

# 6) 文档与忽略清单
Require ($readme -match '长期可用') 'README does not explain the long-lived design'
Require ($readme -match 'GTA5_DMA_tunables\.txt') 'README does not document the override file'
Require ($gitignore -match 'GTA5_DMA_tunables\.txt') 'runtime seed files must be gitignored'

Write-Output 'RuntimeTables contract passed.'
