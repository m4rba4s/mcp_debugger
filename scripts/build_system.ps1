# MCP Debugger Advanced Build System
# PowerShell Core 7+ required for cross-platform compatibility

param(
    [Parameter(Position=0)]
    [ValidateSet("build", "test", "package", "clean", "install", "analyze")]
    [string]$Action = "build",
    
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Release",
    
    [ValidateSet("x64", "x86")]
    [string]$Platform = "x64",
    
    [switch]$SkipTests,
    [switch]$SkipPackaging,
    [switch]$Verbose,
    [switch]$ParallelBuild = $true,
    [string]$OutputDir = "dist",
    [string]$LogFile = "build.log"
)

# =======================================
# Configuration & Constants
# =======================================
$Script:Config = @{
    ProjectName = "MCP-Debugger"
    Version = "1.0.0"
    BuildDir = "build"
    SourceDir = "src"
    TestDir = "tests"
    ArtifactsDir = $OutputDir
    CMakeGenerator = "Visual Studio 17 2022"
    ParallelJobs = [Environment]::ProcessorCount
}

$Script:Paths = @{
    Root = $PSScriptRoot | Split-Path -Parent
    Build = Join-Path $Script:Config.BuildDir $Platform
    Artifacts = $Script:Config.ArtifactsDir
    Scripts = Join-Path $PSScriptRoot ""
}

# =======================================
# Logging & Utilities
# =======================================
function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logMessage = "[$timestamp] [$Level] $Message"
    
    switch ($Level) {
        "ERROR" { Write-Host $logMessage -ForegroundColor Red }
        "WARN"  { Write-Host $logMessage -ForegroundColor Yellow }
        "INFO"  { Write-Host $logMessage -ForegroundColor Green }
        "DEBUG" { if ($Verbose) { Write-Host $logMessage -ForegroundColor Gray } }
    }
    
    Add-Content -Path $LogFile -Value $logMessage
}

function Invoke-Command-Safe {
    param([string]$Command, [string]$ErrorMessage = "Command failed")
    
    Write-Log "Executing: $Command" "DEBUG"
    
    $result = Invoke-Expression $Command
    if ($LASTEXITCODE -ne 0) {
        Write-Log "$ErrorMessage. Exit code: $LASTEXITCODE" "ERROR"
        throw $ErrorMessage
    }
    
    return $result
}

function Test-Prerequisites {
    Write-Log "Checking build prerequisites..." "INFO"
    
    # Check CMake
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw "CMake not found. Please install CMake 3.16+"
    }
    
    # Check Visual Studio
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsInstalls = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
        if (-not $vsInstalls) {
            throw "Visual Studio with C++ tools not found"
        }
        Write-Log "Found Visual Studio: $($vsInstalls[0].displayName)" "INFO"
    }
    
    # Check Python for scripts
    if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
        Write-Log "Python not found. Some scripts may not work." "WARN"
    }
    
    Write-Log "Prerequisites check passed" "INFO"
}

# =======================================
# Build Functions
# =======================================
function Invoke-Configure {
    Write-Log "Configuring CMake for $Platform $Config..." "INFO"
    
    $buildPath = Join-Path $Script:Paths.Root $Script:Paths.Build
    New-Item -Path $buildPath -ItemType Directory -Force | Out-Null
    
    $cmakeArgs = @(
        "-B", $buildPath
        "-A", $Platform
        "-DCMAKE_BUILD_TYPE=$Config"
        "-DBUILD_TESTS=ON"
        "-DBUILD_PLUGIN=ON"
        "-DBUILD_GUI=OFF"
    )
    
    if ($Verbose) {
        $cmakeArgs += "--debug-output"
    }
    
    Push-Location $Script:Paths.Root
    try {
        Invoke-Command-Safe "cmake $($cmakeArgs -join ' ')" "CMake configuration failed"
    }
    finally {
        Pop-Location
    }
}

function Invoke-Build {
    Write-Log "Building $($Script:Config.ProjectName) ($Platform $Config)..." "INFO"
    
    $buildPath = Join-Path $Script:Paths.Root $Script:Paths.Build
    
    $buildArgs = @(
        "--build", $buildPath
        "--config", $Config
    )
    
    if ($ParallelBuild) {
        $buildArgs += "--parallel", $Script:Config.ParallelJobs
    }
    
    if ($Verbose) {
        $buildArgs += "--verbose"
    }
    
    Invoke-Command-Safe "cmake $($buildArgs -join ' ')" "Build failed"
    
    Write-Log "Build completed successfully" "INFO"
}

