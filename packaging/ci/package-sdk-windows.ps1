# =============================================================================
# 打包 Windows 完整 SDK
# =============================================================================
param(
    [Parameter(Mandatory = $true)][string]$PlatformLabel,
    [Parameter(Mandatory = $true)][string]$ArchLabel,
    [Parameter(Mandatory = $true)][string]$Toolchain,
    [Parameter(Mandatory = $true)][string]$BuildType,
    [Parameter(Mandatory = $true)][string]$QtVersion,
    [Parameter(Mandatory = $true)][string]$Shared,
    [Parameter(Mandatory = $true)][string]$Cpu
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "Resolve-ProjectConfig.ps1")

$workspace = $env:GITHUB_WORKSPACE
if (-not $workspace) { $workspace = (Get-Location).Path }

$installRoot = Join-Path $workspace "install"
$distDir = Join-Path $workspace "dist"
$sdkZip = Join-Path $distDir "$ProjectName-sdk.zip"

if (-not (Test-Path $installRoot)) {
    throw "未找到 SDK 安装目录: $installRoot"
}

Copy-Item (Join-Path $workspace "LICENSE") $installRoot -Force
Copy-Item (Join-Path $workspace "packaging/sdk/SDK使用说明.txt") $installRoot -Force
Copy-Item (Join-Path $workspace "packaging/sdk/examples") $installRoot -Recurse -Force

if ($BuildType -eq "Debug") {
    Get-ChildItem -Path (Join-Path $workspace "build") -Filter "$ProjectName.pdb" -Recurse |
        Select-Object -First 1 |
        ForEach-Object {
            $libDir = Join-Path $installRoot "lib"
            New-Item -ItemType Directory -Force -Path $libDir | Out-Null
            Copy-Item $_.FullName $libDir -Force
        }
}

$libType = if ($Shared -eq "ON") { "动态库 (SHARED)" } else { "静态库 (STATIC)" }
$buildInfo = @"
$ProjectName SDK 构建信息
====================
版本: $ProjectVersion
平台: $PlatformLabel
CPU 架构: $Cpu ($ArchLabel)
工具链: $Toolchain
构建类型: $BuildType
库类型: $libType
Qt 版本: $QtVersion
构建时间(UTC): $((Get-Date).ToUniversalTime().ToString('yyyy-MM-dd HH:mm:ss'))
GitHub Run: $($env:GITHUB_RUN_ID)

目录说明:
  include/   公开头文件
  lib/       导入库与 CMake 包配置
  bin/       运行时 .dll (动态库构建时)
  examples/  最小集成示例

注意: SDK 不包含 Qt，使用者需自行安装同版本 Qt。
"@
$buildInfo | Out-File -FilePath (Join-Path $installRoot "BUILD_INFO.txt") -Encoding utf8

New-Item -ItemType Directory -Force -Path $distDir | Out-Null
if (Test-Path $sdkZip) { Remove-Item $sdkZip -Force }
Compress-Archive -Path (Join-Path $installRoot "*") -DestinationPath $sdkZip -Force
Write-Host "已生成 SDK: $sdkZip"
