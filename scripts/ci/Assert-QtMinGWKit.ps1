$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$required = @(
    'QT_SDK_ROOT',
    'QT_MINGW_ROOT',
    'QT_MINGW_TOOLCHAIN_ROOT',
    'QT_MINGW_EXPECTED_GCC_VERSION',
    'NINJA_EXE'
)
foreach ($name in $required) {
    if ([string]::IsNullOrWhiteSpace(
            [Environment]::GetEnvironmentVariable($name))) {
        throw "Missing environment variable $name"
    }
}

function Resolve-ExistingPath([string]$Path) {
    return (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
}

function Assert-UnderRoot(
    [string]$Child,
    [string]$Root,
    [string]$Label
) {
    $rootWithSlash =
        $Root.TrimEnd('\\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $Child.StartsWith(
            $rootWithSlash,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label is outside QT_SDK_ROOT: $Child"
    }
}

$sdkRoot = Resolve-ExistingPath $env:QT_SDK_ROOT
$qtRoot = Resolve-ExistingPath $env:QT_MINGW_ROOT
$toolchainRoot = Resolve-ExistingPath $env:QT_MINGW_TOOLCHAIN_ROOT
$gxx = Resolve-ExistingPath (Join-Path $toolchainRoot 'bin/g++.exe')
$gcc = Resolve-ExistingPath (Join-Path $toolchainRoot 'bin/gcc.exe')
$objdump = Resolve-ExistingPath (Join-Path $toolchainRoot 'bin/objdump.exe')
$qmake = Resolve-ExistingPath (Join-Path $qtRoot 'bin/qmake.exe')
$ninja = Resolve-ExistingPath $env:NINJA_EXE

Assert-UnderRoot $qtRoot $sdkRoot 'Qt MinGW prefix'
Assert-UnderRoot $toolchainRoot $sdkRoot 'Qt MinGW toolchain'

$triple = (& $gxx -dumpmachine).Trim()
if ($LASTEXITCODE -ne 0 -or $triple -ne 'x86_64-w64-mingw32') {
    throw "Unexpected MinGW target triple: $triple"
}

$gccVersion = (& $gxx -dumpfullversion -dumpversion).Trim()
if ($LASTEXITCODE -ne 0 -or
    $gccVersion -ne $env:QT_MINGW_EXPECTED_GCC_VERSION) {
    throw "GCC version $gccVersion does not match the Qt kit declaration"
}

$qtPrefix =
    Resolve-ExistingPath ((& $qmake -query QT_INSTALL_PREFIX).Trim())
if ($LASTEXITCODE -ne 0 -or $qtPrefix -ne $qtRoot) {
    throw "qmake prefix does not match QT_MINGW_ROOT: $qtPrefix"
}

$xspec = (& $qmake -query QMAKE_XSPEC).Trim()
if ($LASTEXITCODE -ne 0 -or $xspec -ne 'win32-g++') {
    throw "Qt kit is not the official win32-g++ kit: $xspec"
}

$qtVersion = [Version]((& $qmake -query QT_VERSION).Trim())
if ($LASTEXITCODE -ne 0 -or $qtVersion -lt [Version]'6.8.0') {
    throw "Qt MinGW kit must be 6.8.0 or newer: $qtVersion"
}

[pscustomobject]@{
    QtPrefix = $qtRoot
    QtVersion = $qtVersion.ToString()
    Compiler = $gxx
    CCompiler = $gcc
    CompilerVersion = $gccVersion
    TargetTriple = $triple
    QMake = $qmake
    ObjDump = $objdump
    Ninja = $ninja
} | ConvertTo-Json -Depth 2
