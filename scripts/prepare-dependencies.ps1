$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$dependencies = Get-Content (Join-Path $repoRoot 'dependencies.json') -Raw | ConvertFrom-Json
foreach ($entry in $dependencies.PSObject.Properties) {
    $file = Join-Path $repoRoot $entry.Name
    if (-not (Test-Path -LiteralPath $file)) { throw "Missing dependency: $($entry.Name)" }
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLower() -ne $entry.Value.sha256) { throw "Dependency hash mismatch: $($entry.Name)" }
}
$packageFolder = Join-Path $repoRoot 'vendor/packages/Microsoft.Windows.CppWinRT.2.0.210806.1'
if (-not (Test-Path -LiteralPath $packageFolder)) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $repoRoot 'vendor/Microsoft.Windows.CppWinRT.2.0.210806.1.nupkg'), $packageFolder)
}
