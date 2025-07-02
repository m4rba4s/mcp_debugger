# Main build script for mcp_debugger on Windows
#
# Usage:
#   .\build.ps1 [-InstallDeps] [-Clean] [-Test] [-Config <Debug|Release>]
#
# Parameters:
#   -InstallDeps:  Install required dependencies using vcpkg and winget.
#   -Clean:        Clean the build directory before building.
#   -Test:         Run tests after a successful build.
#   -Config:       Build configuration (Debug or Release). Defaults to Release.
#

[CmdletBinding()]
param (
    [switch]$InstallDeps,
    [switch]$Clean,
    [switch]$Test,
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release'
)

# Script-level variables
$ErrorActionPreference = "Stop"
$ScriptRoot = $PSScriptRoot
$BuildDir = Join-Path $ScriptRoot "build"
$VcpkgDir = Join-Path $ScriptRoot "vcpkg"
$VcpkgUrl = "https://github.com/microsoft/vcpkg.git"

# --- Helper Functions ---

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "---- $Message ----" -ForegroundColor Green
}

function Write-Error-And-Exit {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
    exit 1
}

# --- Core Functions ---

function Install-Dependencies {
    Write-Step "Installing Dependencies"

    # 1. Check for winget
    try {
        Get-Command winget -ErrorAction Stop | Out-Null
        Write-Host "winget is available."
    }
    catch {
        Write-Error-And-Exit "winget not found. Please install it from the Microsoft Store or GitHub."
    }

    # 2. Check for Git
    try {
        Get-Command git -ErrorAction Stop | Out-Null
        Write-Host "Git is available."
    }
    catch {
        Write-Host "Git not found. Attempting to install via winget..."
        winget install -e --id Git.Git
    }

    # 3. Check for CMake
    try {
        Get-Command cmake -ErrorAction Stop | Out-Null
        Write-Host "CMake is available."
    }
    catch {
        Write-Host "CMake not found. Attempting to install via winget..."
        winget install -e --id Kitware.CMake
    }
    
    # 4. Check for Visual Studio Build Tools
    # This is hard to automate. We'll check for vswhere.
    try {
        $vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
        if (-not $vsPath) { throw }
        Write-Host "Visual Studio seems to be installed at: $vsPath"
    }
    catch {
        Write-Error-And-Exit "Visual Studio with C++ Build Tools not found. Please install it manually: https://visualstudio.microsoft.com/visual-cpp-build-tools/"
    }

    # 5. Install vcpkg
    if (-not (Test-Path -Path $VcpkgDir)) {
        Write-Host "vcpkg not found. Cloning repository..."
        git clone $VcpkgUrl $VcpkgDir
    } else {
        Write-Host "vcpkg directory already exists."
    }

    # Bootstrap vcpkg
    if (-not (Test-Path (Join-Path $VcpkgDir "vcpkg.exe"))) {
        & (Join-Path $VcpkgDir "bootstrap-vcpkg.bat") -disableMetrics
    }

    # 6. Install required libraries
    Write-Host "Installing required libraries via vcpkg (this may take a while)..."
    $vcpkgExe = Join-Path $VcpkgDir "vcpkg.exe"
    # Add libraries we'll need soon
    & $vcpkgExe install nlohmann-json cpp-httplib gtest --triplet x64-windows
    
    Write-Host "Dependency installation complete." -ForegroundColor Green
}

function Invoke-Clean {
    Write-Step "Cleaning Build Directory"
    if (Test-Path -Path $BuildDir) {
        Write-Host "Removing old build directory: $BuildDir"
        Remove-Item -Recurse -Force $BuildDir
    } else {
        Write-Host "Build directory not found. Nothing to clean."
    }
}

function Invoke-Build {
    Write-Step "Building Project (Configuration: $Config)"

    $vcpkgToolchainFile = Join-Path $VcpkgDir "scripts/buildsystems/vcpkg.cmake"
    if (-not (Test-Path $vcpkgToolchainFile)) {
        Write-Error-And-Exit "vcpkg toolchain file not found. Did you run with -InstallDeps?"
    }

    # CMake Configure
    Write-Host "Configuring with CMake..."
    $cmake_args = @(
        "-B", $BuildDir,
        "-S", $ScriptRoot,
        "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchainFile"
    )
    & cmake $cmake_args
    if ($LASTEXITCODE -ne 0) {
        Write-Error-And-Exit "CMake configuration failed."
    }

    # CMake Build
    Write-Host "Building with CMake..."
    $build_args = @(
        "--build", $BuildDir,
        "--config", $Config
    )
    & cmake $build_args
    if ($LASTEXITCODE -ne 0) {
        Write-Error-And-Exit "CMake build failed."
    }

    Write-Host "Build completed successfully." -ForegroundColor Green
}

function Invoke-Tests {
    Write-Step "Running Tests"
    
    if (-not (Test-Path $BuildDir)) {
        Write-Error-And-Exit "Build directory not found. Please build the project first."
    }

    Write-Host "Executing CTest..."
    $ctest_args = @(
        "--test-dir", $BuildDir,
        "--config", $Config,
        "--output-on-failure"
    )

    try {
        & ctest $ctest_args
        if ($LASTEXITCODE -ne 0) {
            # ctest returns non-zero for failed tests, which is not a script error
            Write-Host "CTest finished. Some tests may have failed (see output above)." -ForegroundColor Yellow
        } else {
            Write-Host "All tests passed successfully." -ForegroundColor Green
        }
    } catch {
        Write-Error-And-Exit "Failed to execute ctest. Ensure it is in your PATH."
    }
}


# --- Main Logic ---

if ($InstallDeps) {
    Install-Dependencies
}

if ($Clean) {
    Invoke-Clean
}

if ($PSBoundParameters.ContainsKey('Build') -and !$Build) {
    # If -Build:$false is passed, do nothing more.
    Write-Host "Build skipped as per user request."
} else {
    Invoke-Build

    if ($Test) {
        Invoke-Tests
    }
}

Write-Host ""
Write-Host "Script finished." 