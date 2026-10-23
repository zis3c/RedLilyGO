[CmdletBinding()]
param([string]$Tag = $env:GITHUB_REF_NAME)

$ErrorActionPreference = 'Stop'
$version = (Get-Content -Raw (Join-Path $PSScriptRoot '..\VERSION')).Trim()
if ([string]::IsNullOrWhiteSpace($Tag)) { throw 'Release tag is required.' }
if ($Tag -ne "v$version") { throw "Tag $Tag does not match VERSION $version." }
$agentProject = Get-Content -Raw (Join-Path $PSScriptRoot 'Agent\Agent.csproj')
if ($agentProject -notmatch "<AssemblyVersion>$([regex]::Escape($version))\.0</AssemblyVersion>") {
    throw "Agent AssemblyVersion does not match VERSION $version."
}
$agentProgram = Get-Content -Raw (Join-Path $PSScriptRoot 'Agent\Executer.cs')
$agentVersionPattern = 'Version = "' + [regex]::Escape($version) + '"'
if ($agentProgram -notmatch $agentVersionPattern) {
    throw "Agent protocol version metadata does not match VERSION $version."
}
Write-Host "Release version verified: $version" -ForegroundColor Green
