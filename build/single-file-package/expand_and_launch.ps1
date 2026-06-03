param(
    [string]$ArchivePath = (Join-Path $PSScriptRoot 'ManageSoftCppBundle.zip')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$runtimeRoot = Join-Path $env:LOCALAPPDATA 'ManageSoftCppRuntime'
$extractRoot = Join-Path $runtimeRoot ([DateTime]::Now.ToString('yyyyMMddHHmmss'))

New-Item -ItemType Directory -Path $extractRoot -Force | Out-Null
Expand-Archive -Path $ArchivePath -DestinationPath $extractRoot -Force

$entries = Get-ChildItem -Path $runtimeRoot -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending
if ($entries.Count -gt 3) {
    $entries | Select-Object -Skip 3 | ForEach-Object {
        try {
            Remove-Item $_.FullName -Recurse -Force -ErrorAction Stop
        } catch {
        }
    }
}

Start-Process -FilePath (Join-Path $extractRoot 'ManageSoftCpp.exe')