function Invoke-Test {
    if ($SkipTests) {
        Write-Log "Skipping tests (--SkipTests specified)" "INFO"
        return
    }
    
    Write-Log "Running tests..." "INFO"
    
    $buildPath = Join-Path $Script:Paths.Root $Script:Paths.Build
    
    Push-Location $buildPath
    try {
        # Run CTest
        $testArgs = @(
            "--output-on-failure"
            "--build-config", $Config
            "--parallel", $Script:Config.ParallelJobs
        )
        
        if ($Verbose) {
            $testArgs += "--verbose"
        }
        
        Invoke-Command-Safe "ctest $($testArgs -join ' ')" "Tests failed"
        
        # Run Python integration tests if available
        $integrationScript = Join-Path $Script:Paths.Scripts "run_integration_tests.py"
        if (Test-Path $integrationScript) {
            Write-Log "Running integration tests..." "INFO"
            python $integrationScript --platform $Platform --config $Config
        }
        
    }
    finally {
        Pop-Location
    }
    
    Write-Log "All tests passed" "INFO"
}

function Invoke-StaticAnalysis {
    Write-Log "Running static analysis..." "INFO"
    
    # TODO: Integrate with PVS-Studio, Clang Static Analyzer, or similar
    $analysisScript = Join-Path $Script:Paths.Scripts "static_analysis.py"
    if (Test-Path $analysisScript) {
        python $analysisScript --target $Script:Paths.Build
    } else {
        Write-Log "Static analysis script not found, skipping..." "WARN"
    }
}

function Invoke-Package {
    if ($SkipPackaging) {
        Write-Log "Skipping packaging (--SkipPackaging specified)" "INFO"
        return
    }
    
    Write-Log "Creating packages..." "INFO"
    
    $artifactsPath = Join-Path $Script:Paths.Root $Script:Paths.Artifacts
    New-Item -Path $artifactsPath -ItemType Directory -Force | Out-Null
    
    # Package using Python script
    $packageScript = Join-Path $Script:Paths.Scripts "package.py"
    if (Test-Path $packageScript) {
        python $packageScript --platform $Platform --config $Config --output $artifactsPath
    } else {
        # Fallback: manual packaging
        Write-Log "Package script not found, creating basic package..." "WARN"
        
        $packageName = "$($Script:Config.ProjectName)-$($Script:Config.Version)-$Platform"
        $packagePath = Join-Path $artifactsPath $packageName
        
        New-Item -Path $packagePath -ItemType Directory -Force | Out-Null
        
        # Copy binaries
        $buildPath = Join-Path $Script:Paths.Root $Script:Paths.Build
        $binariesPath = Join-Path $buildPath "src\cli\$Config"
        if (Test-Path $binariesPath) {
            Copy-Item "$binariesPath\*" $packagePath -Recurse
        }
        
        # Copy plugin
        $pluginPath = Join-Path $buildPath "src\x64dbg\$Config"
        if (Test-Path $pluginPath) {
            Copy-Item "$pluginPath\*.dp64" $packagePath
        }
        
        # Create ZIP
        $zipPath = "$packagePath.zip"
        Compress-Archive -Path $packagePath -DestinationPath $zipPath -Force
        
        Write-Log "Package created: $zipPath" "INFO"
    }
}

function Invoke-Clean {
    Write-Log "Cleaning build artifacts..." "INFO"
    
    $pathsToClean = @(
        $Script:Paths.Build,
        $Script:Paths.Artifacts,
        (Join-Path $Script:Paths.Root "build"),
        (Join-Path $Script:Paths.Root "dist")
    )
    
    foreach ($path in $pathsToClean) {
        $fullPath = Join-Path $Script:Paths.Root $path
        if (Test-Path $fullPath) {
            Remove-Item $fullPath -Recurse -Force
            Write-Log "Removed: $fullPath" "INFO"
        }
    }
}

function Invoke-Install {
    Write-Log "Installing $($Script:Config.ProjectName)..." "INFO"
    
    # TODO: Implement system installation
    # - Copy to Program Files
    # - Register x64dbg plugin
    # - Create shortcuts
    # - Update PATH if needed
    
    Write-Log "Installation not yet implemented" "WARN"
}

# =======================================
# Main Execution
# =======================================
function Main {
    Write-Log "Starting MCP Debugger Build System" "INFO"
    Write-Log "Action: $Action, Config: $Config, Platform: $Platform" "INFO"
    
    try {
        Test-Prerequisites
        
        switch ($Action) {
            "build" {
                Invoke-Configure
                Invoke-Build
                if (-not $SkipTests) { Invoke-Test }
                if (-not $SkipPackaging) { Invoke-Package }
            }
            "test" {
                Invoke-Test
            }
            "package" {
                Invoke-Package
            }
            "clean" {
                Invoke-Clean
            }
            "install" {
                Invoke-Install
            }
            "analyze" {
                Invoke-StaticAnalysis
            }
        }
        
        Write-Log "Build system completed successfully" "INFO"
        exit 0
        
    }
    catch {
        Write-Log "Build system failed: $_" "ERROR"
        exit 1
    }
}

# Run if script is executed directly
if ($MyInvocation.InvocationName -ne '.') {
    Main
}