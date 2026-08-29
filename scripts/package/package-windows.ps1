[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('msvc', 'mingw')]
    [string]$Mode,
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,
    [Parameter(Mandatory = $true)]
    [string]$QtRoot,
    [Parameter(Mandatory = $true)]
    [string]$EvidenceRoot,
    [Parameter(Mandatory = $true)]
    [string]$OutputDir,
    [Parameter(Mandatory = $true)]
    [string]$Commit,
    [Parameter(Mandatory = $true)]
    [string]$BuiltAtUtc,
    [string]$DumpBin = '',
    [string]$ObjDump = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Native(
    [string]$File,
    [string[]]$Arguments
) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$File failed with exit code $LASTEXITCODE"
    }
}

function Invoke-NativeCapture(
    [string]$File,
    [string[]]$Arguments
) {
    $output = (& $File @Arguments 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "$File failed with exit code $LASTEXITCODE`n$output"
    }
    return $output
}

function Resolve-RequiredDirectory(
    [string]$Path,
    [string]$Label
) {
    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    $item = Get-Item -LiteralPath $resolved.Path -Force
    if (-not $item.PSIsContainer -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "$Label must be a regular directory: $Path"
    }
    $fullPath = [IO.Path]::GetFullPath($resolved.Path)
    return $fullPath.TrimEnd([char[]]@('\', '/'))
}

function Resolve-RequiredFile(
    [string]$Path,
    [string]$Label
) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "$Label is required"
    }
    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    $item = Get-Item -LiteralPath $resolved.Path -Force
    if ($item.PSIsContainer -or $item.Length -eq 0 -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "$Label must be a non-empty regular file: $Path"
    }
    return [IO.Path]::GetFullPath($resolved.Path)
}

