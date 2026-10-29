<#
.SYNOPSIS
    Long-duration hardware soak: watch heap, temperature and resets.

.DESCRIPTION
    Polls the device /telemetry and /health endpoints for a chosen duration and
    records free heap, the firmware-tracked minimum free heap, temperature and
    reset state. A rising minimum free heap indicates the device rebooted during
    the run. Results are written as JSON and CSV evidence.

    This is an instrumented runtime check on a real board. It complements, and
    does not replace, the manual hardware test record.

.PARAMETER WebUri
    Base URL of the dashboard, for example http://4.3.2.1.

.PARAMETER Credential
    Basic Auth credential when web authentication is enabled.

.PARAMETER Minutes
    Total soak duration in minutes (default 60).

.PARAMETER IntervalSec
    Seconds between samples (default 10).

.PARAMETER EvidenceDirectory
    Output folder for the JSON and CSV evidence files.

.PARAMETER MinFreeHeapBytes
    Fail the run if any sample reports less free heap than this. Default 4096.

.PARAMETER MaxTemperatureC
    Fail the run if any sample reaches this temperature. Default 95 (the
    firmware thermal limit is 95 C).

.PARAMETER HeapRegressionBytes
    Fail the run if free heap ends this many bytes below its first sample,
    which indicates a leak that survived the run. Default 8192.

.OUTPUTS
    Writes JSON and CSV evidence and prints a PASS/FAIL verdict. The process
    exits non-zero on FAIL so it can gate a release script.

.EXAMPLE
    .\tools\hardware-soak.ps1 -WebUri http://4.3.2.1 -Minutes 30
    .\tools\hardware-soak.ps1 -WebUri http://4.3.2.1 -Credential (Get-Credential) -Minutes 120
