#!/usr/bin/env pwsh
<#
.SYNOPSIS
    通用 CMake 项目一键构建脚本。
    自动从 CMakeLists.txt 识别项目名，支持 Qt/非 Qt 项目。

.DESCRIPTION
    支持 MinGW / MSVC，自动定位 CMake/Ninja。
    可选 windeployqt 打包（Qt 项目）、cmake --install、子目录项目。

.EXAMPLE
    ./build.ps1
    ./build.ps1 -Type Debug
    ./build.ps1 -Install
    ./build.ps1 -SubDir libs/mylib
    ./build.ps1 -Target MyApp -Deploy
    ./build.ps1 -Compiler msvc -Qt 'D:/SoftWare/Qt/6.11.0/msvc2022_64'
#>
param (
    [Alias("Qt")]       [string]$QtDir       = "",
    [string]$VsDir       = "",
    [string]$MinGbDir    = "",
    [string]$CmakeDir    = "",
    [string]$ProjectDir  = "",
    [string]$SubDir      = "",
    [string]$Type        = "Release",
    [string]$Compiler    = "auto",
    [string]$Target      = "",
    [string]$DeployDir   = "",
    [string]$InstallDir  = "",
    [switch]$Install,
    [switch]$Deploy,
    [switch]$DeployOnly,
    [switch]$Clean,
    [switch]$ExportCompileCommands,
    [switch]$Help
)

# ── 帮助 ──────────────────────────────────────────────────────────
if ($Help) {
    $me = [IO.Path]::GetFileName($PSCommandPath)
    Write-Host @"

  通用 CMake 构建脚本

  用法:  ./$me [选项]

  选项:
    -QtDir, -Qt <path>      Qt 安装路径（不指定则尝试自动检测）
    -VsDir <path>            Visual Studio 安装路径（默认自动检测）
    -MinGbDir <path>         MinGW 工具链路径（默认自动从 Qt Tools 找）
    -CmakeDir <path>         CMake bin 目录（默认从 Qt Tools 或 PATH 找）
    -ProjectDir <path>       项目根目录（默认脚本所在目录）
    -SubDir <name>           子目录项目（等价于 -ProjectDir <当前>/<name>）
    -Type <type>             构建类型: Debug / Release（默认 Release）
    -Compiler <type>         编译器: mingw / msvc / auto（默认 auto）
    -Target <name>           只编译指定目标（逗号分隔多个）
    -Install                 编译后执行 cmake --install
    -InstallDir <path>       安装目标路径（默认项目根目录/install）
    -Deploy                  编译后用 windeployqt 打包（Qt 项目）
    -DeployOnly              跳过编译，仅打包已有产物
    -DeployDir <path>        打包输出目录（默认 dist/<目标>_<Type>）
    -Clean                   清理 build 目录后重新构建
    -ExportCompileCommands   生成 compile_commands.json（供 clang-tidy 使用）

  示例:
    ./$me
    ./$me -Type Debug
    ./$me -Install
    ./$me -SubDir libs/mylib
    ./$me -Target MyApp -Deploy
    ./$me -Compiler msvc -Qt 'D:/SoftWare/Qt/6.11.0/msvc2022_64'
    ./$me -Compiler msvc -VsDir 'D:/SoftWare/Microsoft Visual Studio/2022/Community'
"@
    Exit 0
}

# ── 路径规范化 ────────────────────────────────────────────────────
if ([string]::IsNullOrEmpty($ProjectDir)) { $ProjectDir = $PSScriptRoot }

if (-not [string]::IsNullOrWhiteSpace($SubDir)) {
    $resolved = Join-Path $PSScriptRoot $SubDir
    if (-not (Test-Path $resolved)) {
        Write-Error "[错误] 子目录不存在: $resolved"
        Exit 1
    }
    $ProjectDir = $resolved
}

$ProjectDir = (Resolve-Path $ProjectDir).Path

# 自动识别项目名
$CmakeFile = Join-Path $ProjectDir "CMakeLists.txt"
$ProjectName = ""
if (Test-Path $CmakeFile) {
    $content = Get-Content $CmakeFile -Raw
    if ($content -match '^\s*project\s*\(\s*(\S+)') {
        $ProjectName = $Matches[1]
    }
}
if ([string]::IsNullOrEmpty($ProjectName)) {
    $ProjectName = Split-Path $ProjectDir -Leaf
}

foreach ($v in @("QtDir","VsDir","MinGbDir","CmakeDir","DeployDir","InstallDir")) {
    Set-Variable $v ((Get-Variable $v).Value -replace '\\', '/')
}

