#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$QtPrefix,
    [string]$BuildDir = (Join-Path $PSScriptRoot '../build/windows-msvc-x86_64'),
    [string]$MsvcRedistDir = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectDir = Split-Path -Parent $PSScriptRoot
$qtDir = (Resolve-Path -LiteralPath $QtPrefix).Path
$buildRoot = (Resolve-Path -LiteralPath $BuildDir).Path
$packageName = 'gs-232b-open-gui-windows-x86_64'

if ([string]::IsNullOrWhiteSpace($MsvcRedistDir)) {
    if ([string]::IsNullOrWhiteSpace($env:VCToolsRedistDir)) {
        throw 'Run from an x64 VS 2022 developer shell or supply -MsvcRedistDir.'
    }
    $MsvcRedistDir = Join-Path $env:VCToolsRedistDir 'x64/Microsoft.VC143.CRT'
}
$redistDir = (Resolve-Path -LiteralPath $MsvcRedistDir).Path

foreach ($requiredFile in @(
    (Join-Path $buildRoot 'gs_232b_open_gui.exe'),
    (Join-Path $qtDir 'bin/windeployqt.exe'),
    (Join-Path $qtDir 'translations/qtbase_zh_CN.qm'),
    (Join-Path $redistDir 'msvcp140.dll'),
    (Join-Path $redistDir 'vcruntime140.dll'),
    (Join-Path $redistDir 'vcruntime140_1.dll')
)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Missing deployment input: $requiredFile"
    }
}
Get-Command python, objdump -ErrorAction Stop | Out-Null

$stagingRoot = Join-Path $buildRoot ('.windows-package.' + [guid]::NewGuid().ToString('N'))
$stagingPackage = Join-Path $stagingRoot $packageName
$packageDir = Join-Path $buildRoot 'package'
$archivePath = Join-Path $buildRoot "$packageName.zip"

# Recursive cleanup is restricted to these generated children of the resolved build directory.
foreach ($generatedDir in @($stagingRoot, $packageDir)) {
    if ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($generatedDir)) -ne $buildRoot.TrimEnd('\', '/')) {
        throw "Generated directory is outside the build directory: $generatedDir"
    }
}

try {
    New-Item -ItemType Directory -Path $stagingPackage | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildRoot 'gs_232b_open_gui.exe') -Destination $stagingPackage
    # Use only the selected MSVC toolchain's redistributable DLLs, never system DLLs.
    Get-ChildItem -LiteralPath $redistDir -Filter '*.dll' -File |
        Copy-Item -Destination $stagingPackage

    $executable = Join-Path $stagingPackage 'gs_232b_open_gui.exe'
    & (Join-Path $qtDir 'bin/windeployqt.exe') --release --no-compiler-runtime `
        --no-opengl-sw --no-system-d3d-compiler --no-translations `
        --dir $stagingPackage $executable
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }

    @'
[Paths]
Prefix=.
Plugins=.
Translations=translations
'@ | Set-Content -LiteralPath (Join-Path $stagingPackage 'qt.conf') -Encoding utf8NoBOM
    $translationsDir = Join-Path $stagingPackage 'translations'
    New-Item -ItemType Directory -Path $translationsDir -Force | Out-Null
    # main.cpp loads qtbase_zh_CN.qm; the application's own translation is embedded.
    Copy-Item -LiteralPath (Join-Path $qtDir 'translations/qtbase_zh_CN.qm') -Destination $translationsDir
    Copy-Item -LiteralPath (Join-Path $projectDir 'LICENSE') -Destination $stagingPackage
    Copy-Item -LiteralPath (Join-Path $projectDir 'CHANGELOG.md') -Destination $stagingPackage
    Copy-Item -LiteralPath (Join-Path $buildRoot 'licenses') -Destination $stagingPackage -Recurse
    foreach ($licenseFolder in @('LICENSES', 'licenses')) {
        $qtLicenses = Join-Path $qtDir $licenseFolder
        if (Test-Path -LiteralPath $qtLicenses -PathType Container) {
            Copy-Item -LiteralPath $qtLicenses -Destination (Join-Path $stagingPackage 'qt-licenses') -Recurse
            break
        }
    }

    foreach ($deployedFile in @(
        'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Network.dll',
        'Qt6Pdf.dll', 'Qt6SerialPort.dll', 'Qt6WebSockets.dll', 'platforms/qwindows.dll'
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $stagingPackage $deployedFile) -PathType Leaf)) {
            throw "Missing deployed Qt file: $deployedFile"
        }
    }
    & python -X utf8 (Join-Path $PSScriptRoot 'verify-windows-runtime.py') $stagingPackage
    if ($LASTEXITCODE -ne 0) { throw 'Windows package runtime verification failed' }

    # Finish verification and compression before replacing any prior successful output.
    $stagingArchive = Join-Path $stagingRoot "$packageName.zip"
    Compress-Archive -LiteralPath $stagingPackage -DestinationPath $stagingArchive
    if (Test-Path -LiteralPath $packageDir) {
        Remove-Item -LiteralPath $packageDir -Recurse -Force
    }
    Move-Item -LiteralPath $stagingPackage -Destination $packageDir
    Move-Item -LiteralPath $stagingArchive -Destination $archivePath -Force
    Write-Host "Windows package directory: $packageDir"
    Write-Host "Windows ZIP: $archivePath"
}
finally {
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}
