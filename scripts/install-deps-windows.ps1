# MCP Debugger Dependencies Installation Script for Windows
# Run as Administrator

param(
    [switch]$SkipChoco = $false
)

Write-Host "=== MCP Debugger Dependencies Installer ===" -ForegroundColor Cyan

# Check if running as Administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "Error: This script must be run as Administrator" -ForegroundColor Red
    Write-Host "Right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    exit 1
}

# Install Chocolatey if not present
if (-not $SkipChoco -and -not (Get-Command choco -ErrorAction SilentlyContinue)) {
    Write-Host "Installing Chocolatey package manager..." -ForegroundColor Yellow
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
}

# Install dependencies
Write-Host "Installing dependencies..." -ForegroundColor Yellow

# Essential build tools
choco install cmake --yes
choco install git --yes

# Visual Studio Build Tools (if not already installed)
if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    Write-Host "Installing Visual Studio Build Tools..." -ForegroundColor Yellow
    choco install visualstudio2019buildtools --yes
    choco install visualstudio2019-workload-vctools --yes
}

# Optional: x64dbg for testing
$installX64dbg = Read-Host "Install x64dbg for testing? (y/N)"
if ($installX64dbg -eq 'y' -or $installX64dbg -eq 'Y') {
    choco install x64dbg.portable --yes
}

Write-Host "`n=== Installation Complete ===" -ForegroundColor Green
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Open a new Command Prompt or PowerShell window" -ForegroundColor White
Write-Host "2. Navigate to the MCP project directory" -ForegroundColor White
Write-Host "3. Run: build-windows.bat" -ForegroundColor White
Write-Host "`nNote: You may need to restart your computer for PATH changes to take effect." -ForegroundColor Yellow

Read-Host "Press Enter to exit"