param(
    [Parameter(Mandatory = $true)][string]$SourceDir,
    [Parameter(Mandatory = $true)][string]$Fixture
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Load the actual functions without running the packaging entry point.
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $SourceDir 'scripts/package/package-windows.ps1'),
    [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count -ne 0) { throw ($parseErrors | Out-String) }
foreach ($name in @('Invoke-Native', 'Invoke-DeployedSmokeTest')) {
    $function = $ast.Find({ param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
            $node.Name -eq $name
    }, $false)
    if ($null -eq $function) { throw "Missing packaging function: $name" }
    . ([scriptblock]::Create($function.Extent.Text))
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('zz smoke ' + [Guid]::NewGuid())
$savedLocation = (Get-Location).Path
$savedEnvironment = @{}
foreach ($name in @('QT_QPA_PLATFORM', 'ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS',
                   'ZZ_SMOKE_FIXTURE_EXIT_CODE')) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
New-Item -ItemType Directory -Path $testRoot | Out-Null
$executable = Join-Path $testRoot ([IO.Path]::GetFileName($Fixture))
Copy-Item -LiteralPath $Fixture -Destination $executable
try {
    foreach ($expectedExit in @(0, 17)) {
        $env:QT_QPA_PLATFORM = 'parent-platform'
        $env:ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS = 'parent-timeout'
        $env:ZZ_SMOKE_FIXTURE_EXIT_CODE = "$expectedExit"
        $global:LASTEXITCODE = 91 # Never trust an unrelated command's result.
        $completion = Join-Path $testRoot 'smoke-completed.txt'
        Remove-Item -LiteralPath $completion -ErrorAction SilentlyContinue
        $failure = $null
        try { Invoke-DeployedSmokeTest -Executable $executable }
        catch { $failure = $_ }
        if ($expectedExit -eq 0 -and $null -ne $failure) { throw $failure }
        if ($expectedExit -ne 0 -and
            ($null -eq $failure -or "$failure" -notmatch 'exit code 17')) {
            throw 'Smoke failure was not propagated using the child exit code'
        }
        if (-not (Test-Path -LiteralPath $completion) -or
            (Get-Content -LiteralPath $completion -Raw) -ne 'offscreen') {
            throw 'Packaging continued before the GUI smoke completed'
        }
        # On Windows this deletion also proves the GUI no longer holds the file.
        Remove-Item -LiteralPath (Join-Path $testRoot 'smoke.lock')
        if ((Get-Location).Path -ne $savedLocation -or
            $env:QT_QPA_PLATFORM -ne 'parent-platform' -or
            $env:ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS -ne 'parent-timeout') {
            throw 'Smoke did not restore the parent environment and location'
        }
    }
    Write-Host 'PASS Windows GUI smoke wait, child exit code, and cleanup'
} finally {
    foreach ($name in $savedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
    }
    Set-Location -LiteralPath $savedLocation
    Remove-Item -LiteralPath $testRoot -Recurse -Force
}
