# Static Analysis Script for MCP Debugger
# Runs multiple static analysis tools

[CmdletBinding()]
param (
    [switch]$ClangTidy,
    [switch]$CppCheck,
    [switch]$MSAnalyze,
    [switch]$All,
    [string]$OutputDir = "static-analysis-results"
)

$ErrorActionPreference = "Continue"
$ScriptRoot = $PSScriptRoot

# Create output directory
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

function Write-Section {
    param([string]$Title)
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host $Title -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
}

function Test-Command {
    param([string]$Command)
    try {
        Get-Command $Command -ErrorAction Stop | Out-Null
        return $true
    } catch {
        return $false
    }
}

# Check available tools
$toolsAvailable = @{
    'clang-tidy' = Test-Command 'clang-tidy'
    'cppcheck' = Test-Command 'cppcheck'
    'msbuild' = Test-Command 'msbuild'
}

Write-Host "Available Static Analysis Tools:" -ForegroundColor Cyan
foreach ($tool in $toolsAvailable.Keys) {
    $status = if ($toolsAvailable[$tool]) { "[+] Available" } else { "[-] Not Found" }
    $color = if ($toolsAvailable[$tool]) { "Green" } else { "Red" }
    Write-Host "  ${tool}: $status" -ForegroundColor $color
}

# Run Clang-Tidy Analysis
if (($ClangTidy -or $All) -and $toolsAvailable['clang-tidy']) {
    Write-Section "Running Clang-Tidy Analysis"
    
    $outputFile = Join-Path $OutputDir "clang-tidy-results.txt"
    
    try {
        # Generate compile_commands.json if not exists
        if (-not (Test-Path "build/compile_commands.json")) {
            Write-Host "Generating compile_commands.json..." -ForegroundColor Yellow
            cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        }
        
        Write-Host "Running clang-tidy analysis..." -ForegroundColor Blue
        
        # Find all source files
        $sourceFiles = Get-ChildItem -Path "src" -Include "*.cpp", "*.hpp" -Recurse | 
                      Where-Object { $_.FullName -notmatch "vcpkg|third_party" }
        
        $clangTidyResults = @()
        
        foreach ($file in $sourceFiles) {
            Write-Host "Analyzing: $($file.Name)" -ForegroundColor Gray
            
            $result = clang-tidy $file.FullName -p=build --format-style=file 2>&1
            if ($result) {
                $clangTidyResults += "=== $($file.FullName) ==="
                $clangTidyResults += $result
                $clangTidyResults += ""
            }
        }
        
        $clangTidyResults | Out-File -FilePath $outputFile -Encoding UTF8
        Write-Host "Clang-Tidy results saved to: $outputFile" -ForegroundColor Green
        
        # Show summary
        $warningCount = ($clangTidyResults | Select-String "warning:" | Measure-Object).Count
        $errorCount = ($clangTidyResults | Select-String "error:" | Measure-Object).Count
        
        Write-Host "Clang-Tidy Summary:" -ForegroundColor Cyan
        Write-Host "  Errors: $errorCount" -ForegroundColor $(if ($errorCount -gt 0) { "Red" } else { "Green" })
        Write-Host "  Warnings: $warningCount" -ForegroundColor $(if ($warningCount -gt 0) { "Yellow" } else { "Green" })
        
    } catch {
        Write-Host "Error running clang-tidy: $_" -ForegroundColor Red
    }
}

# Run CPPCheck Analysis
if (($CppCheck -or $All) -and $toolsAvailable['cppcheck']) {
    Write-Section "Running CPPCheck Analysis"
    
    $outputFile = Join-Path $OutputDir "cppcheck-results.txt"
    $xmlOutputFile = Join-Path $OutputDir "cppcheck-results.xml"
    
    try {
        Write-Host "Running cppcheck analysis..." -ForegroundColor Blue
        
        $cppCheckArgs = @(
            "--config-file=cppcheck.cfg"
            "--xml"
            "--xml-version=2"
            "src/"
        )
        
        # Run cppcheck and capture output
        $cppCheckOutput = & cppcheck $cppCheckArgs 2>&1
        
        # Save results
        $cppCheckOutput | Out-File -FilePath $outputFile -Encoding UTF8
        $cppCheckOutput | Out-File -FilePath $xmlOutputFile -Encoding UTF8
        
        Write-Host "CPPCheck results saved to: $outputFile" -ForegroundColor Green
        
        # Show summary
        $errorCount = ($cppCheckOutput | Select-String "\[error\]" | Measure-Object).Count
        $warningCount = ($cppCheckOutput | Select-String "\[warning\]" | Measure-Object).Count
        $styleCount = ($cppCheckOutput | Select-String "\[style\]" | Measure-Object).Count
        
        Write-Host "CPPCheck Summary:" -ForegroundColor Cyan
        Write-Host "  Errors: $errorCount" -ForegroundColor $(if ($errorCount -gt 0) { "Red" } else { "Green" })
        Write-Host "  Warnings: $warningCount" -ForegroundColor $(if ($warningCount -gt 0) { "Yellow" } else { "Green" })
        Write-Host "  Style Issues: $styleCount" -ForegroundColor $(if ($styleCount -gt 0) { "Yellow" } else { "Green" })
        
    } catch {
        Write-Host "Error running cppcheck: $_" -ForegroundColor Red
    }
}

