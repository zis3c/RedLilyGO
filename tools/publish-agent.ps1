param(
    [ValidateSet("win-x64", "win-arm64")]
    [string]$Runtime = "win-x64"
)

$ErrorActionPreference = "Stop"
$output = Join-Path (Get-Location) "dist\agent-$Runtime"

if (Test-Path -LiteralPath $output) {
    Remove-Item -LiteralPath $output -Recurse -Force
}

dotnet publish .\tools\Agent\Agent.csproj `
    --configuration Release `
    --runtime $Runtime `
    --self-contained true `
    -p:PublishAot=false `
    -p:PublishSingleFile=true `
    -p:IncludeNativeLibrariesForSelfExtract=true `
    -p:DebugType=None `
    --output $output

if ($LASTEXITCODE -ne 0) {
    throw "Agent publish failed with exit code $LASTEXITCODE."
}

Write-Host "Agent package created at $output"
