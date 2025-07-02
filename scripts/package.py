#!/usr/bin/env python3
"""
MCP Debugger Packaging Script

Creates distributable packages for Windows including:
- Standalone executable with dependencies
- x64dbg plugin
- Configuration templates
- Documentation
- Installation scripts
"""

import os
import sys
import shutil
import zipfile
import json
import argparse
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, List, Optional
import logging

# =======================================
# Package Configuration
# =======================================
PACKAGE_CONFIG = {
    "name": "MCP-Debugger",
    "version": "1.0.0",
    "description": "Multi-Context Prompt Debugging Utility",
    "author": "MCP Team",
    "license": "MIT",
    "supported_platforms": ["x64", "x86"],
    "required_files": [
        "mcp-debugger.exe",
        "config.json",
        "README.md",
        "LICENSE"
    ],
    "optional_files": [
        "mcp_debugger.dp64",  # x64dbg plugin
        "mcp_debugger.dp32",  # x32dbg plugin
    ]
}

class PackageBuilder:
    def __init__(self, platform: str, config: str, output_dir: str):
        self.platform = platform
        self.config = config
        self.output_dir = Path(output_dir)
        self.project_root = Path(__file__).parent.parent
        self.build_dir = self.project_root / "build" / platform
        self.logger = self._setup_logging()
        
        # Package info
        self.package_name = f"{PACKAGE_CONFIG['name']}-{PACKAGE_CONFIG['version']}-{platform}"
        self.package_dir = self.output_dir / self.package_name
        
    def _setup_logging(self) -> logging.Logger:
        logger = logging.getLogger('mcp_package')
        logger.setLevel(logging.INFO)
        
        handler = logging.StreamHandler()
        formatter = logging.Formatter('[%(levelname)s] %(message)s')
        handler.setFormatter(formatter)
        logger.addHandler(handler)
        
        return logger
    
    def create_package(self) -> bool:
        """Create the complete package"""
        try:
            self.logger.info(f"Creating package: {self.package_name}")
            
            # Create package directory
            self.package_dir.mkdir(parents=True, exist_ok=True)
            
            # Copy binaries and dependencies
            self._copy_binaries()
            
            # Copy configuration files
            self._copy_configs()
            
            # Copy documentation
            self._copy_documentation()
            
            # Create installation scripts
            self._create_install_scripts()
            
            # Create plugin package
            self._create_plugin_package()
            
            # Create manifests
            self._create_manifests()
            
            # Create ZIP archive
            zip_path = self._create_zip_archive()
            
            # Verify package
            self._verify_package()
            
            self.logger.info(f"Package created successfully: {zip_path}")
            return True
            
        except Exception as e:
            self.logger.error(f"Package creation failed: {e}")
            return False
    
    def _copy_binaries(self):
        """Copy main executables and dependencies"""
        self.logger.info("Copying binaries...")
        
        bin_dir = self.package_dir / "bin"
        bin_dir.mkdir(exist_ok=True)
        
        # Main executable
        exe_path = self.build_dir / "src" / "cli" / self.config / "mcp-debugger.exe"
        if exe_path.exists():
            shutil.copy2(exe_path, bin_dir)
            self.logger.info(f"Copied: {exe_path.name}")
        else:
            raise FileNotFoundError(f"Main executable not found: {exe_path}")
        
        # Copy runtime dependencies (DLLs)
        self._copy_runtime_dependencies(bin_dir)
        
        # x64dbg plugins
        plugin_dir = self.package_dir / "plugins"
        plugin_dir.mkdir(exist_ok=True)
        
        plugin_suffix = "dp64" if self.platform == "x64" else "dp32"
        plugin_path = self.build_dir / "src" / "x64dbg" / self.config / f"mcp_debugger.{plugin_suffix}"
        
        if plugin_path.exists():
            shutil.copy2(plugin_path, plugin_dir)
            self.logger.info(f"Copied plugin: {plugin_path.name}")
    
    def _copy_runtime_dependencies(self, bin_dir: Path):
        """Copy required runtime DLLs"""
        # Visual C++ Redistributables are usually system-installed
        # Copy any other required DLLs here
        
        # Check for libcurl, OpenSSL, etc. if statically linked, skip this
        dependencies = [
            # "libcurl.dll",
            # "openssl.dll",
        ]
        
        for dll in dependencies:
            # Search common locations for dependencies
            search_paths = [
                self.build_dir / "src" / "cli" / self.config,
                Path("C:/Windows/System32"),
                # Add vcpkg or other dependency paths
            ]
            
            for search_path in search_paths:
                dll_path = search_path / dll
                if dll_path.exists():
                    shutil.copy2(dll_path, bin_dir)
                    self.logger.info(f"Copied dependency: {dll}")
                    break
    
    def _copy_configs(self):
        """Copy configuration files and templates"""
        self.logger.info("Copying configuration files...")
        
        config_dir = self.package_dir / "config"
        config_dir.mkdir(exist_ok=True)
        
        # Sample configuration
        sample_config = self.project_root / "sample-config.json"
        if sample_config.exists():
            shutil.copy2(sample_config, config_dir / "config.json")
        
        # Create default configuration if sample doesn't exist
        else:
            default_config = {
                "api_configs": {
                    "claude": {
                        "model": "claude-3-sonnet-20240229",
                        "endpoint": "https://api.anthropic.com/v1/messages",
                        "timeout_ms": 30000,
                        "max_retries": 3,
                        "validate_ssl": True
                    }
                },
                "debug_config": {
                    "x64dbg_path": "C:\\x64dbg\\release\\x64\\x64dbg.exe",
                    "auto_connect": False,
                    "connection_timeout_ms": 5000
                },
                "log_config": {
                    "level": 1,
                    "output_path": "mcp-debugger.log",
                    "console_output": True,
                    "file_output": True
                },
                "security_config": {
                    "credential_store_path": "credentials.encrypted",
                    "require_api_key_validation": True,
                    "encrypt_credentials": True
                }
            }
            
            with open(config_dir / "config.json", 'w') as f:
                json.dump(default_config, f, indent=2)
    
    def _copy_documentation(self):
        """Copy documentation files"""
        self.logger.info("Copying documentation...")
        
        docs_dir = self.package_dir / "docs"
        docs_dir.mkdir(exist_ok=True)
        
        # Copy documentation files
        doc_files = [
            ("README.md", "README.md"),
            ("README-WINDOWS.md", "WINDOWS-SETUP.md"),
            ("CLAUDE.md", "ARCHITECTURE.md"),
            ("LICENSE", "LICENSE"),
        ]
        
        for src_name, dst_name in doc_files:
            src_path = self.project_root / src_name
            if src_path.exists():
                shutil.copy2(src_path, docs_dir / dst_name)
            else:
                self.logger.warning(f"Documentation file not found: {src_name}")
        
        # Copy demo scripts
        scripts_src = self.project_root / "scripts"
        scripts_dst = docs_dir / "examples"
        scripts_dst.mkdir(exist_ok=True)
        
        demo_files = ["demo-session.mcp"]
        for demo_file in demo_files:
            demo_path = scripts_src / demo_file
            if demo_path.exists():
                shutil.copy2(demo_path, scripts_dst)
    
    def _create_install_scripts(self):
        """Create installation and setup scripts"""
        self.logger.info("Creating installation scripts...")
        
        # Windows batch installer
        install_script = f"""@echo off
REM MCP Debugger Installation Script
echo Installing MCP Debugger {PACKAGE_CONFIG['version']}...

REM Create installation directory
set INSTALL_DIR=%ProgramFiles%\\MCPDebugger
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

REM Copy files
echo Copying files...
xcopy /E /I /Y bin "%INSTALL_DIR%\\bin\\"
xcopy /E /I /Y config "%INSTALL_DIR%\\config\\"
xcopy /E /I /Y docs "%INSTALL_DIR%\\docs\\"

REM Add to PATH
echo Adding to PATH...
setx PATH "%PATH%;%INSTALL_DIR%\\bin" /M

REM Install x64dbg plugin
set PLUGIN_SRC=%~dp0plugins\\mcp_debugger.{plugin_suffix}
echo.
echo To install x64dbg plugin:
echo 1. Copy %PLUGIN_SRC% to your x64dbg\\plugins\\ directory
echo 2. Restart x64dbg
echo.

REM Create shortcuts
echo Creating shortcuts...
set DESKTOP=%USERPROFILE%\\Desktop
powershell "$WshShell = New-Object -comObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%DESKTOP%\\MCP Debugger.lnk'); $Shortcut.TargetPath = '%INSTALL_DIR%\\bin\\mcp-debugger.exe'; $Shortcut.Save()"

echo.
echo Installation complete!
echo Run 'mcp-debugger --help' to get started.
echo.
pause
"""
        
        plugin_suffix = "dp64" if self.platform == "x64" else "dp32"
        install_script = install_script.format(plugin_suffix=plugin_suffix)
        
        with open(self.package_dir / "install.bat", 'w') as f:
            f.write(install_script)
        
        # PowerShell installer (more advanced)
        ps_script = f'''# MCP Debugger PowerShell Installer
param(
    [string]$InstallPath = "$env:ProgramFiles\\MCPDebugger",
    [switch]$AddToPath = $true,
    [switch]$CreateShortcuts = $true
)

Write-Host "Installing MCP Debugger {PACKAGE_CONFIG['version']}..." -ForegroundColor Cyan

# Check if running as administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {{
    Write-Host "Warning: Not running as Administrator. Some features may not work." -ForegroundColor Yellow
}}

# Create installation directory
if (!(Test-Path $InstallPath)) {{
    New-Item -Path $InstallPath -ItemType Directory -Force | Out-Null
}}

# Copy files
Write-Host "Copying files to $InstallPath..." -ForegroundColor Green
Copy-Item -Path "bin\\*" -Destination "$InstallPath\\bin\\" -Recurse -Force
Copy-Item -Path "config\\*" -Destination "$InstallPath\\config\\" -Recurse -Force
Copy-Item -Path "docs\\*" -Destination "$InstallPath\\docs\\" -Recurse -Force

# Add to PATH
if ($AddToPath) {{
    Write-Host "Adding to system PATH..." -ForegroundColor Green
    $currentPath = [Environment]::GetEnvironmentVariable("PATH", "Machine")
    if ($currentPath -notlike "*$InstallPath\\bin*") {{
        [Environment]::SetEnvironmentVariable("PATH", "$currentPath;$InstallPath\\bin", "Machine")
    }}
}}

# Create shortcuts
if ($CreateShortcuts) {{
    Write-Host "Creating desktop shortcut..." -ForegroundColor Green
    $WshShell = New-Object -comObject WScript.Shell
    $Shortcut = $WshShell.CreateShortcut("$env:USERPROFILE\\Desktop\\MCP Debugger.lnk")
    $Shortcut.TargetPath = "$InstallPath\\bin\\mcp-debugger.exe"
    $Shortcut.WorkingDirectory = "$InstallPath\\bin"
    $Shortcut.Save()
}}

Write-Host "Installation complete!" -ForegroundColor Green
Write-Host "Run 'mcp-debugger --help' to get started." -ForegroundColor Cyan
'''
        
        with open(self.package_dir / "install.ps1", 'w', encoding='utf-8') as f:
            f.write(ps_script)
    
    def _create_plugin_package(self):
        """Create separate x64dbg plugin package"""
        plugin_dir = self.package_dir / "x64dbg-plugin"
        plugin_dir.mkdir(exist_ok=True)
        
        plugin_suffix = "dp64" if self.platform == "x64" else "dp32"
        plugin_src = self.package_dir / "plugins" / f"mcp_debugger.{plugin_suffix}"
        
        if plugin_src.exists():
            shutil.copy2(plugin_src, plugin_dir)
            
            # Create plugin README
            plugin_readme = f"""# MCP Debugger x64dbg Plugin

## Installation
1. Copy `mcp_debugger.{plugin_suffix}` to your x64dbg `plugins` directory
2. Restart x64dbg
3. The plugin will be automatically loaded

## Usage
Use MCP commands directly in x64dbg command line:
- `mcp help` - Show available commands
- `mcp llm "analyze this function"` - Send prompt to LLM
- `mcp connect` - Connect to external MCP instance

## Configuration
Edit the MCP configuration file in the x64dbg directory or use the standalone application to configure API keys.
"""
            
            with open(plugin_dir / "README.md", 'w') as f:
                f.write(plugin_readme)
    
    def _create_manifests(self):
        """Create package manifests and metadata"""
        self.logger.info("Creating package manifests...")
        
        # Package manifest
        manifest = {
            "name": PACKAGE_CONFIG["name"],
            "version": PACKAGE_CONFIG["version"],
            "platform": self.platform,
            "config": self.config,
            "description": PACKAGE_CONFIG["description"],
            "author": PACKAGE_CONFIG["author"],
            "license": PACKAGE_CONFIG["license"],
            "created": str(Path().absolute()),
            "files": [],
            "dependencies": {
                "visual_cpp_redist": "2019+",
                "windows": "10+",
                "x64dbg": "2023+" if "x64" in self.platform else "2023+"
            }
        }
        
        # Enumerate all package files
        for root, dirs, files in os.walk(self.package_dir):
            for file in files:
                file_path = Path(root) / file
                rel_path = file_path.relative_to(self.package_dir)
                manifest["files"].append({
                    "path": str(rel_path),
                    "size": file_path.stat().st_size
                })
        
        with open(self.package_dir / "manifest.json", 'w') as f:
            json.dump(manifest, f, indent=2)
        
        # Version info
        version_info = {
            "version": PACKAGE_CONFIG["version"],
            "build_date": str(Path().absolute()),
            "platform": self.platform,
            "config": self.config,
            "git_hash": self._get_git_hash(),
            "build_info": {
                "compiler": "MSVC",
                "cmake_version": self._get_cmake_version(),
                "build_type": self.config
            }
        }
        
        with open(self.package_dir / "version.json", 'w') as f:
            json.dump(version_info, f, indent=2)
    
    def _get_git_hash(self) -> str:
        """Get current git commit hash"""
        try:
            result = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                capture_output=True,
                text=True,
                cwd=self.project_root
            )
            if result.returncode == 0:
                return result.stdout.strip()
        except:
            pass
        return "unknown"
    
    def _get_cmake_version(self) -> str:
        """Get CMake version"""
        try:
            result = subprocess.run(
                ["cmake", "--version"],
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                return result.stdout.split('\\n')[0]
        except:
            pass
        return "unknown"
    
    def _create_zip_archive(self) -> Path:
        """Create ZIP archive of the package"""
        self.logger.info("Creating ZIP archive...")
        
        zip_path = self.output_dir / f"{self.package_name}.zip"
        
        with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            for root, dirs, files in os.walk(self.package_dir):
                for file in files:
                    file_path = Path(root) / file
                    arc_name = file_path.relative_to(self.package_dir)
                    zipf.write(file_path, arc_name)
        
        self.logger.info(f"Archive created: {zip_path} ({zip_path.stat().st_size // 1024} KB)")
        return zip_path
    
    def _verify_package(self):
        """Verify package completeness"""
        self.logger.info("Verifying package...")
        
        required_files = [
            "bin/mcp-debugger.exe",
            "config/config.json",
            "docs/README.md",
            "manifest.json",
            "version.json",
            "install.bat",
            "install.ps1"
        ]
        
        missing_files = []
        for req_file in required_files:
            file_path = self.package_dir / req_file
            if not file_path.exists():
                missing_files.append(req_file)
        
        if missing_files:
            self.logger.warning(f"Missing files: {missing_files}")
        else:
            self.logger.info("Package verification passed")

def main():
    parser = argparse.ArgumentParser(description='MCP Debugger Package Creator')
    parser.add_argument('--platform', choices=['x64', 'x86'], default='x64',
                        help='Target platform')
    parser.add_argument('--config', choices=['Debug', 'Release'], default='Release',
                        help='Build configuration')
    parser.add_argument('--output', required=True,
                        help='Output directory for packages')
    parser.add_argument('--verbose', action='store_true',
                        help='Verbose output')
    
    args = parser.parse_args()
    
    # Create package builder
    builder = PackageBuilder(args.platform, args.config, args.output)
    
    if args.verbose:
        builder.logger.setLevel(logging.DEBUG)
    
    # Create package
    success = builder.create_package()
    
    return 0 if success else 1

if __name__ == '__main__':
    sys.exit(main())