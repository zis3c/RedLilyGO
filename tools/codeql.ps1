<#
.SYNOPSIS
    Run CodeQL locally over the RedLilyGO components.

.DESCRIPTION
    Analyzes JavaScript/TypeScript (UI), Python (tests/tools) and C# (Windows
    Agent) with the local CodeQL CLI. This works on a private repository with no
    GitHub Advanced Security, unlike the CodeQL GitHub Action which uploads to
    the Security tab. Results are written as SARIF files and a short summary is
    printed.

    The C/C++ firmware target needs a full compile and is not run here by
    default; use -IncludeCpp to attempt it after building with PlatformIO.

.PARAMETER CodeqlPath
    Path to codeql.exe. Defaults to $env:USERPROFILE\codeql\codeql\codeql.exe,
    then codeql on PATH.

.PARAMETER OutputDirectory
    Where SARIF files are written. Defaults to .\dist\codeql.

.PARAMETER RamMb
    Java heap for the query evaluator, in MB. Default 4096.

.PARAMETER Languages
    Subset of javascript, python, csharp to analyze. Defaults to all three.

.PARAMETER IncludeCpp
    Also analyze the C/C++ firmware. Requires a prior PlatformIO build.

.PARAMETER FailOnFindings
    Exit non-zero when any security finding is reported. Off by default because
    the current UI has known informational findings under review.

.EXAMPLE
    .\tools\codeql.ps1
    .\tools\codeql.ps1 -Languages javascript -RamMb 6144
    .\tools\codeql.ps1 -FailOnFindings
#>
[CmdletBinding()]
# CodeqlPath is consumed by Resolve-Codeql below; PSScriptAnalyzer cannot see
# across the function boundary, so the unused-parameter rule is suppressed.
[Diagnostics.CodeAnalysis.SuppressMessageAttribute('PSReviewUnusedParameter', 'CodeqlPath')]
param(
    [string]$CodeqlPath,
    [string]$OutputDirectory = '.\dist\codeql',
    [int]$RamMb = 4096,
    [string[]]$Languages = @('javascript', 'python', 'csharp'),
    [switch]$IncludeCpp,
    [switch]$FailOnFindings
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $repoRoot

function Resolve-Codeql {
    if ($CodeqlPath) {
        if (-not (Test-Path $CodeqlPath)) { throw "CodeQL not found at $CodeqlPath" }
        return (Resolve-Path $CodeqlPath).Path
    }
    $candidates = @(
        (Join-Path $env:USERPROFILE 'codeql\codeql\codeql.exe'),
        'codeql'
    )
    foreach ($candidate in $candidates) {
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
        if (Test-Path $candidate) { return (Resolve-Path $candidate).Path }
    }
    throw 'CodeQL CLI not found. Install the codeql-bundle-win64 release or pass -CodeqlPath.'
}

$codeql = Resolve-Codeql
$outDir = Join-Path $repoRoot $OutputDirectory
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

# Returns true when an alert is covered by a `// codeql[rule]` comment on the
# line directly above it. Mirrors the GitHub code-scanning suppression rule.
function Test-Suppressed {
    param([Parameter(Mandatory)] $Alert)
    try {
        $loc = $Alert.locations[0].physicalLocation
        $uri = $loc.artifactLocation.uri -replace '%20', ' '
        $file = Join-Path $repoRoot $uri
        if (-not (Test-Path $file)) { return $false }
        $line = [int]$loc.region.startLine
        if ($line -lt 2) { return $false }
        $prev = (Get-Content $file)[$line - 2]
        $rule = [regex]::Escape($Alert.ruleId)
        return ($prev -match "codeql\s*\[[^\]]*\b$rule\b[^\]]*\]")
    }
    catch {
        return $false
    }
}

# Native tools (codeql, dotnet, platformio) write progress to stderr. With
# ErrorActionPreference=Stop, PowerShell treats that as a terminating error, so
# relax it for the lifetime of the script and check exit codes explicitly.
$ErrorActionPreference = 'Continue'

$querySuites = @{
    javascript = 'javascript-security-and-quality.qls'
    python     = 'python-security-and-quality.qls'
    csharp     = 'csharp-security-and-quality.qls'
    cpp        = 'cpp-security-and-quality.qls'
}

# Source roots and build needs per language.
$specs = @{
    javascript = @{ source = '.'; build = $null }
    python     = @{ source = '.'; build = $null }
    csharp     = @{ source = '.'; build = 'dotnet build tools/Agent/Agent.csproj --configuration Release' }
    cpp        = @{ source = '.'; build = 'python -m platformio run -e LILYGO-T-Dongle-S3' }
}

$targets = @($Languages)
if ($IncludeCpp) { $targets += 'cpp' }
$targets = $targets | Select-Object -Unique

Write-Host ("CodeQL CLI: {0}" -f $codeql) -ForegroundColor Cyan
$totalSecurity = 0

foreach ($lang in $targets) {
    if (-not $querySuites.ContainsKey($lang)) { Write-Warning "Unknown language: $lang"; continue }
    $spec = $specs[$lang]
    $db = Join-Path $outDir ("db-{0}" -f $lang)
    $sarif = Join-Path $outDir ("{0}.sarif" -f $lang)

    Write-Host ("`n=== CodeQL: {0} ===" -f $lang) -ForegroundColor Cyan

    $createArgs = @('database', 'create', $db, "--language=$lang",
                    "--source-root=$($spec.source)", '--overwrite')
    $config = Join-Path $repoRoot '.github\codeql\codeql-config.yml'
    if (Test-Path $config) { $createArgs += @('--codescanning-config', $config) }
    if ($spec.build) {
        Write-Host ("  building: {0}" -f $spec.build)
        $createArgs += @('--command', $spec.build)
    }
    # Native tools write progress to stderr; redirect both streams so a normal
    # run under $ErrorActionPreference='Stop' does not treat it as an error.
    & $codeql @createArgs 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Warning "Database creation failed for $lang"; continue }

    $analyzeArgs = @('database', 'analyze', $db, $querySuites[$lang],
                     '--format=sarif-latest', "--output=$sarif", "--ram=$RamMb", '--download')
    & $codeql @analyzeArgs 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Warning "Analysis failed for $lang"; continue }

    $json = Get-Content $sarif -Raw | ConvertFrom-Json
    $results = @($json.runs[0].results)
    # The CodeQL CLI does not apply inline `// codeql[rule]` suppressions the way
    # the GitHub uploader does, so honour them here: drop any alert whose file
    # has a matching suppression comment on the line above it.
    $results = @($results | Where-Object { -not (Test-Suppressed $_) })
    # Persist the filtered set back to SARIF so the artifact matches the summary.
    $json.runs[0].results = $results
    $json | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $sarif -Encoding utf8

    $security = @($results | Where-Object { $_.ruleId -match 'security|injection|forgery|xss|sanitiz|untrusted|eval' })
    $totalSecurity += $security.Count
    Write-Host ("  findings: {0} total, {1} security-relevant" -f $results.Count, $security.Count) -ForegroundColor Yellow
    $results | Group-Object ruleId | Sort-Object Count -Descending |
        Select-Object -First 10 | ForEach-Object { Write-Host ("    {0,3}  {1}" -f $_.Count, $_.Name) }
    Write-Host ("  SARIF: {0}" -f $sarif)
}

Write-Host ("`nCodeQL complete. SARIF files in {0}" -f $outDir) -ForegroundColor Green
if ($FailOnFindings -and $totalSecurity -gt 0) {
    throw "CodeQL reported $totalSecurity security-relevant finding(s)."
}
