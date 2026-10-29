[CmdletBinding()]
param(
    [string]$Environment = 'LILYGO-T-Dongle-S3',
    [string]$BuildDirectory = '.pio\build',
    [string]$AgentPath = 'dist\agent-win-x64\RedLilyGO.Agent.exe',
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $repoRoot
$version = (Get-Content -Raw (Join-Path $repoRoot 'VERSION')).Trim()
$releaseDir = if ($OutputDirectory) { $OutputDirectory } else { Join-Path $repoRoot "dist\RedLilyGO-v$version" }
if (-not [IO.Path]::IsPathRooted($releaseDir)) { $releaseDir = Join-Path $repoRoot $releaseDir }
$releaseDir = [IO.Path]::GetFullPath($releaseDir)
$distRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'dist')) + [IO.Path]::DirectorySeparatorChar
if (-not $releaseDir.StartsWith($distRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Release output must stay inside this repository dist directory.'
}
$firmwareDir = Join-Path $BuildDirectory $Environment
$agent = $AgentPath

$required = @(
    (Join-Path $firmwareDir 'firmware.bin'),
    (Join-Path $firmwareDir 'bootloader.bin'),
    (Join-Path $firmwareDir 'partitions.bin'),
    $agent,
    (Join-Path $repoRoot 'LICENSE'),
    (Join-Path $repoRoot 'THIRD_PARTY_NOTICES.md'),
    (Join-Path $repoRoot 'SECURITY.md'),
    (Join-Path $repoRoot 'docs\SETUP.md')
)
foreach ($path in $required) { if (-not (Test-Path -LiteralPath $path)) { throw "Missing release artifact: $path" } }
if (Test-Path -LiteralPath $releaseDir) { throw "Release directory already exists: $releaseDir. Choose a new -OutputDirectory." }
New-Item -ItemType Directory -Path $releaseDir | Out-Null
Copy-Item -LiteralPath $required -Destination $releaseDir

$licenseStaging = Join-Path $releaseDir '.license-staging'
New-Item -ItemType Directory -Path $licenseStaging | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE'), (Join-Path $repoRoot 'THIRD_PARTY_NOTICES.md') -Destination $licenseStaging
Copy-Item -LiteralPath (Join-Path $repoRoot 'lib\ESP32Marauder\LICENSE') -Destination (Join-Path $licenseStaging 'ESP32Marauder-LICENSE')
Copy-Item -Path (Join-Path $repoRoot 'ui\vnc\LICENSE*') -Destination $licenseStaging
Copy-Item -LiteralPath (Join-Path $repoRoot 'ui\vnc\vendor\pako\LICENSE') -Destination (Join-Path $licenseStaging 'pako-LICENSE')
Get-ChildItem (Join-Path $repoRoot 'ui\content\assets\vendor') -Recurse -File | Where-Object { $_.Name -match '^(LICENSE|COPYING|NOTICE)' } | ForEach-Object {
    $relative = $_.FullName.Substring($repoRoot.Path.Length).TrimStart('\')
    $target = Join-Path $licenseStaging $relative
    New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $target
}

$licenseRoots = @(
    @{ Name = 'platformio-libraries'; Path = (Join-Path $repoRoot ".pio\libdeps\$Environment") },
    @{ Name = 'platformio-packages'; Path = (Join-Path $env:USERPROFILE '.platformio\packages') },
    @{ Name = 'nuget-packages'; Path = (Join-Path $env:USERPROFILE '.nuget\packages') }
)
foreach ($root in $licenseRoots) {
    if (-not (Test-Path -LiteralPath $root.Path)) { continue }
    Get-ChildItem $root.Path -Recurse -File | Where-Object {
        $_.Name -match '^(LICENSE|COPYING|NOTICE)' -or
        $_.Name -match '^(library|platform)\.json$|^library\.properties$' -or
        $_.Extension -eq '.nuspec'
    } | ForEach-Object {
        $relative = $_.FullName.Substring($root.Path.Length).TrimStart('\')
        $target = Join-Path (Join-Path $licenseStaging $root.Name) $relative
        New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
        Copy-Item -LiteralPath $_.FullName -Destination $target
    }
}
Compress-Archive -Path (Join-Path $licenseStaging '*') -DestinationPath (Join-Path $releaseDir 'THIRD_PARTY_LICENSES.zip')
$resolvedStaging = (Resolve-Path -LiteralPath $licenseStaging).Path
if ($resolvedStaging -ne (Join-Path $releaseDir '.license-staging')) { throw 'Unexpected license staging path.' }
Remove-Item -LiteralPath $licenseStaging -Recurse -Force

$commit = (git -C $repoRoot rev-parse HEAD).Trim()
$files = Get-ChildItem -LiteralPath $releaseDir -File | Sort-Object Name
$artifacts = @($files | ForEach-Object {
    [ordered]@{ name = $_.Name; size = $_.Length; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant() }
})
$manifest = [ordered]@{
    product = 'RedLilyGO'; version = $version; environment = $Environment
    builtAtUtc = [DateTime]::UtcNow.ToString('o'); gitCommit = $commit; artifacts = $artifacts
    gitDirty = [bool](git -C $repoRoot status --porcelain)
    hardwareTested = $false
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $releaseDir 'release-manifest.json') -Encoding utf8

$checksumFiles = Get-ChildItem -LiteralPath $releaseDir -File | Sort-Object Name
$checksumLines = $checksumFiles | ForEach-Object { "{0} *{1}" -f (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant(), $_.Name }
$checksumLines | Set-Content -LiteralPath (Join-Path $releaseDir 'SHA256SUMS.txt') -Encoding ascii
Write-Host "Release package created: $releaseDir" -ForegroundColor Green
