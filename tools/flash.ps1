[CmdletBinding()]
param(
    [string]$Environment = 'LILYGO-T-Dongle-S3',
    [string]$Port
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $repoRoot

function Get-BoardPort {
    $deviceOutput = python -m platformio device list | Out-String
    $portMatch = [regex]::Match($deviceOutput, '\bCOM\d+\b')
    if ($portMatch.Success) { return $portMatch.Value.Trim() }
    return $null
}

if ([string]::IsNullOrWhiteSpace($Port)) { $Port = Get-BoardPort }
if ([string]::IsNullOrWhiteSpace($Port)) {
    throw 'No PlatformIO serial device found. Connect LilyGO and retry.'
}

Write-Host "Building and flashing $Environment on $Port..." -ForegroundColor Cyan
python -m platformio run -e $Environment -t upload --upload-port $Port
if ($LASTEXITCODE -ne 0) {
    $bootloaderPort = $null
    for ($attempt = 1; $attempt -le 6 -and [string]::IsNullOrWhiteSpace($bootloaderPort); $attempt++) {
        Start-Sleep -Seconds 2
        $candidate = Get-BoardPort
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and $candidate -ne $Port) { $bootloaderPort = $candidate }
    }
    if ([string]::IsNullOrWhiteSpace($bootloaderPort) -or $bootloaderPort -eq $Port) {
        throw "Flash failed on $Port."
    }
    Write-Host "Board re-enumerated on $bootloaderPort. Retrying upload..." -ForegroundColor Yellow
    python -m platformio run -e $Environment -t upload --upload-port $bootloaderPort
    if ($LASTEXITCODE -ne 0) { throw "Flash failed on $bootloaderPort." }
}

Write-Host "Flash complete. Board may re-enumerate on a new COM port after reset." -ForegroundColor Green
