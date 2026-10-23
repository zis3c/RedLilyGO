<#
.SYNOPSIS
    Remove generated build and test output from the working tree.

.DESCRIPTION
    RedLilyGO writes build output, logs and dependency caches into the working
    tree. A full checkout can grow past 1.5 GB even though git tracks almost
    none of it. This script deletes only generated paths that can be recreated
    from source, so the tracked project is never touched.

    Always run a normal invocation before release or hand-off:

        .\tools\clean.ps1

    Use -Deep to also remove UI and Python dependency caches. The next build
    restores them, but it takes noticeably longer.

.PARAMETER Deep
    Also remove ui/node_modules, Python __pycache__ directories and the
    isolated quality build tree under .pio.

.PARAMETER DryRun
    Print what would be removed without deleting anything.

.EXAMPLE
    .\tools\clean.ps1
    .\tools\clean.ps1 -Deep
    .\tools\clean.ps1 -DryRun
#>
param(
    [switch]$Deep,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot

$generatedPaths = @(
    ".pio",
    "dist",
    "Debug",
    "test-results",
    "pio_upload.err.log",
    "pio_upload.out.log",
    "tools/Agent/bin",
    "tools/Agent/obj",
    "tools/Agent.Tests/bin",
    "tools/Agent.Tests/obj",
    "tests/agent_transport/bin",
    "tests/agent_transport/obj"
)

if ($Deep) {
    $generatedPaths += @(
        "ui/node_modules",
        "ui/test-results"
    )
}

$freedBytes = 0
$removedCount = 0

foreach ($relative in $generatedPaths) {
    $target = Join-Path $repoRoot $relative
    if (-not (Test-Path -LiteralPath $target)) { continue }

    $size = (Get-ChildItem -LiteralPath $target -Recurse -File -Force -ErrorAction SilentlyContinue |
        Measure-Object -Property Length -Sum).Sum
    if (-not $size) { $size = 0 }

    if ($DryRun) {
        Write-Host ("would remove {0} ({1:N1} MB)" -f $relative, ($size / 1MB))
    } else {
        Remove-Item -LiteralPath $target -Recurse -Force
        Write-Host ("removed {0} ({1:N1} MB)" -f $relative, ($size / 1MB))
    }
    $freedBytes += $size
    $removedCount++
}

if ($Deep -and -not $DryRun) {
    $pycacheDirs = Get-ChildItem -Path $repoRoot -Recurse -Force -Directory -Filter "__pycache__" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch "node_modules" }
    foreach ($dir in $pycacheDirs) {
        Remove-Item -LiteralPath $dir.FullName -Recurse -Force -ErrorAction SilentlyContinue
        $removedCount++
    }
}

$verb = if ($DryRun) { "would reclaim" } else { "reclaimed" }
Write-Host ("{0} {1:N1} MB across {2} path(s)." -f $verb, ($freedBytes / 1MB), $removedCount)
