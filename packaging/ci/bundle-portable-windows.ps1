# =============================================================================
# 打包 Windows 可运行测试包
# =============================================================================
param(
    [Parameter(Mandatory = $true)][string]$BuildType,
    [Parameter(Mandatory = $true)][string]$PlatformLabel,
    [Parameter(Mandatory = $true)][string]$ArchLabel,
    [Parameter(Mandatory = $true)][string]$Toolchain,
    [Parameter(Mandatory = $true)][string]$QtVersion
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "Resolve-ProjectConfig.ps1")

$workspace = $env:GITHUB_WORKSPACE
if (-not $workspace) { $workspace = (Get-Location).Path }

$exampleDir = Join-Path $workspace "build/example"
$exe = Join-Path $exampleDir "$ExampleName.exe"
$distDir = Join-Path $workspace "dist"
$portableZip = Join-Path $distDir "$ProjectName-portable.zip"

if (-not (Test-Path $exe)) {
    throw "未找到示例程序: $exe"
}

Get-ChildItem -Path (Join-Path $workspace "build") -Filter "$ProjectName.dll" -Recurse |
    Select-Object -First 1 |
    ForEach-Object { Copy-Item $_.FullName $exampleDir -Force }

$windeployqt = Join-Path $env:QT_ROOT_DIR "bin/windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "未找到 windeployqt: $windeployqt"
}

$deployFlag = if ($BuildType -eq "Debug") { "--debug" } else { "--release" }
$deployArgs = @($deployFlag, $exe)
if ($Toolchain -eq "msvc") {
    $deployArgs = @($deployFlag, "--compiler-runtime", $exe)
}
Write-Host "运行: $windeployqt $($deployArgs -join ' ')"
& $windeployqt @deployArgs

@"
$ProjectName 可运行示例包 (Windows)
==============================
双击 $ExampleName.exe 即可运行。

本目录已包含：
  - $ExampleName.exe
  - $ProjectName.dll
  - Qt 6 运行时与 plugins
  - MSVC 运行库（MSVC 构建时）

平台: $PlatformLabel
架构: $ArchLabel
工具链: $Toolchain
构建类型: $BuildType
Qt 版本: $QtVersion
"@ | Out-File -FilePath (Join-Path $exampleDir "使用说明.txt") -Encoding utf8

New-Item -ItemType Directory -Force -Path $distDir | Out-Null
if (Test-Path $portableZip) { Remove-Item $portableZip -Force }
Compress-Archive -Path (Join-Path $exampleDir "*") -DestinationPath $portableZip -Force
Write-Host "已生成: $portableZip"
