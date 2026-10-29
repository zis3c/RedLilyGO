[CmdletBinding()]
param(
    [string]$Environment = 'LILYGO-T-Dongle-S3',
    [string]$Port,
    [string]$WebUri,
    [switch]$Flash,
    [PSCredential]$Credential,
    [string]$EvidenceDirectory = '.\dist\hardware-evidence'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $repoRoot

function Get-BoardPort {
    $deviceOutput = python -m platformio device list | Out-String
    $portMatch = [regex]::Match($deviceOutput, '(?ms)^(COM\d+).*?Hardware ID:.*?VID:PID=(?:CAFE:403F|303A:1001)')
    if ($portMatch.Success) { return $portMatch.Groups[1].Value.Trim() }
    return $null
}

$devices = python -m platformio device list | Out-String
if ([string]::IsNullOrWhiteSpace($Port)) {
    $match = [regex]::Match($devices, '(?ms)^(COM\d+).*?Hardware ID:.*?VID:PID=CAFE:403F')
    if ($match.Success) { $Port = $match.Groups[1].Value.Trim() }
}

if ([string]::IsNullOrWhiteSpace($Port)) {
    Write-Warning 'No serial board detected. Running build-only smoke check.'
}
else {
    if ($devices -notmatch "(?ms)^$([regex]::Escape($Port)).*?VID:PID=CAFE:403F") { throw "$Port is not expected CAFE:403F board." }
    Write-Host "Detected board port: $Port" -ForegroundColor Green
}

Write-Host "Building $Environment for hardware smoke check..." -ForegroundColor Cyan
python -m platformio run -e $Environment
if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed.' }

if ($Flash) {
    if ([string]::IsNullOrWhiteSpace($Port)) { throw 'Flash requested but no serial port detected.' }
    Write-Host "Flashing $Environment on $Port..." -ForegroundColor Yellow
    $ErrorActionPreference = 'Continue'
    $uploadOutput = python -m platformio run -e $Environment -t upload --upload-port $Port 2>&1 | Out-String
    $uploadExitCode = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    $uploadOutput | Write-Host
    if ($uploadExitCode -ne 0) {
        $bootloaderPort = $null
        for ($attempt = 1; $attempt -le 6 -and [string]::IsNullOrWhiteSpace($bootloaderPort); $attempt++) {
            Start-Sleep -Seconds 2
            $candidate = Get-BoardPort
            if (-not [string]::IsNullOrWhiteSpace($candidate) -and $candidate -ne $Port) { $bootloaderPort = $candidate }
        }
        if ([string]::IsNullOrWhiteSpace($bootloaderPort) -or $bootloaderPort -eq $Port) {
            throw 'Firmware flash failed.'
        }
        Write-Host "Board re-enumerated on $bootloaderPort. Retrying upload..." -ForegroundColor Yellow
        $ErrorActionPreference = 'Continue'
        $uploadOutput = python -m platformio run -e $Environment -t upload --upload-port $bootloaderPort 2>&1 | Out-String
        $uploadExitCode = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        $uploadOutput | Write-Host
        if ($uploadExitCode -ne 0) { throw 'Firmware flash failed on retry.' }
    }
    if ($uploadOutput -notmatch 'Hash of data verified') { throw 'Flash completed without hash verification evidence.' }
    Write-Host 'Flash passed. Perform USB/WiFi/display smoke checks on hardware.' -ForegroundColor Green
}
else {
    Write-Host 'Build-only smoke check passed. Use -Flash for explicit hardware flash.' -ForegroundColor Green
}

if (-not [string]::IsNullOrWhiteSpace($WebUri)) {
    $setupUri = $WebUri.TrimEnd('/') + '/setup/status'
    $healthUri = $WebUri.TrimEnd('/') + '/health'
    Write-Host "Checking live setup endpoint $setupUri..." -ForegroundColor Cyan
    try {
        $setupResponse = Invoke-WebRequest -Uri $setupUri -TimeoutSec 8
        if ($setupResponse.StatusCode -ne 200 -or $setupResponse.Content -notmatch 'required') { throw "Unexpected response from $setupUri" }
        $healthContent = $null
        if ($null -ne $Credential) {
            $healthResponse = Invoke-WebRequest -Uri $healthUri -Credential $Credential -AllowUnencryptedAuthentication -TimeoutSec 8
            if ($healthResponse.StatusCode -ne 200 -or $healthResponse.Content -notmatch 'overall') { throw "Unexpected response from $healthUri" }
            $healthContent = $healthResponse.Content | ConvertFrom-Json
        }
        Write-Host 'Live WiFi setup and health smoke check passed.' -ForegroundColor Green
    }
    catch {
        throw "Live WiFi web smoke check failed: $($_.Exception.Message)"
    }
}

$evidencePath = Join-Path $repoRoot $EvidenceDirectory
New-Item -ItemType Directory -Path $evidencePath -Force | Out-Null
$evidence = [ordered]@{
    checkedAtUtc = [DateTime]::UtcNow.ToString('o'); environment = $Environment; port = $Port
    hardwareId = 'CAFE:403F'; flashRequested = [bool]$Flash
    flashHashVerified = [bool]($Flash -and $uploadOutput -match 'Hash of data verified')
    webUri = $WebUri; health = $healthContent
}
$evidenceFile = Join-Path $evidencePath ("smoke-{0}.json" -f [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
$evidence | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $evidenceFile -Encoding utf8
Write-Host "Hardware evidence saved: $evidenceFile" -ForegroundColor Green
