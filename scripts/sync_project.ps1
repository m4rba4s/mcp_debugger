# MCP Debugger Project Sync Script
# Synchronizes project between development machines

param(
    [string]$Mode = "backup",  # backup, restore, sync
    [string]$TargetPath = "",
    [switch]$IncludeBuild = $false,
    [switch]$Compress = $true,
    [switch]$Verbose = $false
)

Write-Host "=== MCP Debugger Project Sync ===" -ForegroundColor Cyan

$ProjectRoot = Split-Path $PSScriptRoot -Parent
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

# Files and directories to include in sync
$IncludePatterns = @(
    "*.cpp", "*.hpp", "*.c", "*.h",
    "*.cmake", "CMakeLists.txt",
    "*.json", "*.md", "*.txt",
    "*.bat", "*.ps1", "*.py",
    "Makefile", "LICENSE"
)

# Directories to exclude
$ExcludeDirectories = @(
    "build", ".git", ".vs", 
    "artifacts", "logs", "reports",
    "*.tmp", "*.log"
)

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    
    if ($Verbose -or $Level -eq "ERROR") {
        $Color = switch ($Level) {
            "ERROR" { "Red" }
            "WARN"  { "Yellow" }
            "INFO"  { "Green" }
            default { "White" }
        }
        Write-Host "[$Level] $Message" -ForegroundColor $Color
    }
}

function Get-ProjectFiles {
    Write-Log "Scanning project files..."
    
    $Files = @()
    
    # Get all source and config files
    foreach ($Pattern in $IncludePatterns) {
        $FoundFiles = Get-ChildItem -Path $ProjectRoot -Recurse -Include $Pattern | Where-Object {
            $RelativePath = $_.FullName.Replace($ProjectRoot, "").TrimStart("\")
            
            # Exclude unwanted directories
            $ShouldExclude = $false
            foreach ($ExcludeDir in $ExcludeDirectories) {
                if ($RelativePath -like "*$ExcludeDir*") {
                    $ShouldExclude = $true
                    break
                }
            }
            
            return -not $ShouldExclude
        }
        
        $Files += $FoundFiles
    }
    
    Write-Log "Found $($Files.Count) files to sync"
    return $Files
}

function Backup-Project {
    param([string]$BackupPath)
    
    if (-not $BackupPath) {
        $BackupPath = "MCP_Backup_$Timestamp"
    }
    
    Write-Host "Creating project backup to: $BackupPath" -ForegroundColor Green
    
    $Files = Get-ProjectFiles
    
    if ($Compress) {
        # Create ZIP archive
        $ZipPath = "$BackupPath.zip"
        
        if (Test-Path $ZipPath) {
            Remove-Item $ZipPath -Force
        }
        
        Write-Log "Creating ZIP archive: $ZipPath"
        
        # Create temporary directory for organization
        $TempDir = Join-Path $env:TEMP "MCP_Temp_$Timestamp"
        New-Item -Path $TempDir -ItemType Directory -Force | Out-Null
        
        foreach ($File in $Files) {
            $RelativePath = $File.FullName.Replace($ProjectRoot, "").TrimStart("\")
            $TargetFile = Join-Path $TempDir $RelativePath
            $TargetDir = Split-Path $TargetFile -Parent
            
            if (!(Test-Path $TargetDir)) {
                New-Item -Path $TargetDir -ItemType Directory -Force | Out-Null
            }
            
            Copy-Item $File.FullName $TargetFile -Force
        }
        
        # Create ZIP
        Compress-Archive -Path "$TempDir\*" -DestinationPath $ZipPath -Force
        
        # Cleanup temp directory
        Remove-Item $TempDir -Recurse -Force
        
        Write-Host "Backup created: $ZipPath" -ForegroundColor Green
        Write-Host "Size: $([math]::Round((Get-Item $ZipPath).Length / 1MB, 2)) MB" -ForegroundColor Cyan
        
    } else {
        # Create directory backup
        if (Test-Path $BackupPath) {
            Remove-Item $BackupPath -Recurse -Force
        }
        
        New-Item -Path $BackupPath -ItemType Directory -Force | Out-Null
        
        foreach ($File in $Files) {
            $RelativePath = $File.FullName.Replace($ProjectRoot, "").TrimStart("\")
            $TargetFile = Join-Path $BackupPath $RelativePath
            $TargetDir = Split-Path $TargetFile -Parent
            
            if (!(Test-Path $TargetDir)) {
                New-Item -Path $TargetDir -ItemType Directory -Force | Out-Null
            }
            
            Copy-Item $File.FullName $TargetFile -Force
        }
        
        Write-Host "Backup created: $BackupPath" -ForegroundColor Green
    }
}

function Restore-Project {
    param([string]$SourcePath)
    
    if (-not $SourcePath -or -not (Test-Path $SourcePath)) {
        Write-Host "Error: Source path not found: $SourcePath" -ForegroundColor Red
        return
    }
    
    Write-Host "Restoring project from: $SourcePath" -ForegroundColor Green
    
    if ($SourcePath.EndsWith(".zip")) {
        # Extract ZIP
        Write-Log "Extracting ZIP archive..."
        
        $TempDir = Join-Path $env:TEMP "MCP_Restore_$Timestamp"
        Expand-Archive -Path $SourcePath -DestinationPath $TempDir -Force
        
        # Copy files to project root
        Copy-Item "$TempDir\*" $ProjectRoot -Recurse -Force
        
        # Cleanup
        Remove-Item $TempDir -Recurse -Force
        
    } else {
        # Copy from directory
        Copy-Item "$SourcePath\*" $ProjectRoot -Recurse -Force
    }
    
    Write-Host "Project restored successfully!" -ForegroundColor Green
    
    # Verify project structure
    Write-Host "Verifying project structure..." -ForegroundColor Cyan
    & "$ProjectRoot\scripts\verify_project.bat"
}

function Sync-WithRemote {
    Write-Host "Remote sync not implemented yet" -ForegroundColor Yellow
    Write-Host "Use backup/restore for now, or set up git sync" -ForegroundColor Yellow
}

# Main execution
switch ($Mode.ToLower()) {
    "backup" {
        Backup-Project -BackupPath $TargetPath
    }
    "restore" {
        Restore-Project -SourcePath $TargetPath
    }
    "sync" {
        Sync-WithRemote
    }
    default {
        Write-Host "Usage: sync_project.ps1 -Mode [backup|restore|sync] -TargetPath <path>" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Examples:" -ForegroundColor Cyan
        Write-Host "  # Create backup to USB drive"
        Write-Host "  .\sync_project.ps1 -Mode backup -TargetPath 'E:\MCP_Backup'"
        Write-Host ""
        Write-Host "  # Restore from backup"
        Write-Host "  .\sync_project.ps1 -Mode restore -TargetPath 'E:\MCP_Backup.zip'"
        Write-Host ""
        Write-Host "  # Quick backup with timestamp"
        Write-Host "  .\sync_project.ps1 -Mode backup"
    }
}