function Assert-PathEquals(
    [string]$Actual,
    [string]$Expected,
    [string]$Label
) {
    if (-not $Actual.Equals(
            $Expected, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label mismatch: expected $Expected, found $Actual"
    }
}

function Get-CacheValue(
    [string]$Cache,
    [string]$Key
) {
    $pattern = '(?m)^{0}:[^=]*=(.*)$' -f [regex]::Escape($Key)
    $match = [regex]::Match($Cache, $pattern)
    if (-not $match.Success) {
        throw "CMake cache lacks $Key"
    }
    return $match.Groups[1].Value.Trim()
}

function Assert-CacheBool(
    [string]$Cache,
    [string]$Key,
    [bool]$Expected
) {
    $value = (Get-CacheValue -Cache $Cache -Key $Key).ToUpperInvariant()
    $enabled = $value -in @('1', 'ON', 'YES', 'TRUE', 'Y')
    $disabled = $value -in @('0', 'OFF', 'NO', 'FALSE', 'N', 'IGNORE')
    if (-not $enabled -and -not $disabled) {
        throw "CMake cache $Key is not a boolean: $value"
    }
    if ($enabled -ne $Expected) {
        throw "CMake cache $Key must equal $Expected; found $value"
    }
}

function Get-CompilerState([string]$ConfiguredBuildDir) {
    $compilerFiles = @(Get-ChildItem `
        -LiteralPath (Join-Path $ConfiguredBuildDir 'CMakeFiles') `
        -Recurse -Filter CMakeCXXCompiler.cmake -File)
    if ($compilerFiles.Count -ne 1) {
        throw 'Expected exactly one CMakeCXXCompiler.cmake'
    }
    return Get-Content -LiteralPath $compilerFiles[0].FullName -Raw
}

function Get-CompilerField(
    [string]$CompilerState,
    [string]$Key
) {
    $pattern = 'set\({0} "([^"]+)"\)' -f [regex]::Escape($Key)
    $match = [regex]::Match($CompilerState, $pattern)
    if (-not $match.Success) {
        throw "Compiler state lacks $Key"
    }
    return $match.Groups[1].Value
}

function Install-Component([string]$Name) {
    Invoke-Native -File 'cmake' -Arguments @(
        '--install', $script:ResolvedBuildDir,
        '--prefix', $script:StageRoot,
        '--config', 'Release',
        '--component', $Name)
}

function Invoke-WinDeployQt([string]$Executable) {
    $compilerRuntimeOption = if ($script:Mode -eq 'msvc') {
        '--no-compiler-runtime'
    } else {
        '--compiler-runtime'
    }
    Invoke-Native -File $script:WinDeployQt -Arguments @(
        '--release',
        $compilerRuntimeOption,
        '--include-plugins', 'qoffscreen',
        '--dir', (Split-Path -Parent $Executable),
        $Executable)
}

function Invoke-StageMsvcRuntime([string]$StageRoot) {
    if ($script:Mode -ne 'msvc') {
        return
    }
    $redistRoot = [Environment]::GetEnvironmentVariable(
        'VCToolsRedistDir', 'Process')
    if ([string]::IsNullOrWhiteSpace($redistRoot)) {
        throw 'VCToolsRedistDir is required for MSVC packaging'
    }
    Invoke-Native -File 'cmake' -Arguments @(
        "-DZZ_MSVC_REDIST_DIR=$redistRoot",
        "-DZZ_STAGE_ROOT=$StageRoot",
        '-P', (Join-Path $script:SourceDir `
            'scripts/package/StageMsvcRuntime.cmake'))
}

function Invoke-DeployedSmokeTest([string]$Executable) {
    $oldQpaPlatform =
        [Environment]::GetEnvironmentVariable('QT_QPA_PLATFORM', 'Process')
    $oldAutoClose = [Environment]::GetEnvironmentVariable(
        'ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS', 'Process')
    Push-Location -LiteralPath (Split-Path -Parent $Executable)
    try {
        $env:QT_QPA_PLATFORM = 'offscreen'
        $env:ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS = '1500'
        Invoke-Native -File $Executable -Arguments @('--smoke-test')
    } finally {
        if ($null -eq $oldQpaPlatform) {
            Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
        } else {
            $env:QT_QPA_PLATFORM = $oldQpaPlatform
        }
        if ($null -eq $oldAutoClose) {
            Remove-Item Env:ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS `
                -ErrorAction SilentlyContinue
        } else {
            $env:ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS = $oldAutoClose
        }
        Pop-Location
    }
}

function Assert-PeDependencies([string]$StageRoot) {
    $peFiles = @(Get-ChildItem -LiteralPath $StageRoot -Recurse -File |
        Where-Object { $_.Extension -in @('.exe', '.dll') })
    if ($peFiles.Count -eq 0) {
        throw 'Deployment stage contains no PE files'
    }

    $dependencyText = ''
    foreach ($peFile in $peFiles) {
        if ($script:Mode -eq 'msvc') {
            $headers = Invoke-NativeCapture -File $script:DependencyTool `
                -Arguments @('/headers', $peFile.FullName)
            if ($headers -notmatch '(?i)machine \(x64\)') {
                throw "MSVC package contains a non-x64 PE: $($peFile.FullName)"
            }
            $dependencyText += Invoke-NativeCapture `
                -File $script:DependencyTool `
                -Arguments @('/dependents', $peFile.FullName)
        } else {
            $headers = Invoke-NativeCapture -File $script:DependencyTool `
                -Arguments @('-f', $peFile.FullName)
            if ($headers -notmatch '(?i)file format pei-x86-64') {
                throw "MinGW package contains a non-x64 PE: $($peFile.FullName)"
            }
            $dependencyText += Invoke-NativeCapture `
                -File $script:DependencyTool `
                -Arguments @('-p', $peFile.FullName)
        }
    }

    $deployedNames = @($peFiles | ForEach-Object { $_.Name.ToLowerInvariant() })
    if ($script:Mode -eq 'msvc') {
        if ($dependencyText -match '(?i)libgcc_s|libstdc\+\+' -or
            $deployedNames -match '(?i)^libgcc_s|^libstdc\+\+') {
            throw 'MSVC package contains a MinGW runtime dependency'
        }
        if (-not ($deployedNames -match '^vcruntime.*\.dll$') -or
            -not ($deployedNames -match '^msvcp.*\.dll$')) {
            throw 'MSVC package lacks the deployed compiler runtime'
        }
    } else {
        if ($dependencyText -match '(?i)vcruntime|msvcp' -or
            $deployedNames -match '(?i)^vcruntime|^msvcp') {
            throw 'MinGW package contains an MSVC runtime dependency'
        }
        if (-not ($deployedNames -match '^libgcc_s.*\.dll$') -or
            'libstdc++-6.dll' -notin $deployedNames -or
            'libwinpthread-1.dll' -notin $deployedNames) {
            throw 'MinGW package lacks the deployed GCC runtime'
        }
    }
}

function Invoke-StageRuntimeLicenses([string]$StageRoot) {
    Invoke-Native -File 'cmake' -Arguments @(
        "-DZZ_STAGE_ROOT=$StageRoot",
        "-DZZ_QT_LICENSE_DIR=$script:QtLicenseDir",
        '-P', (Join-Path $script:SourceDir `
            'scripts/package/StageRuntimeLicenses.cmake'))
}

function Invoke-WriteBuildInfo([string]$PackagePath) {
    Invoke-Native -File 'cmake' -Arguments @(
        "-DZZ_PACKAGE_PATH=$PackagePath",
        "-DZZ_PLATFORM_ID=$script:PlatformId",
        "-DZZ_COMMIT=$script:Commit",
        '-DZZ_DIRTY=false',
        "-DZZ_BUILT_AT_UTC=$script:BuiltAtUtc",
        "-DZZ_RUNNER_OS=$([Environment]::OSVersion.VersionString)",
        '-DZZ_ARCHITECTURE=x86_64',
        "-DZZ_QT_VERSION=$script:QtVersion",
        "-DZZ_COMPILER_ID=$script:CompilerId",
        "-DZZ_COMPILER_VERSION=$script:CompilerVersion",
        "-DZZ_PRESET=$script:Preset",
        '-DZZ_LINKAGE=shared',
        '-DZZ_LTO=true',
        '-P', (Join-Path $script:SourceDir `
            'scripts/package/WriteBuildInfo.cmake'))
}

if ($Commit -notmatch '^[0-9a-f]{40}$') {
    throw 'Commit must be 40 lowercase hexadecimal characters'
}
if ($BuiltAtUtc -notmatch
    '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$') {
    throw 'BuiltAtUtc must use UTC YYYY-MM-DDTHH:MM:SSZ'
}

$script:SourceDir = Resolve-RequiredDirectory `
    (Join-Path $PSScriptRoot '../..') 'source directory'
$buildRoot = Resolve-RequiredDirectory `
    (Join-Path $script:SourceDir 'build') 'build root'
$script:ResolvedBuildDir = Resolve-RequiredDirectory $BuildDir 'BuildDir'
$script:ResolvedQtRoot = Resolve-RequiredDirectory $QtRoot 'QtRoot'
$resolvedEvidenceRoot = Resolve-RequiredDirectory `
    $EvidenceRoot 'EvidenceRoot'
$resolvedOutputDir = Resolve-RequiredDirectory $OutputDir 'OutputDir'
if (@(Get-ChildItem -LiteralPath $resolvedOutputDir -Force).Count -ne 0) {
    throw 'OutputDir must be empty'
}

$script:Preset = if ($Mode -eq 'msvc') {
    'windows-msvc2022-continuous'
} else {
    'windows-mingw-continuous'
}
$expectedBuildDir = [IO.Path]::GetFullPath(
    (Join-Path $buildRoot $script:Preset))
Assert-PathEquals $script:ResolvedBuildDir $expectedBuildDir 'BuildDir'

$cachePath = Resolve-RequiredFile `
    (Join-Path $script:ResolvedBuildDir 'CMakeCache.txt') 'CMakeCache.txt'
$cache = Get-Content -LiteralPath $cachePath -Raw
foreach ($enabledKey in @(
    'BUILD_SHARED_LIBS',
    'ZZ_ENABLE_LTO',
    'ZZ_BUILD_TESTS',
    'ZZ_BUILD_EXAMPLES',
    'ZZ_RELEASE_BUILD')) {
    Assert-CacheBool -Cache $cache -Key $enabledKey -Expected $true
}
Assert-CacheBool -Cache $cache -Key 'ZZ_BUILD_BENCHMARKS' -Expected $false
if ($Mode -eq 'msvc') {
    Assert-CacheBool `
        -Cache $cache -Key 'ZZ_ENABLE_MSVC_ANALYZE' -Expected $false
} elseif ((Get-CacheValue -Cache $cache -Key 'CMAKE_BUILD_TYPE') -ne 'Release') {
    throw 'MinGW package build must use CMAKE_BUILD_TYPE=Release'
}

$cachedQtRoot = Resolve-RequiredDirectory `
    (Get-CacheValue -Cache $cache -Key 'ZZ_QT_PREFIX') 'cached Qt root'
$cachedEvidenceRoot = Resolve-RequiredDirectory `
    (Get-CacheValue -Cache $cache -Key 'ZZ_RELEASE_EVIDENCE_ROOT') `
    'cached evidence root'
Assert-PathEquals $cachedQtRoot $script:ResolvedQtRoot 'QtRoot'
Assert-PathEquals $cachedEvidenceRoot $resolvedEvidenceRoot 'EvidenceRoot'

$qmake = Resolve-RequiredFile `
    (Join-Path $script:ResolvedQtRoot 'bin/qmake.exe') 'qmake.exe'
$script:WinDeployQt = Resolve-RequiredFile `
    (Join-Path $script:ResolvedQtRoot 'bin/windeployqt.exe') 'windeployqt.exe'
$queriedQtRootValue = (Invoke-NativeCapture `
    -File $qmake -Arguments @('-query', 'QT_INSTALL_PREFIX')).Trim()
$queriedQtRoot = Resolve-RequiredDirectory `
    $queriedQtRootValue 'qmake Qt prefix'
Assert-PathEquals $queriedQtRoot $script:ResolvedQtRoot 'qmake Qt prefix'
$script:QtVersion = (Invoke-NativeCapture `
    -File $qmake -Arguments @('-query', 'QT_VERSION')).Trim()
if ([string]::IsNullOrWhiteSpace($script:QtVersion)) {
    throw 'qmake failed to query QT_VERSION'
}
$script:QtLicenseDir = Resolve-RequiredDirectory `
    (Join-Path $resolvedEvidenceRoot `
        "qt-$script:QtVersion/LICENSES") 'Qt license directory'
$qmakeXspec = (Invoke-NativeCapture `
    -File $qmake -Arguments @('-query', 'QMAKE_XSPEC')).Trim()
if (($Mode -eq 'msvc' -and $qmakeXspec -notmatch '^win32-msvc') -or
    ($Mode -eq 'mingw' -and $qmakeXspec -ne 'win32-g++')) {
    throw "Qt kit does not match package mode ${Mode}: $qmakeXspec"
}

$compilerState = Get-CompilerState $script:ResolvedBuildDir
$script:CompilerId = Get-CompilerField `
    $compilerState 'CMAKE_CXX_COMPILER_ID'
$script:CompilerVersion = Get-CompilerField `
    $compilerState 'CMAKE_CXX_COMPILER_VERSION'
$expectedCompilerId = if ($Mode -eq 'msvc') { 'MSVC' } else { 'GNU' }
if ($script:CompilerId -ne $expectedCompilerId) {
    throw "Compiler id does not match package mode ${Mode}: " +
        $script:CompilerId
}

$script:DependencyTool = if ($Mode -eq 'msvc') {
    Resolve-RequiredFile $DumpBin 'dumpbin'
} else {
    Resolve-RequiredFile $ObjDump 'objdump'
}
if ($Mode -eq 'msvc') {
    if ([IO.Path]::GetFileName($script:DependencyTool) -ne 'dumpbin.exe') {
        throw 'DumpBin must identify dumpbin.exe'
    }
} else {
    if ([IO.Path]::GetFileName($script:DependencyTool) -ne 'objdump.exe') {
        throw 'ObjDump must identify objdump.exe'
    }
    $cachedObjDump = Resolve-RequiredFile `
        (Get-CacheValue -Cache $cache -Key 'CMAKE_OBJDUMP') `
        'cached objdump'
    Assert-PathEquals $script:DependencyTool $cachedObjDump 'ObjDump'
}
$script:PlatformId = if ($Mode -eq 'msvc') {
    'windows-msvc2022-x86_64'
} else {
    'windows-mingw-x86_64'
}
$packageName =
    "ZzPureToolsExample-continuous-$($script:PlatformId).zip"
$finalPackage = Join-Path $resolvedOutputDir $packageName
$finalChecksum = "$finalPackage.sha256"
$finalBuildInfo = Join-Path $resolvedOutputDir 'build-info.json'
$workDir = Join-Path $resolvedOutputDir `
    ".windows-package.$([Guid]::NewGuid().ToString('N'))"
$script:StageRoot = Join-Path $workDir 'stage'
$workingPackage = Join-Path $workDir $packageName
$published = $false

New-Item -ItemType Directory -Path $script:StageRoot | Out-Null
try {
    Install-Component -Name 'Runtime'
    Install-Component -Name 'ExampleRuntime'

    $exampleExecutable = Resolve-RequiredFile `
        (Join-Path $script:StageRoot 'bin/ZzPureToolsExample.exe') `
        'installed ZzPureToolsExample.exe'
    Invoke-WinDeployQt -Executable $exampleExecutable
    Invoke-StageMsvcRuntime -StageRoot $script:StageRoot
    Invoke-DeployedSmokeTest -Executable $exampleExecutable
    Assert-PeDependencies -StageRoot $script:StageRoot
    Invoke-StageRuntimeLicenses -StageRoot $script:StageRoot

    $archiveInputs = @(Get-ChildItem -LiteralPath $script:StageRoot -Force |
        ForEach-Object { $_.FullName })
    if ($archiveInputs.Count -eq 0) {
        throw 'Deployment stage is empty'
    }
    Compress-Archive -LiteralPath $archiveInputs `
        -DestinationPath $workingPackage -CompressionLevel Optimal
    $workingPackage = Resolve-RequiredFile $workingPackage 'deployment ZIP'
    Invoke-WriteBuildInfo -PackagePath $workingPackage

    Move-Item -LiteralPath "$workingPackage.sha256" `
        -Destination $finalChecksum
    Move-Item -LiteralPath (Join-Path $workDir 'build-info.json') `
        -Destination $finalBuildInfo
    Move-Item -LiteralPath $workingPackage -Destination $finalPackage
    $published = $true
    Write-Host "PASS Windows $Mode package: $finalPackage"
} finally {
    if (Test-Path -LiteralPath $workDir) {
        $expectedWorkPrefix = $resolvedOutputDir +
            [IO.Path]::DirectorySeparatorChar + '.windows-package.'
        if (-not $workDir.StartsWith(
                $expectedWorkPrefix,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clean unexpected work directory: $workDir"
        }
        Remove-Item -LiteralPath $workDir -Recurse -Force
    }
    if (-not $published) {
        foreach ($failedOutput in @(
            $finalPackage, $finalChecksum, $finalBuildInfo)) {
            Remove-Item -LiteralPath $failedOutput `
                -Force -ErrorAction SilentlyContinue
        }
    }
}