# ── 工具函数 ──────────────────────────────────────────────────────
function Add-ToPath { param([string[]]$D) ; $D | ?{ $_ -and (Test-Path $_) } | %{ $env:PATH = "$_;$env:PATH" } }
function Require-Cmd { param([string]$N,[string]$H) ; if (!(Get-Command $N -ea 0)) { Write-Error "[错误] 未找到 $N。$H"; Exit 1 } }
function Copy-IfExists { param($S,$F,$D) ; $p = Join-Path $S $F; if (Test-Path $p) { Copy-Item $p $D -Force; $true } else { $false } }

# ── 编译器检测 ────────────────────────────────────────────────────
$SelectedCompiler = $Compiler.ToLower()
if ($SelectedCompiler -eq "auto") {
    $SelectedCompiler = if ($QtDir -match "msvc") { "msvc" } else { "mingw" }
}

# ── MSVC 环境载入 ────────────────────────────────────────────────
if (!$DeployOnly -and $SelectedCompiler -eq "msvc") {
    Write-Host "[状态] 检测 MSVC 环境..."

    $devCmd = $null

    # 1. 如果用户指定了 VsDir，优先使用
    if ($VsDir) {
        $candidate = "$VsDir/Common7/Tools/VsDevCmd.bat"
        if (Test-Path $candidate) { $devCmd = $candidate }
        else {
            # 兼容只给到版本目录的情况，如 D:/.../2022/Community
            $candidate = "$VsDir/../Common7/Tools/VsDevCmd.bat" | Resolve-Path -ea 0 | Select-Object -ExpandProperty Path
            if ($candidate -and (Test-Path $candidate)) { $devCmd = $candidate }
        }
    }

    # 2. 尝试用 vswhere 自动定位
    if (!$devCmd) {
        $vswhereLocations = @(
            "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe",
            "${env:ProgramFiles}/Microsoft Visual Studio/Installer/vswhere.exe",
            "D:/SoftWare/Microsoft Visual Studio/Installer/vswhere.exe"
        )
        $vswhere = $vswhereLocations | Where-Object { Test-Path $_ } | Select-Object -First 1
        if ($vswhere) {
            $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if ($vsPath) {
                $candidate = "$vsPath/Common7/Tools/VsDevCmd.bat"
                if (Test-Path $candidate) { $devCmd = $candidate }
            }
        }
    }

    # 3. 最后扫描常见自定义安装路径
    if (!$devCmd) {
        $customRoots = @(
            "D:/SoftWare/Microsoft Visual Studio/2022/Community",
            "D:/SoftWare/Microsoft Visual Studio/2022/Professional",
            "D:/SoftWare/Microsoft Visual Studio/2022/Enterprise",
            "D:/SoftWare/Microsoft Visual Studio/2019/Community",
            "D:/SoftWare/Microsoft Visual Studio/2019/Professional",
            "D:/SoftWare/Microsoft Visual Studio/2019/Enterprise"
        )
        foreach ($root in $customRoots) {
            $candidate = "$root/Common7/Tools/VsDevCmd.bat"
            if (Test-Path $candidate) { $devCmd = $candidate; break }
        }
    }

    if (!$devCmd) {
        Write-Error "[错误] 未找到 VsDevCmd.bat。请安装 Visual Studio C++ 开发组件，或用 -VsDir 指定路径。"
        Exit 1
    }

    # 载入 MSVC 环境变量到当前进程
    cmd /c "`"$devCmd`" -arch=amd64 && set" | ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$' -and $Matches[1] -notin @("Prompt","WINEVENT_IPC_NAME")) {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
    Write-Host "[成功] MSVC 环境已载入 (x64): $devCmd"
    Require-Cmd cl "请安装 Visual Studio C++ 开发组件。"
}

# ── Qt 自动检测 ──────────────────────────────────────────────────
if ([string]::IsNullOrEmpty($QtDir) -and !$DeployOnly) {
    $qmake = Get-Command qmake -ea 0
    if ($qmake) {
        $QtDir = Split-Path -Parent (Split-Path -Parent $qmake.Source)
    }
    if (!$QtDir) {
        # 扫描常见 Qt 安装位置，根据编译器偏好选择 kit
        $qtRoots = @("D:/SoftWare/Qt", "C:/Qt")
        $candidates = foreach ($r in $qtRoots) {
            if (Test-Path $r) { Get-ChildItem $r -Directory -ea 0 | Sort-Object Name -Descending }
        }

        # MSVC 优先查找 msvc2022_64 / msvc2019_64；MinGW 优先查找 mingw_64
        $kitNames = if ($SelectedCompiler -eq "msvc") {
            @("msvc2022_64", "msvc2022_arm64", "msvc2019_64", "msvc2019_arm64")
        } else {
            @("mingw_64")
        }

        foreach ($ver in $candidates) {
            foreach ($kitName in $kitNames) {
                $kit = "$($ver.FullName)/$kitName"
                if (Test-Path "$kit/bin/qmake.exe") { $QtDir = $kit; break }
            }
            if ($QtDir) { break }
        }
    }
    if ($QtDir) { Write-Host "[信息] 检测到 Qt: $QtDir" }
}

if ($QtDir -and !(Test-Path "$QtDir/bin")) {
    Write-Warning "[警告] Qt bin 不存在: $QtDir/bin，跳过 Qt 相关功能。"
    $QtDir = ""
}

# ── CMake 定位 ────────────────────────────────────────────────────
if ([string]::IsNullOrEmpty($CmakeDir)) {
    # 先从 Qt Tools 找
    $QtToolsRoot = if ($MinGbDir) { Split-Path $MinGbDir -Parent }
                   elseif ($QtDir)  { Split-Path $QtDir -Parent | Split-Path -Parent | Join-Path -ChildPath "Tools" }
    if ($QtToolsRoot -and (Test-Path "$QtToolsRoot/CMake_64/bin/cmake.exe")) {
        $CmakeDir = "$QtToolsRoot/CMake_64/bin"
    }
    # 再从 PATH 找
    if (!$CmakeDir) {
        $cmakeCmd = Get-Command cmake -ea 0
        if ($cmakeCmd) { $CmakeDir = Split-Path -Parent $cmakeCmd.Source }
    }
}
if ($CmakeDir) { Add-ToPath @($CmakeDir) }
if ($QtDir)     { Add-ToPath @("$QtDir/bin") }

# ── MinGW 自动检测 ────────────────────────────────────────────────
if ($SelectedCompiler -eq "mingw") {
    if ([string]::IsNullOrEmpty($MinGbDir)) {
        $QtToolsDir = Split-Path $QtDir -Parent | Split-Path -Parent | Join-Path -ChildPath "Tools"
        $mingwDirs = Get-ChildItem "$QtToolsDir/mingw*_64" -Directory -ea 0 | Sort-Object Name -Descending
        if ($mingwDirs) { $MinGbDir = $mingwDirs[0].FullName }
    }
    if ($MinGbDir -and (Test-Path "$MinGbDir/bin")) {
        Add-ToPath @("$MinGbDir/bin")
    }
    if ($QtDir) { Add-ToPath @("$QtDir/bin") }
}

# Ninja：MinGW 从 Tools/Ninja 找；MSVC 也可以从 Qt 安装目录的 Tools/Ninja 找
$QtToolsRoot2 = $null
if ($MinGbDir) {
    $QtToolsRoot2 = Split-Path $MinGbDir -Parent
}
elseif ($QtDir) {
    # Qt 安装目录通常是 D:/SoftWare/Qt/6.x.x/msvc2022_64，向上两级到 D:/SoftWare/Qt，再进 Tools
    $QtToolsRoot2 = Split-Path $QtDir -Parent | Split-Path -Parent | Join-Path -ChildPath "Tools"
}
if ($QtToolsRoot2) { Add-ToPath @("$QtToolsRoot2/Ninja") }
$HasNinja = [bool](Get-Command ninja -ea 0)

if ($HasNinja)                { $Generator = "Ninja" }
elseif ($SelectedCompiler -eq "msvc") { $Generator = "Visual Studio 17 2022" }
else                          { $Generator = "MinGW Makefiles" }

$IsMultiConfig = $Generator -match '^Visual Studio'

if (!$DeployOnly) {
    Require-Cmd cmake "请安装 CMake 或用 -CmakeDir 指定路径。"
    if ($Generator -eq "Ninja") { Require-Cmd ninja "请安装 Ninja。" }
}

# ── 构建 ──────────────────────────────────────────────────────────
Set-Location $ProjectDir
$BuildDir = "$ProjectDir/build"

# ── 清理 ──────────────────────────────────────────────────────────
if ($Clean) {
    if (Test-Path $BuildDir) {
        Write-Host "[状态] 清理 build 目录..."
        Remove-Item -Recurse -Force $BuildDir
    }
}

if (!$DeployOnly) {
    Write-Host ("=" * 60)
    Write-Host "  项目: $ProjectName  |  编译器: $SelectedCompiler  |  $Generator  |  $Type"
    Write-Host ("=" * 60)
    if ($Target) { Write-Host "[目标] $Target" }

    # CMake 配置
    Write-Host "`n[1/3] 配置 CMake..."
    $cmakeArgs = @("-S", $ProjectDir, "-B", $BuildDir)
    if ($QtDir) { $cmakeArgs += "-DCMAKE_PREFIX_PATH=$QtDir" }
    if (!$IsMultiConfig) { $cmakeArgs += "-DCMAKE_BUILD_TYPE=$Type" }
    if ($ExportCompileCommands) { $cmakeArgs += "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" }
    if ($Generator) { $cmakeArgs = @("-G", $Generator) + $cmakeArgs }

    & cmake @cmakeArgs 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { Write-Error "[错误] CMake 配置失败。"; Exit 1 }

    # 编译
    Write-Host "`n[2/3] 编译..."
    $buildArgs = @("--build", $BuildDir, "--parallel")
    if ($IsMultiConfig) { $buildArgs += @("--config", $Type) }

    if ($Target) {
        foreach ($t in ($Target -split ',' | %{ $_.Trim() } | ?{ $_ })) {
            Write-Host "  → 目标: $t"
            & cmake @buildArgs --target $t 2>&1 | ForEach-Object { Write-Host $_ }
            if ($LASTEXITCODE -ne 0) { Write-Error "[错误] 编译 '$t' 失败。"; Exit 1 }
        }
    }
    else {
        & cmake @buildArgs 2>&1 | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0) { Write-Error "[错误] 编译失败。"; Exit 1 }
    }
    Write-Host "`n[完成] 构建产物: $BuildDir"

    # 安装
    if ($Install) {
        Write-Host "`n[3/3] 安装..."
        if ([string]::IsNullOrEmpty($InstallDir)) { $InstallDir = "$ProjectDir/install" }
        & cmake --install $BuildDir --prefix $InstallDir 2>&1 | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0) { Write-Error "[错误] 安装失败。"; Exit 1 }
        Write-Host "[完成] 安装到: $InstallDir"
    }
}

