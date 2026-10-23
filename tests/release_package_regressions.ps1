# Exercise release preflight without building, flashing or collecting licenses.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$sandbox = Join-Path $root ('dist\release-regression-' + [Guid]::NewGuid().ToString('N'))
$script = Join-Path $root 'tools\package-release.ps1'
New-Item -ItemType Directory -Path (Join-Path $sandbox 'build\fixture'), (Join-Path $sandbox 'existing') -Force | Out-Null
try {
    foreach ($name in 'firmware.bin', 'bootloader.bin', 'partitions.bin') {
        Set-Content -LiteralPath (Join-Path $sandbox "build\fixture\$name") -Value 'fixture'
    }
    $agent = Join-Path $sandbox 'agent.exe'
    Set-Content -LiteralPath $agent -Value 'fixture'
    $sentinel = Join-Path $sandbox 'existing\preserve.txt'
    Set-Content -LiteralPath $sentinel -Value 'preserve'
    foreach ($case in @(
        @{ Agent = (Join-Path $sandbox 'missing.exe'); Output = (Join-Path $sandbox 'existing'); Error = 'Missing release artifact' },
        @{ Agent = $agent; Output = (Join-Path $sandbox 'existing'); Error = 'already exists' },
        @{ Agent = $agent; Output = $root; Error = 'must stay inside' }
    )) {
        $caught = $false
        try {
            & $script -Environment fixture -BuildDirectory (Join-Path $sandbox 'build') -AgentPath $case.Agent -OutputDirectory $case.Output
        } catch {
            if ($_.Exception.Message -notlike "*$($case.Error)*") { throw }
            $caught = $true
        }
        if (-not $caught -or (Get-Content -Raw -LiteralPath $sentinel).Trim() -ne 'preserve') {
            throw 'Release preflight failed to preserve the existing package.'
        }
    }
    Write-Host 'PASS release missing inputs, existing package preservation and output containment'
} finally {
    $resolved = (Resolve-Path -LiteralPath $sandbox).Path
    $expectedRoot = [IO.Path]::GetFullPath((Join-Path $root 'dist')) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($expectedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected fixture cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