#>
[CmdletBinding()]
param(
    [string]$WebUri = 'http://4.3.2.1',
    [PSCredential]$Credential,
    [int]$Minutes = 60,
    [int]$IntervalSec = 10,
    [string]$EvidenceDirectory = '.\dist\hardware-evidence',
    [int64]$MinFreeHeapBytes = 4096,
    [double]$MaxTemperatureC = 95,
    [int64]$HeapRegressionBytes = 8192
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $repoRoot

$base = $WebUri.TrimEnd('/')
$headers = @{}
if ($null -ne $Credential) {
    $pair = '{0}:{1}' -f $Credential.UserName, $Credential.GetNetworkCredential().Password
    $headers['Authorization'] = 'Basic ' + [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes($pair))
}

function Get-Json([string]$path) {
    return Invoke-RestMethod -Uri ($base + $path) -Headers $headers -TimeoutSec 8
}

$samples = New-Object System.Collections.Generic.List[object]
$resetCount = 0
$unreachable = 0
$previousMinFree = $null
$deadline = (Get-Date).AddMinutes($Minutes)

Write-Host ("Soak started for {0} minute(s), sampling every {1}s..." -f $Minutes, $IntervalSec) -ForegroundColor Cyan
Write-Host ("Thresholds: minFree>={0}B, maxTemp<{1}C, heapRegression<={2}B" -f $MinFreeHeapBytes, $MaxTemperatureC, $HeapRegressionBytes) -ForegroundColor DarkGray

try {
    while ((Get-Date) -lt $deadline) {
        try {
            $telemetry = Get-Json '/telemetry'
            $health = Get-Json '/health'

            $freeHeap = [int64]$telemetry.freeHeap
            $minFreeHeap = [int64]$telemetry.minFreeHeap
            $temperature = [double]$telemetry.temperatureC
            $errorCount = [int64]$telemetry.errorCount

            # The firmware-tracked minimum can only fall within one boot, so a
            # higher value than the previous sample means the device rebooted.
            if ($null -ne $previousMinFree -and $minFreeHeap -gt $previousMinFree) {
                $resetCount++
                Write-Host "Detected a reboot (min free heap rose $previousMinFree -> $minFreeHeap)." -ForegroundColor Yellow
            }
            $previousMinFree = $minFreeHeap

            $samples.Add([pscustomobject]@{
                timeUtc        = (Get-Date).ToUniversalTime().ToString('o')
                freeHeap       = $freeHeap
                minFreeHeap    = $minFreeHeap
                temperatureC   = $temperature
                errorCount     = $errorCount
                resetReason    = $health.resetReason
                lastBootCrash  = [bool]$health.lastBootCrash
            })
            Write-Host ("{0}  free={1}  minFree={2}  temp={3}C  errors={4}" -f (Get-Date).ToString('HH:mm:ss'), $freeHeap, $minFreeHeap, $temperature, $errorCount)
        }
        catch {
            $unreachable++
            Write-Host ("Sample failed ({0}); device may be rebooting." -f $_.Exception.Message) -ForegroundColor DarkYellow
        }
        Start-Sleep -Seconds $IntervalSec
    }
}
finally {
    $evidencePath = Join-Path $repoRoot $EvidenceDirectory
    New-Item -ItemType Directory -Path $evidencePath -Force | Out-Null
    $stamp = (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss')

    $csv = Join-Path $evidencePath ("soak-{0}.csv" -f $stamp)
    $samples | Export-Csv -LiteralPath $csv -NoTypeInformation

    $lowestFree = if ($samples.Count) { ($samples | Measure-Object -Property freeHeap -Minimum).Minimum } else { $null }
    $lastFree = if ($samples.Count) { [int64]$samples[-1].freeHeap } else { $null }
    $firstFree = if ($samples.Count) { [int64]$samples[0].freeHeap } else { $null }
    $heapTrend = if ($null -ne $firstFree -and $null -ne $lastFree) { $lastFree - $firstFree } else { $null }
    $peakTemp = if ($samples.Count) { ($samples | Measure-Object -Property temperatureC -Maximum).Maximum } else { $null }

    # Fail reasons are objective: reboots, unreachable gaps, low heap, thermal
    # proximity and leak-like downward heap trend across the window.
    $failReasons = New-Object System.Collections.Generic.List[string]
    if ($resetCount -gt 0) { $failReasons.Add("device rebooted $resetCount time(s)") }
    if ($samples.Count -eq 0) { $failReasons.Add('no successful samples') }
    if ($unreachable -gt [Math]::Max(2, [int]($samples.Count * 0.1))) { $failReasons.Add("$unreachable unreachable samples") }
    if ($null -ne $lowestFree -and $lowestFree -lt $MinFreeHeapBytes) { $failReasons.Add("free heap $lowestFree < $MinFreeHeapBytes") }
    if ($null -ne $peakTemp -and $peakTemp -ge $MaxTemperatureC) { $failReasons.Add("temperature $peakTemp >= $MaxTemperatureC") }
    if ($null -ne $heapTrend -and $heapTrend -lt (-1 * $HeapRegressionBytes)) { $failReasons.Add("heap trend $heapTrend below -$HeapRegressionBytes (possible leak)") }

    $verdict = if ($failReasons.Count -eq 0) { 'PASS' } else { 'FAIL' }

    $summary = [ordered]@{
        checkedAtUtc     = (Get-Date).ToUniversalTime().ToString('o')
        webUri           = $base
        minutes          = $Minutes
        intervalSec      = $IntervalSec
        sampleCount      = $samples.Count
        unreachableCount = $unreachable
        rebootCount      = $resetCount
        firstFreeHeap    = $firstFree
        lastFreeHeap     = $lastFree
        minFreeHeap      = $lowestFree
        heapTrendBytes   = $heapTrend
        lowestTrackedMin = if ($samples.Count) { ($samples | Measure-Object -Property minFreeHeap -Minimum).Minimum } else { $null }
        maxTemperatureC  = $peakTemp
        lastResetReason  = if ($samples.Count) { $samples[-1].resetReason } else { $null }
        anyBootCrash     = [bool]($samples | Where-Object { $_.lastBootCrash } | Select-Object -First 1)
        thresholds       = [ordered]@{
            minFreeHeapBytes    = $MinFreeHeapBytes
            maxTemperatureC     = $MaxTemperatureC
            heapRegressionBytes = $HeapRegressionBytes
        }
        verdict          = $verdict
        failReasons      = ($failReasons -join '; ')
    }
    $json = Join-Path $evidencePath ("soak-{0}.json" -f $stamp)
    $summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $json -Encoding utf8

    Write-Host ''
    Write-Host 'Soak summary:' -ForegroundColor Cyan
    $summary.GetEnumerator() | ForEach-Object { Write-Host ("  {0,-16} {1}" -f $_.Key, $_.Value) }
    Write-Host ("Evidence written: {0} and {1}" -f $json, $csv) -ForegroundColor Green

    if ($verdict -ne 'PASS') {
        Write-Host ("SOAK RESULT: FAIL - {0}" -f ($failReasons -join '; ')) -ForegroundColor Red
        exit 1
    }
    Write-Host 'SOAK RESULT: PASS' -ForegroundColor Green
}
