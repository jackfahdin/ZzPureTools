$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Native([string]$File, [string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$File failed with exit code $LASTEXITCODE"
    }
}

function Resolve-Required([string]$Path) {
    return (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
}

function Normalize-WindowsPath([string]$Path) {
    $resolved = Resolve-Required $Path
    return [IO.Path]::GetFullPath($resolved).TrimEnd([char[]]@('\', '/'))
}

function Assert-ConfiguredPreset(
    [string]$Preset,
    [string]$ExpectedId,
    [string]$ExpectedCompiler,
    [string]$ExpectedQt) {
    $buildDir = Resolve-Required (Join-Path $sourceDir "build/$Preset")
    $compilerFiles = @(Get-ChildItem `
        -LiteralPath (Join-Path $buildDir 'CMakeFiles') `
        -Recurse -Filter CMakeCXXCompiler.cmake -File)
    if ($compilerFiles.Count -ne 1) {
        throw "Expected one CMakeCXXCompiler.cmake for $Preset"
    }
    $compilerState = Get-Content -LiteralPath $compilerFiles[0].FullName -Raw
    $compilerIdPattern = 'set\(CMAKE_CXX_COMPILER_ID "{0}"\)' -f `
        [regex]::Escape($ExpectedId)
    if ($compilerState -notmatch $compilerIdPattern) {
        throw "Unexpected compiler id for $Preset"
    }
    $cache = Get-Content -LiteralPath (Join-Path $buildDir 'CMakeCache.txt') -Raw
    $compilerPath = (Normalize-WindowsPath $ExpectedCompiler).Replace('\', '/')
    $qtPath = (Normalize-WindowsPath $ExpectedQt).Replace('\', '/')
    $normalizedCompilerState = $compilerState.Replace('\', '/')
    $normalizedCache = $cache.Replace('\', '/')
    $qtMatches =
        $normalizedCache.Contains(
            "ZZ_QT_PREFIX:UNINITIALIZED=$qtPath",
            [StringComparison]::OrdinalIgnoreCase) -or
        $normalizedCache.Contains(
            "ZZ_QT_PREFIX:PATH=$qtPath",
            [StringComparison]::OrdinalIgnoreCase) -or
        $normalizedCache.Contains(
            "ZZ_QT_PREFIX:STRING=$qtPath",
            [StringComparison]::OrdinalIgnoreCase)
    if (-not $normalizedCompilerState.Contains(
            $compilerPath, [StringComparison]::OrdinalIgnoreCase) -or
        -not $qtMatches) {
        throw "Compiler or Qt prefix mismatch for $Preset"
    }
}

$sourceDir = Resolve-Required (Join-Path $PSScriptRoot '../..')
Set-Location $sourceDir
$msvcQt = Normalize-WindowsPath $env:QT_MSVC_ROOT
$msvcQmake = Resolve-Required (Join-Path $msvcQt 'bin/qmake.exe')
$msvcPrefixRaw = (& $msvcQmake -query QT_INSTALL_PREFIX).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to query QT_INSTALL_PREFIX from QT_MSVC_ROOT'
}
$msvcPrefix = Normalize-WindowsPath $msvcPrefixRaw
$msvcXspec = (& $msvcQmake -query QMAKE_XSPEC).Trim()
if ($LASTEXITCODE -ne 0 -or
    -not $msvcPrefix.Equals(
        $msvcQt, [StringComparison]::OrdinalIgnoreCase) -or
    $msvcXspec -notmatch '^win32-msvc') {
    throw 'QT_MSVC_ROOT is not an MSVC Qt kit'
}

$dumpbin = (Get-Command dumpbin.exe -ErrorAction Stop).Source
$env:ZZ_DUMPBIN = Resolve-Required $dumpbin
$mingwKit = pwsh -NoProfile -File scripts/ci/Assert-QtMinGWKit.ps1 |
    ConvertFrom-Json
$env:ZZ_MINGW_OBJDUMP = Resolve-Required $mingwKit.ObjDump
$mingwQt = Normalize-WindowsPath $mingwKit.QtPrefix
if ($mingwQt.Equals($msvcQt, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'MSVC and MinGW must use separate Qt prefixes'
}

$presets = @(
    'windows-msvc2022-release',
    'windows-msvc2022-static',
    'windows-mingw-release',
    'windows-mingw-static'
)
foreach ($preset in $presets) {
    Invoke-Native cmake @('--preset', $preset, '-DZZ_BUILD_EXAMPLES=ON')
    if ($preset -like 'windows-msvc*') {
        Assert-ConfiguredPreset $preset 'MSVC' `
            (Get-Command cl.exe -ErrorAction Stop).Source $msvcQt
    } else {
        Assert-ConfiguredPreset $preset 'GNU' $mingwKit.Compiler $mingwQt
    }
    Invoke-Native cmake @('--build', '--preset', $preset)
    Invoke-Native ctest @('--preset', $preset, '--output-on-failure')
}