# ── windeployqt 打包 ──────────────────────────────────────────────
if ($Deploy -or $DeployOnly) {
    if (!$QtDir) {
        Write-Error "[错误] -Deploy 需要 Qt，但未找到 Qt 安装路径。"
        Exit 1
    }

    Write-Host "`n" + ("=" * 60)
    Write-Host "  windeployqt 打包"
    Write-Host ("=" * 60)

    $windeployqt = Join-Path $QtDir "bin/windeployqt.exe"
    if (!(Test-Path $windeployqt)) {
        Write-Error "[错误] 未找到 windeployqt: $windeployqt"
        Exit 1
    }

    # 递归查找可执行文件（Ninja 会把产物放在子目录中）
    $preferredName = if ($Target) { ($Target -split ',')[0].Trim() } else { "" }
    $exe = $null
    $allExes = Get-ChildItem $BuildDir -Filter *.exe -File -Recurse -ea 0 |
        Where-Object { $_.DirectoryName -notmatch 'CMakeFiles|_autogen|CompilerId' }

    if ($preferredName) {
        $exe = $allExes | Where-Object { $_.Name -eq "$preferredName.exe" } | Select-Object -First 1
    }
    if (!$exe) {
        $exe = $allExes | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    }
    if (!$exe) { Write-Error "[错误] 未找到 .exe 文件。"; Exit 1 }

    $pkgName = [IO.Path]::GetFileNameWithoutExtension($exe.Name)
    if ([string]::IsNullOrWhiteSpace($DeployDir)) {
        $DeployDir = "$ProjectDir/dist/${pkgName}_$Type"
    }
    if (Test-Path $DeployDir) { Remove-Item $DeployDir -Recurse -Force }
    New-Item -ItemType Directory $DeployDir -Force | Out-Null

    $targetExe = Join-Path $DeployDir $exe.Name
    Copy-Item $exe.FullName $targetExe -Force
    Write-Host "[状态] 已复制: $($exe.Name)"

    Write-Host "[状态] 收集 Qt 运行库..."
    & $windeployqt --$($Type.ToLower()) --compiler-runtime --no-translations `
        --no-system-d3d-compiler --no-opengl-sw $targetExe
    if (!$?) { Write-Error "[错误] windeployqt 失败。"; Exit 1 }

    # MinGW 运行时 DLL
    if ($SelectedCompiler -eq "mingw" -and $MinGbDir) {
        Write-Host "[状态] 复制 MinGW 运行时..."
        @("libgcc_s_seh-1.dll","libgcc_s_dw2-1.dll","libstdc++-6.dll","libwinpthread-1.dll") |
            ForEach-Object { if (Copy-IfExists "$MinGbDir/bin" $_ $DeployDir) { Write-Host "  + $_" } }
    }

    Write-Host "[完成] 打包目录: $DeployDir"
}
