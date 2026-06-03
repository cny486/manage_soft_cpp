param(
    [Parameter(Mandatory = $true)]
    [string]$ServerHost,

    [Parameter(Mandatory = $true)]
    [string]$Username,

    [string]$Password,
    [int]$Port = 22,
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [string]$RemoteRoot = '~/manage_soft_cpp_linux_build',
    [string]$ServiceName = 'manage-soft-server',
    [string]$ListenHost = '0.0.0.0',
    [int]$ListenPort = 45454,
    [string]$ApiUrl = '',
    [string]$ApiKey = '',
    [string]$ApiModel = '',
    [int]$ApiTimeoutMs = 30000,
    [switch]$SkipSystemd
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$python = Join-Path $repoRoot '.venv\Scripts\python.exe'
if (-not (Test-Path $python)) {
    $python = (Get-Command python -ErrorAction Stop).Source
}

$arguments = @(
    (Join-Path $PSScriptRoot 'deploy_linux_server.py'),
    '--host', $ServerHost,
    '--port', $Port,
    '--username', $Username,
    '--configuration', $Configuration,
    '--remote-root', $RemoteRoot,
    '--service-name', $ServiceName,
    '--listen-host', $ListenHost,
    '--listen-port', $ListenPort,
    '--api-timeout-ms', $ApiTimeoutMs
)

if (-not [string]::IsNullOrWhiteSpace($ApiUrl)) {
    $arguments += @('--api-url', $ApiUrl)
}

if (-not [string]::IsNullOrWhiteSpace($ApiKey)) {
    $arguments += @('--api-key', $ApiKey)
}

if (-not [string]::IsNullOrWhiteSpace($ApiModel)) {
    $arguments += @('--api-model', $ApiModel)
}

if ($Password) {
    $arguments += @('--password', $Password)
}

if ($SkipSystemd) {
    $arguments += '--skip-systemd'
}

& $python @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Linux deployment failed with exit code $LASTEXITCODE"
}