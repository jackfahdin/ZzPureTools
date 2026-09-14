[CmdletBinding(DefaultParameterSetName = 'Build')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Build')]
    [ValidateSet('windows-msvc2022-release', 'windows-mingw-release')]
    [string]$Preset,

    [Parameter(Mandatory, ParameterSetName = 'Build')]
    [ValidateNotNullOrEmpty()]
    [string]$Tests,

    [Parameter(Mandatory, ParameterSetName = 'Docs')]
    [switch]$DocsOnly
)

$ErrorActionPreference = 'Stop'
$excludedLabels = 'benchmark|screenshot|install|packaging|release'
$sourceDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Assert-LastExitCode {
    param([Parameter(Mandatory)][string]$Operation)

    if ($LASTEXITCODE -ne 0) {
        throw "本机开发门禁失败：$Operation，退出码 $LASTEXITCODE"
    }
}

function Get-CacheValue {
    param(
        [Parameter(Mandatory)][string]$CachePath,
        [Parameter(Mandatory)][string]$Name
    )

    $line = Select-String -LiteralPath $CachePath -Pattern "^${Name}:[^=]*=(.*)$" |
        Select-Object -First 1
    if ($null -eq $line) {
        return '未知'
    }
    return $line.Matches[0].Groups[1].Value
}

git -C $sourceDir diff --check
Assert-LastExitCode 'git diff --check'
git -C $sourceDir diff --cached --check
Assert-LastExitCode 'git diff --cached --check'

if ($DocsOnly) {
    cmake "-DZZ_SOURCE_DIR=$sourceDir" `
        -P "$sourceDir/tests/Architecture/ZzDocumentationAudit.cmake"
    Assert-LastExitCode '文档审计'
    Write-Host '本机最小门禁已通过：纯文档模式。'
    Write-Host '待验证：不适用；本模式未修改生产代码。'
    exit 0
}

if (-not $IsWindows) {
    throw '本机开发门禁失败：PowerShell 构建模式只支持 Windows。'
}

Push-Location $sourceDir
try {
    cmake --preset $Preset `
        -DZZ_BUILD_TESTS=ON `
        -DZZ_BUILD_EXAMPLES=ON `
        -DZZ_BUILD_BENCHMARKS=OFF `
        -DZZ_WARNINGS_AS_ERRORS=ON
    Assert-LastExitCode "配置 $Preset"

    cmake --build --preset $Preset
    Assert-LastExitCode "构建 $Preset"

    ctest --preset $Preset --output-on-failure -LE $excludedLabels
    Assert-LastExitCode "运行 $Preset 普通测试"

    $inventory = ctest --preset $Preset -N -R $Tests -LE $excludedLabels 2>&1
    $inventoryResult = $LASTEXITCODE
    $inventory | ForEach-Object { Write-Host $_ }
    if ($inventoryResult -ne 0) {
        throw "本机开发门禁失败：枚举定向测试，退出码 $inventoryResult"
    }
    $totalMatch = [regex]::Match(($inventory -join "`n"), 'Total Tests:\s+(\d+)')
    if (-not $totalMatch.Success -or [int]$totalMatch.Groups[1].Value -eq 0) {
        throw "本机开发门禁失败：定向测试正则没有匹配普通测试：$Tests"
    }

    ctest --preset $Preset --output-on-failure -R $Tests -LE $excludedLabels
    Assert-LastExitCode "运行定向测试 $Tests"

    $cachePath = Join-Path $sourceDir "build/$Preset/CMakeCache.txt"
    $compilerPath = Get-CacheValue -CachePath $cachePath -Name 'CMAKE_CXX_COMPILER'
    $qtCMakeDir = Get-CacheValue -CachePath $cachePath -Name 'Qt6_DIR'
    $cmakeVersion = cmake --version | Select-Object -First 1
    Assert-LastExitCode '读取 CMake 版本'

    Write-Host "本机最小门禁已通过：Windows/$([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture)。"
    Write-Host "CMake：$cmakeVersion"
    Write-Host "Qt：$qtCMakeDir"
    Write-Host "编译器：$compilerPath"
    Write-Host "preset：$Preset"
    Write-Host "定向测试：$Tests"
    Write-Host "排除标签：$excludedLabels"
    Write-Host '待验证：其他平台 CI、ASan/UBSan、clang-tidy、视觉、性能和真机交互。'
}
finally {
    Pop-Location
}
