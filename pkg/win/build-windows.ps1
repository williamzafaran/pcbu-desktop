<#
.SYNOPSIS
    Build pcbu_desktop Windows installer (x64).

.DESCRIPTION
    Requires:
      - Visual Studio 2022 with "Desktop development with C++" workload
      - CMake 3.22+ (bundled with VS or standalone)
      - Qt 6 MSVC2022 x64  (set QT_BASE_DIR)
      - vcpkg             (set VCPKG_ROOT)
      - Inno Setup 6      (ISCC.exe on PATH or at default install location)

    Usage:
        $env:QT_BASE_DIR  = "C:\Qt\6.x.x"
        $env:VCPKG_ROOT   = "C:\vcpkg"
        $env:ARCH         = "x64"          # or arm64
        .\pkg\win\build-windows.ps1

.NOTES
    Run from the repository root, e.g.:
        cd C:\Users\William\Documents\GitHub\pcbu-desktop
        .\pkg\win\build-windows.ps1
#>
param(
    [string]$Arch       = $env:ARCH         ?? "x64",
    [string]$QtBaseDir  = $env:QT_BASE_DIR  ?? "",
    [string]$VcpkgRoot  = $env:VCPKG_ROOT   ?? "",
    [int]   $Cores      = 4
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Validate pre-requisites ──────────────────────────────────────────────────
if (-not $QtBaseDir)  { throw "QT_BASE_DIR is not set. Set it to your Qt installation root, e.g. C:\Qt\6.8.0" }
if (-not $VcpkgRoot)  { throw "VCPKG_ROOT is not set. Set it to your vcpkg clone root, e.g. C:\vcpkg" }
if ($Arch -notin @("x64","arm64")) { throw "ARCH must be 'x64' or 'arm64'" }

$VSArch  = if ($Arch -eq "arm64") { "ARM64" } else { "x64" }

# Locate cmake (prefer VS-bundled, fall back to PATH)
$cmake = Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
if (-not $cmake) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsInstall = & $vswhere -latest -property installationPath
        $cmake = "$vsInstall\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    }
    if (-not (Test-Path $cmake)) { throw "cmake.exe not found. Install CMake or ensure Visual Studio 2022 is installed." }
}
Write-Host "Using cmake: $cmake"

# Locate ISCC (Inno Setup compiler)
$iscc = Get-Command iscc -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
if (-not $iscc) {
    $iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    if (-not (Test-Path $iscc)) { throw "ISCC.exe not found. Install Inno Setup 6." }
}
Write-Host "Using ISCC: $iscc"

# Locate Windows SDK mt.exe
$winKitsBase = "C:\Program Files (x86)\Windows Kits\10\bin"
$mtExe = Get-ChildItem $winKitsBase -Filter "mt.exe" -Recurse -ErrorAction SilentlyContinue |
         Where-Object { $_.FullName -match "x64" } | Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not $mtExe) { throw "mt.exe not found in Windows SDK. Ensure Windows 10 SDK is installed." }
Write-Host "Using mt.exe: $mtExe"

$repoRoot  = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
$buildDir  = "$repoRoot\pkg\build"
$winQtPath = "$QtBaseDir\msvc2022_64"

Write-Host "`n=== pcbu_desktop Windows Build (v3.2.5, $Arch) ===`n"

# ── Step 1: Build win-pcbiounlock native DLL (static link) ──────────────────
Write-Host "[1/4] Building win-pcbiounlock (static link)..."
New-Item -ItemType Directory -Force $buildDir | Out-Null
Push-Location $buildDir
try {
    & $cmake $repoRoot `
        -DCMAKE_BUILD_TYPE=Release `
        -DTARGET_ARCH=$Arch `
        -DQT_BASE_DIR="$QtBaseDir" `
        -G "Visual Studio 17 2022" `
        -A $VSArch `
        -DCMAKE_GENERATOR_PLATFORM=$VSArch `
        -DMSVC_STATIC_LINK=1
    if ($LASTEXITCODE -ne 0) { throw "CMake configure (static) failed." }

    & $cmake --build . --target "win-pcbiounlock" --config Release -- /maxcpucount:$Cores
    if ($LASTEXITCODE -ne 0) { throw "win-pcbiounlock build failed." }
} finally { Pop-Location }

# ── Step 2: Build pcbu_desktop ───────────────────────────────────────────────
Write-Host "`n[2/4] Building pcbu_desktop..."
Remove-Item "$buildDir\*" -Recurse -Force
Push-Location $buildDir
try {
    & $cmake $repoRoot `
        -DCMAKE_BUILD_TYPE=Release `
        -DTARGET_ARCH=$Arch `
        -DQT_BASE_DIR="$QtBaseDir" `
        -G "Visual Studio 17 2022" `
        -A $VSArch `
        -DCMAKE_GENERATOR_PLATFORM=$VSArch
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

    & $cmake --build . --target "pcbu_desktop" --config Release -- /maxcpucount:$Cores
    if ($LASTEXITCODE -ne 0) { throw "pcbu_desktop build failed." }
} finally { Pop-Location }

# ── Step 3: Package with windeployqt + manifest ──────────────────────────────
Write-Host "`n[3/4] Packaging..."
$installerDir = "$buildDir\installer_dir"
New-Item -ItemType Directory -Force $installerDir | Out-Null

Copy-Item "$buildDir\desktop\Release\pcbu_desktop.exe" $installerDir
& $mtExe -manifest "$PSScriptRoot\requireAdmin.manifest" -outputresource:"$installerDir\pcbu_desktop.exe"

& "$winQtPath\bin\windeployqt.exe" --qmldir "$repoRoot\desktop\qml" "$installerDir\pcbu_desktop.exe"
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

# ── Step 4: Inno Setup installer ─────────────────────────────────────────────
Write-Host "`n[4/4] Building installer..."
$env:ARCH = $Arch
Push-Location $buildDir
try {
    & $iscc "$PSScriptRoot\installer.iss"
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed." }
    Rename-Item "mysetup.exe" "PCBioUnlock-Setup-$Arch.exe" -Force
} finally { Pop-Location }

Write-Host "`n✅ Done! Installer: $buildDir\PCBioUnlock-Setup-$Arch.exe`n"
