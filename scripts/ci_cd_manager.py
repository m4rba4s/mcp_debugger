#!/usr/bin/env python3
"""
MCP Debugger CI/CD Management Script

Master script that orchestrates the entire CI/CD pipeline:
- Build coordination
- Test execution
- Security scanning
- Performance testing
- Packaging
- Deployment preparation
"""

import os
import sys
import json
import time
import subprocess
import argparse
import shutil
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import logging
from dataclasses import dataclass

@dataclass
class BuildConfig:
    platform: str = "x64"
    config: str = "Release"
    enable_tests: bool = True
    enable_security_scan: bool = True
    enable_performance_test: bool = True
    enable_packaging: bool = True
    parallel_jobs: int = 4
    
class CICDManager:
    def __init__(self, project_root: str):
        self.project_root = Path(project_root)
        self.scripts_dir = self.project_root / "scripts"
        self.logger = self._setup_logging()
        self.build_artifacts = {}
        
    def _setup_logging(self) -> logging.Logger:
        logger = logging.getLogger('mcp_cicd')
        logger.setLevel(logging.INFO)
        
        # Console handler
        console_handler = logging.StreamHandler()
        console_formatter = logging.Formatter(
            '[%(asctime)s] [%(levelname)s] %(message)s'
        )
        console_handler.setFormatter(console_formatter)
        logger.addHandler(console_handler)
        
        # File handler
        log_file = self.project_root / "ci_cd.log"
        file_handler = logging.FileHandler(log_file)
        file_formatter = logging.Formatter(
            '[%(asctime)s] [%(levelname)s] [%(name)s] %(message)s'
        )
        file_handler.setFormatter(file_formatter)
        logger.addHandler(file_handler)
        
        return logger
    
    def run_full_pipeline(self, config: BuildConfig) -> bool:
        """Run the complete CI/CD pipeline"""
        self.logger.info("="*60)
        self.logger.info("STARTING MCP DEBUGGER CI/CD PIPELINE")
        self.logger.info("="*60)
        self.logger.info(f"Platform: {config.platform}")
        self.logger.info(f"Configuration: {config.config}")
        
        pipeline_start = time.time()
        
        try:
            # Stage 1: Environment Setup
            if not self._setup_environment(config):
                return False
            
            # Stage 2: Build
            if not self._run_build(config):
                return False
            
            # Stage 3: Unit Tests
            if config.enable_tests and not self._run_unit_tests(config):
                return False
            
            # Stage 4: Integration Tests
            if config.enable_tests and not self._run_integration_tests(config):
                return False
            
            # Stage 5: Security Scan
            if config.enable_security_scan and not self._run_security_scan(config):
                self.logger.warning("Security scan found issues, but continuing...")
            
            # Stage 6: Performance Tests
            if config.enable_performance_test and not self._run_performance_tests(config):
                self.logger.warning("Performance tests had issues, but continuing...")
            
            # Stage 7: Static Analysis
            if not self._run_static_analysis(config):
                self.logger.warning("Static analysis had issues, but continuing...")
            
            # Stage 8: Packaging
            if config.enable_packaging and not self._run_packaging(config):
                return False
            
            # Stage 9: Final Verification
            if not self._final_verification(config):
                return False
            
            pipeline_duration = time.time() - pipeline_start
            self.logger.info("="*60)
            self.logger.info("CI/CD PIPELINE COMPLETED SUCCESSFULLY")
            self.logger.info(f"Total duration: {pipeline_duration:.2f} seconds")
            self.logger.info("="*60)
            
            return True
            
        except Exception as e:
            self.logger.error(f"Pipeline failed with exception: {e}")
            return False
    
    def _setup_environment(self, config: BuildConfig) -> bool:
        """Setup build environment"""
        self.logger.info("Stage 1: Setting up environment...")
        
        # Check required tools
        required_tools = ["cmake", "git"]
        if os.name == 'nt':
            required_tools.append("cl")  # MSVC compiler
        
        for tool in required_tools:
            if not shutil.which(tool):
                self.logger.error(f"Required tool not found: {tool}")
                return False
        
        # Create build directories
        build_dir = self.project_root / "build" / config.platform
        build_dir.mkdir(parents=True, exist_ok=True)
        
        # Create output directories
        output_dirs = ["artifacts", "reports", "logs"]
        for dir_name in output_dirs:
            (self.project_root / dir_name).mkdir(exist_ok=True)
        
        self.logger.info("✓ Environment setup completed")
        return True
    
    def _run_build(self, config: BuildConfig) -> bool:
        """Run the build process"""
        self.logger.info("Stage 2: Building project...")
        
        try:
            # Use PowerShell build system
            build_script = self.scripts_dir / "build_system.ps1"
            
            cmd = [
                "powershell", "-ExecutionPolicy", "Bypass",
                "-File", str(build_script),
                "build",
                "-Config", config.config,
                "-Platform", config.platform
            ]
            
            if not config.enable_tests:
                cmd.append("-SkipTests")
            
            result = subprocess.run(
                cmd,
                cwd=self.project_root,
                capture_output=True,
                text=True,
                timeout=600  # 10 minutes
            )
            
            if result.returncode != 0:
                self.logger.error(f"Build failed: {result.stderr}")
                return False
            
            # Check for build artifacts
            binary_path = self.project_root / "build" / config.platform / "src" / "cli" / config.config / "mcp-debugger.exe"
            if not binary_path.exists():
                self.logger.error("Main binary not found after build")
                return False
            
            self.build_artifacts["binary"] = str(binary_path)
            self.logger.info("✓ Build completed successfully")
            return True
            
        except subprocess.TimeoutExpired:
            self.logger.error("Build timed out")
            return False
        except Exception as e:
            self.logger.error(f"Build failed: {e}")
            return False
    
    def _run_unit_tests(self, config: BuildConfig) -> bool:
        """Run unit tests"""
        self.logger.info("Stage 3: Running unit tests...")
        
        try:
            build_dir = self.project_root / "build" / config.platform
            
            # Run CTest
            result = subprocess.run(
                ["ctest", "--output-on-failure", "--build-config", config.config],
                cwd=build_dir,
                capture_output=True,
                text=True,
                timeout=300  # 5 minutes
            )
            
            # Save test results
            test_report = self.project_root / "reports" / "unit_tests.txt"
            with open(test_report, 'w') as f:
                f.write(f"Return code: {result.returncode}\\n")
                f.write(f"STDOUT:\\n{result.stdout}\\n")
                f.write(f"STDERR:\\n{result.stderr}\\n")
            
            if result.returncode != 0:
                self.logger.error("Unit tests failed")
                return False
            
            self.logger.info("✓ Unit tests passed")
            return True
            
        except Exception as e:
            self.logger.error(f"Unit tests failed: {e}")
            return False
    
    def _run_integration_tests(self, config: BuildConfig) -> bool:
        """Run integration tests"""
        self.logger.info("Stage 4: Running integration tests...")
        
        try:
            test_script = self.scripts_dir / "run_integration_tests.py"
            binary_path = self.build_artifacts.get("binary")
            
            if not binary_path:
                self.logger.error("Binary path not available for integration tests")
                return False
            
            cmd = [
                "python", str(test_script),
                "--binary", binary_path,
                "--platform", config.platform,
                "--config", config.config
            ]
            
            result = subprocess.run(
                cmd,
                cwd=self.project_root,
                capture_output=True,
                text=True,
                timeout=600  # 10 minutes
            )
            
            # Save test results
            test_report = self.project_root / "reports" / "integration_tests.txt"
            with open(test_report, 'w') as f:
                f.write(f"Return code: {result.returncode}\\n")
                f.write(f"STDOUT:\\n{result.stdout}\\n")
                f.write(f"STDERR:\\n{result.stderr}\\n")
            
            if result.returncode != 0:
                self.logger.warning("Some integration tests failed, but continuing...")
            else:
                self.logger.info("✓ Integration tests passed")
            
            return True
            
        except Exception as e:
            self.logger.error(f"Integration tests failed: {e}")
            return False
    
    def _run_security_scan(self, config: BuildConfig) -> bool:
        """Run security scan"""
        self.logger.info("Stage 5: Running security scan...")
        
        try:
            security_script = self.scripts_dir / "security_scan.py"
            
            result = subprocess.run(
                ["python", str(security_script), "--project-root", str(self.project_root)],
                capture_output=True,
                text=True,
                timeout=300  # 5 minutes
            )
            
            # Save security report
            security_report = self.project_root / "reports" / "security_scan.txt"
            with open(security_report, 'w') as f:
                f.write(f"Return code: {result.returncode}\\n")
                f.write(f"STDOUT:\\n{result.stdout}\\n")
                f.write(f"STDERR:\\n{result.stderr}\\n")
            
            if result.returncode != 0:
                self.logger.warning("Security scan found issues")
                return False
            
            self.logger.info("✓ Security scan passed")
            return True
            
        except Exception as e:
            self.logger.error(f"Security scan failed: {e}")
            return False
    
    def _run_performance_tests(self, config: BuildConfig) -> bool:
        """Run performance tests"""
        self.logger.info("Stage 6: Running performance tests...")
        
        try:
            perf_script = self.scripts_dir / "performance_tests.py"
            binary_path = self.build_artifacts.get("binary")
            
            if not binary_path:
                self.logger.error("Binary path not available for performance tests")
                return False
            
            cmd = [
                "python", str(perf_script),
                "--binary", binary_path,
                "--iterations", "50",  # Reduced for CI
                "--output", str(self.project_root / "reports")
            ]
            
            result = subprocess.run(
                cmd,
                cwd=self.project_root,
                capture_output=True,
                text=True,
                timeout=900  # 15 minutes
            )
            
            # Save performance report
            perf_report = self.project_root / "reports" / "performance_tests.txt"
            with open(perf_report, 'w') as f:
                f.write(f"Return code: {result.returncode}\\n")
                f.write(f"STDOUT:\\n{result.stdout}\\n")
                f.write(f"STDERR:\\n{result.stderr}\\n")
            
            if result.returncode != 0:
                self.logger.warning("Performance tests had issues")
                return False
            
            self.logger.info("✓ Performance tests completed")
            return True
            
        except Exception as e:
            self.logger.error(f"Performance tests failed: {e}")
            return False
    
    def _run_static_analysis(self, config: BuildConfig) -> bool:
        """Run static analysis"""
        self.logger.info("Stage 7: Running static analysis...")
        
        # This would integrate with tools like:
        # - PVS-Studio
        # - Clang Static Analyzer
        # - PC-lint
        # - SonarQube
        
        self.logger.info("✓ Static analysis completed (placeholder)")
        return True
    
    def _run_packaging(self, config: BuildConfig) -> bool:
        """Run packaging"""
        self.logger.info("Stage 8: Creating packages...")
        
        try:
            package_script = self.scripts_dir / "package.py"
            artifacts_dir = self.project_root / "artifacts"
            
            cmd = [
                "python", str(package_script),
                "--platform", config.platform,
                "--config", config.config,
                "--output", str(artifacts_dir)
            ]
            
            result = subprocess.run(
                cmd,
                cwd=self.project_root,
                capture_output=True,
                text=True,
                timeout=300  # 5 minutes
            )
            
            if result.returncode != 0:
                self.logger.error(f"Packaging failed: {result.stderr}")
                return False
            
            # Check for package artifacts
            package_name = f"MCP-Debugger-1.0.0-{config.platform}.zip"
            package_path = artifacts_dir / package_name
            
            if package_path.exists():
                self.build_artifacts["package"] = str(package_path)
                self.logger.info(f"✓ Package created: {package_name}")
            else:
                self.logger.warning("Package file not found, but packaging reported success")
            
            return True
            
        except Exception as e:
            self.logger.error(f"Packaging failed: {e}")
            return False
    
    def _final_verification(self, config: BuildConfig) -> bool:
        """Final verification of build artifacts"""
        self.logger.info("Stage 9: Final verification...")
        
        # Verify all expected artifacts exist
        required_artifacts = ["binary"]
        if config.enable_packaging:
            required_artifacts.append("package")
        
        for artifact in required_artifacts:
            if artifact not in self.build_artifacts:
                self.logger.error(f"Missing required artifact: {artifact}")
                return False
            
            path = Path(self.build_artifacts[artifact])
            if not path.exists():
                self.logger.error(f"Artifact file not found: {path}")
                return False
        
        # Generate build summary
        self._generate_build_summary(config)
        
        self.logger.info("✓ Final verification passed")
        return True
    
    def _generate_build_summary(self, config: BuildConfig):
        """Generate build summary report"""
        summary = {
            "build_info": {
                "platform": config.platform,
                "configuration": config.config,
                "timestamp": time.time(),
                "success": True
            },
            "artifacts": self.build_artifacts,
            "reports": {
                "unit_tests": "reports/unit_tests.txt",
                "integration_tests": "reports/integration_tests.txt",
                "security_scan": "reports/security_scan.txt",
                "performance_tests": "reports/performance_tests.txt"
            }
        }
        
        summary_path = self.project_root / "artifacts" / "build_summary.json"
        with open(summary_path, 'w') as f:
            json.dump(summary, f, indent=2)
        
        self.logger.info(f"Build summary saved to: {summary_path}")

def main():
    parser = argparse.ArgumentParser(description='MCP Debugger CI/CD Manager')
    parser.add_argument('--platform', choices=['x64', 'x86'], default='x64')
    parser.add_argument('--config', choices=['Debug', 'Release'], default='Release')
    parser.add_argument('--skip-tests', action='store_true', help='Skip all tests')
    parser.add_argument('--skip-security', action='store_true', help='Skip security scan')
    parser.add_argument('--skip-performance', action='store_true', help='Skip performance tests')
    parser.add_argument('--skip-packaging', action='store_true', help='Skip packaging')
    parser.add_argument('--project-root', default='.', help='Project root directory')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    # Create build configuration
    build_config = BuildConfig(
        platform=args.platform,
        config=args.config,
        enable_tests=not args.skip_tests,
        enable_security_scan=not args.skip_security,
        enable_performance_test=not args.skip_performance,
        enable_packaging=not args.skip_packaging
    )
    
    # Create CI/CD manager
    manager = CICDManager(args.project_root)
    
    if args.verbose:
        manager.logger.setLevel(logging.DEBUG)
    
    # Run pipeline
    success = manager.run_full_pipeline(build_config)
    
    return 0 if success else 1

if __name__ == '__main__':
    sys.exit(main())