# Run Microsoft Code Analysis
if (($MSAnalyze -or $All) -and $toolsAvailable['msbuild']) {
    Write-Section "Running Microsoft Code Analysis"
    
    $outputFile = Join-Path $OutputDir "ms-analysis-results.txt"
    
    try {
        Write-Host "Running Microsoft Code Analysis..." -ForegroundColor Blue
        
        # Build with code analysis enabled
        $buildOutput = msbuild build\mcp_debugger.sln /p:Configuration=Release /p:EnableCodeAnalysis=true /p:CodeAnalysisRuleSet=AllRules.ruleset /verbosity:normal 2>&1
        
        $buildOutput | Out-File -FilePath $outputFile -Encoding UTF8
        
        Write-Host "Microsoft Analysis results saved to: $outputFile" -ForegroundColor Green
        
        # Show summary
        $warningCount = ($buildOutput | Select-String "warning CA" | Measure-Object).Count
        $errorCount = ($buildOutput | Select-String "error CA" | Measure-Object).Count
        
        Write-Host "Microsoft Analysis Summary:" -ForegroundColor Cyan
        Write-Host "  CA Errors: $errorCount" -ForegroundColor $(if ($errorCount -gt 0) { "Red" } else { "Green" })
        Write-Host "  CA Warnings: $warningCount" -ForegroundColor $(if ($warningCount -gt 0) { "Yellow" } else { "Green" })
        
    } catch {
        Write-Host "Error running Microsoft Code Analysis: $_" -ForegroundColor Red
    }
}

# Manual Code Review Checklist
Write-Section "Manual Code Review Checklist"

$checklistFile = Join-Path $OutputDir "manual-review-checklist.md"

# Create checklist content
$checklistContent = @'
# Manual Code Review Checklist for MCP Debugger

## Security Review
- [ ] No hardcoded credentials or API keys
- [ ] Input validation on all external inputs
- [ ] Proper bounds checking for arrays/vectors
- [ ] No buffer overflows in C-style operations
- [ ] Secure string handling (no strcpy, strcat)
- [ ] Memory management is RAII-compliant
- [ ] No use-after-free vulnerabilities
- [ ] Thread-safe operations where needed

## Performance Review  
- [ ] No unnecessary memory allocations in hot paths
- [ ] Efficient algorithm choices (O(n) vs O(n²))
- [ ] Proper use of move semantics
- [ ] Avoiding copy operations where possible
- [ ] String operations are efficient
- [ ] Container operations use optimal methods
- [ ] Caching where appropriate

## Maintainability Review
- [ ] Functions are reasonably sized (<150 lines)
- [ ] Classes have single responsibility
- [ ] Proper error handling and logging
- [ ] Clear variable and function names
- [ ] Adequate comments for complex logic
- [ ] Consistent coding style
- [ ] Unit tests for critical functionality

## Architecture Review
- [ ] Proper separation of concerns
- [ ] Minimal coupling between modules
- [ ] Clear interfaces and abstractions
- [ ] Dependency injection where appropriate
- [ ] Proper use of design patterns
- [ ] Extensible architecture
- [ ] Configuration management

## Thread Safety Review
- [ ] All shared state is properly synchronized
- [ ] No race conditions in multi-threaded code
- [ ] Proper use of atomic operations
- [ ] Deadlock prevention measures
- [ ] Exception safety in concurrent code

## Resource Management Review
- [ ] All resources are properly managed (RAII)
- [ ] No memory leaks
- [ ] Proper cleanup in destructors
- [ ] Exception-safe resource handling
- [ ] File handles are properly closed
- [ ] Network connections are properly managed
'@

$checklistContent += "`n`nGenerated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$checklistContent | Out-File -FilePath $checklistFile -Encoding UTF8

Write-Host "Manual review checklist saved to: $checklistFile" -ForegroundColor Green

# Summary
Write-Section "Static Analysis Complete"

Write-Host "Results Directory: $OutputDir" -ForegroundColor Green
Write-Host ""
Write-Host "Generated Files:" -ForegroundColor Cyan

if (Test-Path $OutputDir) {
    Get-ChildItem -Path $OutputDir | ForEach-Object {
        $size = if ($_.Length -gt 1024) { "{0:N1} KB" -f ($_.Length / 1024) } else { "$($_.Length) bytes" }
        Write-Host "  $($_.Name) ($size)" -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "1. Review all generated reports" -ForegroundColor White
Write-Host "2. Address critical errors and warnings" -ForegroundColor White
Write-Host "3. Complete manual review checklist" -ForegroundColor White
Write-Host "4. Update code and re-run analysis" -ForegroundColor White 