[CmdletBinding()]
param(
    [string]$AgentPath = (Join-Path $PSScriptRoot '..\dist\agent-win-x64\RedLilyGO.Agent.exe'),
    [string]$Vid = 'CAFE',
    [string]$DevicePid = '403F'
)

$ErrorActionPreference = 'Stop'
$resolvedAgent = Resolve-Path -LiteralPath $AgentPath -ErrorAction SilentlyContinue
if (-not $resolvedAgent) {
    throw "Agent executable not found: $AgentPath. Run .\tools\publish-agent.ps1 first."
}

Write-Host "Starting RedLilyGO Agent. Connect board and keep this window open." -ForegroundColor Cyan
& $resolvedAgent.Path "vid=$Vid" "pid=$DevicePid"
