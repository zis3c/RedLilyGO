param(
    [string]$Environment = "LILYGO-T-Dongle-S3"
)

$ErrorActionPreference = "Stop"

function Assert-NativeSuccess([string]$Step) {
    if ($LASTEXITCODE -ne 0) { throw "$Step failed with exit code $LASTEXITCODE." }
}

Write-Host "Building dashboard UI..."
python .\build_ui.py
Assert-NativeSuccess 'Dashboard UI build'

Write-Host "Checking RedLilyGO UI contract..."
foreach ($check in Get-ChildItem .\tests -Filter '*_contract.mjs' | Sort-Object Name) {
    node $check.FullName
    Assert-NativeSuccess $check.Name
}

Write-Host "Checking Remote UI JavaScript syntax..."
node --check .\ui\src\core\navigation.js
Assert-NativeSuccess 'Remote UI syntax check'
node --check .\ui\content\assets\js\ui.js
Assert-NativeSuccess 'Generated UI syntax check'

Write-Host "Running host regression checks..."
python .\tests\firmware_regressions.py
Assert-NativeSuccess 'Host firmware regressions'
python .\tests\macro_server_regressions.py
Assert-NativeSuccess 'Macro server regressions'
python .\tests\build_assets_regressions.py
Assert-NativeSuccess 'Build asset regressions'
python .\tests\preview_server_regressions.py
Assert-NativeSuccess 'Preview server regressions'
python .\tests\static_analysis.py --strict
Assert-NativeSuccess 'Static analysis'
Push-Location .\tests
try {
    python .\coverage.py --min 80
    Assert-NativeSuccess 'Host coverage'
} finally {
    Pop-Location
}
& .\tests\release_package_regressions.ps1

Write-Host "Running desktop and mobile browser checks..."
Push-Location .\ui
try {
    npm run test:e2e
    Assert-NativeSuccess 'Browser checks'
} finally {
    Pop-Location
}

Write-Host "Building Windows Agent..."
dotnet build .\tools\Agent\Agent.csproj --configuration Release
Assert-NativeSuccess 'Agent build'

Write-Host "Running Agent protocol checks..."
dotnet run --project .\tools\Agent.Tests\Agent.Tests.csproj --configuration Release
Assert-NativeSuccess 'Agent protocol checks'

Write-Host "Building firmware for $Environment..."
python -m platformio run -e $Environment
Assert-NativeSuccess 'Firmware build'

Write-Host "Verification complete."
