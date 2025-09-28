<# 
  scripts/build-run.ps1

  Usage examples:
    # Visual Studio generator (default), clean build, init DB, run server:
    .\scripts\build-run.ps1 -Clean -Init -Serve

    # Ninja (must have Ninja & MSVC env; use Developer PowerShell):
    .\scripts\build-run.ps1 -Generator ninja -BuildType Debug -Clean

    # Custom vcpkg location and DB/port:
    .\scripts\build-run.ps1 -VcpkgRoot "C:\dev\vcpkg" -DbPath ".\data\mission-metadata.db" -Port 9090 -Init -Serve
#>

[CmdletBinding()]
param(
  [ValidateSet('vs','ninja')]
  [string]$Generator = 'vs',

  [ValidateSet('Debug','Release')]
  [string]$BuildType = 'Release',

  [string]$VcpkgRoot,                      # e.g., C:\dev\vcpkg or .\external\vcpkg
  [string]$DbPath = "data\mission-metadata.db",
  [int]$Port = 8080,

  [switch]$Clean,
  [switch]$Init,
  [switch]$Serve
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Resolve-CMake {
  $candidates = @(
    "C:\Program Files\CMake\bin\cmake.exe",
    "C:\Program Files (x86)\CMake\bin\cmake.exe",
    "cmake"
  )
  foreach ($c in $candidates) {
    try { return (Get-Command $c -ErrorAction Stop).Source } catch {}
  }
  throw "CMake not found. Install it or add it to PATH."
}

function Resolve-VcpkgRoot {
  param([string]$Given)
  if ($Given) { return (Resolve-Path $Given).Path }
  if ($env:VCPKG_ROOT) { return (Resolve-Path $env:VCPKG_ROOT).Path }
  if (Test-Path ".\external\vcpkg") { return (Resolve-Path ".\external\vcpkg").Path }
  if (Test-Path "C:\dev\vcpkg") { return "C:\dev\vcpkg" }
  throw "Could not find vcpkg. Pass -VcpkgRoot, set VCPKG_ROOT, or clone to .\external\vcpkg or C:\dev\vcpkg."
}

$CMake     = Resolve-CMake
$RepoRoot  = (Resolve-Path ".").Path
$VcpkgRoot = Resolve-VcpkgRoot -Given:$VcpkgRoot
$Toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"

if (-not (Test-Path $Toolchain)) {
  throw "vcpkg toolchain not found: $Toolchain"
}

# Use generator-specific build folders to avoid cache clashes
$BuildDir = if ($Generator -eq 'vs') { "build-vs" } else { "build-ninja" }
$ExeDir   = if ($Generator -eq 'vs') { Join-Path $BuildDir $BuildType } else { $BuildDir }

if ($Clean -and (Test-Path $BuildDir)) {
  Write-Host "Cleaning build directory: $BuildDir"
  Remove-Item -Recurse -Force $BuildDir
}

# Avoid inheriting MSYS/MinGW compilers: clear CC/CXX for MSVC selection
Remove-Item Env:CC,Env:CXX,Env:CFLAGS,Env:CXXFLAGS -ErrorAction SilentlyContinue

Write-Host "Generator: $Generator"
Write-Host "Build dir: $BuildDir"
Write-Host "Exe dir  : $ExeDir"
Write-Host "Toolchain: $Toolchain"

# ----- Configure -----
if ($Generator -eq 'vs') {
  & $CMake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="$Toolchain" `
    -DVCPKG_TARGET_TRIPLET=x64-windows `
    -DVCPKG_MANIFEST_MODE=ON
} else {
  # Ninja path: must have Ninja installed and MSVC toolchain available
  try { Get-Command ninja -ErrorAction Stop | Out-Null } catch { 
    throw "Ninja not found. Install it (choco install ninja) or use -Generator vs."
  }
  # Force MSVC with Ninja (avoid picking MSYS2 GCC)
  & $CMake -S . -B $BuildDir -G Ninja `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -DCMAKE_TOOLCHAIN_FILE="$Toolchain" `
    -DVCPKG_TARGET_TRIPLET=x64-windows `
    -DVCPKG_MANIFEST_MODE=ON `
    -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe
}

# ----- Build -----
if ($Generator -eq 'vs') {
  & $CMake --build $BuildDir --config $BuildType
} else {
  & $CMake --build $BuildDir
}

# ----- Ensure schema next to the binary -----
$SchemaSrc = Join-Path $RepoRoot "src\core\metadata\schema.sql"
$SchemaDst = Join-Path $ExeDir    "schema.sql"
if (-not (Test-Path (Split-Path -Parent $SchemaDst))) {
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $SchemaDst) | Out-Null
}
Copy-Item -Force $SchemaSrc $SchemaDst

# ----- Optional: initialize DB (idempotent) -----
if ($Init) {
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $DbPath) | Out-Null
  $env:MDM_DB_PATH = (Resolve-Path $DbPath).Path
  Push-Location $ExeDir
  try {
    Write-Host "Initializing DB at $env:MDM_DB_PATH ..."
    .\mdm.exe --init
  } finally {
    Pop-Location
  }
}

# ----- Optional: run server -----
if ($Serve) {
  $env:MDM_DB_PATH = (Resolve-Path $DbPath).Path
  $env:MDM_PORT    = "$Port"
  Push-Location $ExeDir
  try {
    Write-Host "Starting server on http://localhost:$Port (Ctrl+C to stop)..."
    .\mdm.exe --serve
  } finally {
    Pop-Location
  }
} else {
  Write-Host "Done."
  Write-Host "To run the server now:"
  Write-Host "  `$env:MDM_DB_PATH = `"$((Resolve-Path $DbPath).Path)`""
  Write-Host "  `$env:MDM_PORT = `"$Port`""
  Write-Host "  & `"$ExeDir\mdm.exe`" --serve"
}
