$ScriptDir = $PSScriptRoot

function Get-ProjectConfigValue {
    param([Parameter(Mandatory = $true)][string]$Key)
    $output = & python3 (Join-Path $ScriptDir "project_config.py") --print $Key
    if ($LASTEXITCODE -ne 0) { throw "读取 project.json 失败: $Key" }
    return $output.Trim()
}

$ProjectName = Get-ProjectConfigValue name
$ExampleName = Get-ProjectConfigValue example
$ProjectVersion = Get-ProjectConfigValue version
