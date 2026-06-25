$ScriptDir = $PSScriptRoot

# 部分 Windows 环境的 python3 启动器无法正常工作，先验证可用性
$PythonCmd = "python"
if (Get-Command python3 -ErrorAction SilentlyContinue) {
    $testOutput = & python3 -c "print('ok')" 2>&1
    if ($testOutput -eq "ok") {
        $PythonCmd = "python3"
    }
}
$env:PYTHONUTF8 = "1"

function Get-ProjectConfigValue {
    param([Parameter(Mandatory = $true)][string]$Key)
    $output = & $PythonCmd (Join-Path $ScriptDir "project_config.py") --print $Key
    if ($LASTEXITCODE -ne 0) { throw "读取 project.json 失败: $Key" }
    return $output.Trim()
}

$ProjectName = Get-ProjectConfigValue name
$ExampleName = Get-ProjectConfigValue example
$ProjectVersion = Get-ProjectConfigValue version
