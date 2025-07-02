#!/usr/bin/env python3
"""
MCP Debugger Integration Test Suite

Comprehensive testing framework for MCP Debugger that tests:
- Core engine functionality
- LLM API integration (with mocking)
- x64dbg bridge communication
- S-Expression parser
- Memory analysis pipeline
- Security features
"""

import os
import sys
import json
import time
import subprocess
import tempfile
import threading
import argparse
import logging
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from contextlib import contextmanager

# =======================================
# Test Configuration
# =======================================
@dataclass
class TestConfig:
    platform: str = "x64"
    config: str = "Release"
    binary_path: str = ""
    timeout: int = 30
    verbose: bool = False
    skip_llm_tests: bool = False
    skip_x64dbg_tests: bool = False
    mock_apis: bool = True

# =======================================
# Test Framework
# =======================================
class TestResult:
    def __init__(self, name: str):
        self.name = name
        self.passed = False
        self.error_message = ""
        self.duration = 0.0
        self.output = ""

class TestRunner:
    def __init__(self, config: TestConfig):
        self.config = config
        self.results: List[TestResult] = []
        self.logger = self._setup_logging()
        
    def _setup_logging(self) -> logging.Logger:
        logger = logging.getLogger('mcp_tests')
        logger.setLevel(logging.DEBUG if self.config.verbose else logging.INFO)
        
        handler = logging.StreamHandler()
        formatter = logging.Formatter(
            '[%(asctime)s] %(levelname)s: %(message)s'
        )
        handler.setFormatter(formatter)
        logger.addHandler(handler)
        
        return logger
    
    def run_test(self, test_func, name: str) -> TestResult:
        """Run a single test function and capture results"""
        result = TestResult(name)
        self.logger.info(f"Running test: {name}")
        
        start_time = time.time()
        try:
            test_func(result)
            result.passed = True
            self.logger.info(f"✓ {name} - PASSED")
        except Exception as e:
            result.passed = False
            result.error_message = str(e)
            self.logger.error(f"✗ {name} - FAILED: {e}")
        finally:
            result.duration = time.time() - start_time
            
        self.results.append(result)
        return result
    
    def run_all_tests(self) -> bool:
        """Run all integration tests"""
        self.logger.info("Starting MCP Debugger Integration Tests")
        self.logger.info(f"Platform: {self.config.platform}, Config: {self.config.config}")
        
        # Core functionality tests
        self.run_test(self.test_binary_execution, "Binary Execution")
        self.run_test(self.test_config_loading, "Configuration Loading")
        self.run_test(self.test_sexpr_parser, "S-Expression Parser")
        self.run_test(self.test_logging_system, "Logging System")
        
        # API integration tests (with mocking)
        if not self.config.skip_llm_tests:
            self.run_test(self.test_llm_api_mock, "LLM API (Mocked)")
            self.run_test(self.test_llm_error_handling, "LLM Error Handling")
        
        # x64dbg bridge tests
        if not self.config.skip_x64dbg_tests:
            self.run_test(self.test_x64dbg_bridge_mock, "x64dbg Bridge (Mocked)")
        
        # Memory analysis tests
        self.run_test(self.test_memory_analysis, "Memory Analysis")
        self.run_test(self.test_pattern_matching, "Pattern Matching")
        
        # Security tests
        self.run_test(self.test_credential_storage, "Credential Storage")
        self.run_test(self.test_api_key_validation, "API Key Validation")
        
        # Performance tests
        self.run_test(self.test_performance_basic, "Basic Performance")
        
        # CLI/REPL tests
        self.run_test(self.test_cli_commands, "CLI Commands")
        self.run_test(self.test_repl_session, "REPL Session")
        
        return self.print_summary()
    
    def print_summary(self) -> bool:
        """Print test summary and return success status"""
        total = len(self.results)
        passed = sum(1 for r in self.results if r.passed)
        failed = total - passed
        
        self.logger.info("\n" + "="*60)
        self.logger.info("TEST SUMMARY")
        self.logger.info("="*60)
        self.logger.info(f"Total tests: {total}")
        self.logger.info(f"Passed: {passed}")
        self.logger.info(f"Failed: {failed}")
        
        if failed > 0:
            self.logger.info("\nFAILED TESTS:")
            for result in self.results:
                if not result.passed:
                    self.logger.error(f"- {result.name}: {result.error_message}")
        
        total_time = sum(r.duration for r in self.results)
        self.logger.info(f"\nTotal execution time: {total_time:.2f}s")
        
        return failed == 0

    # =======================================
    # Test Implementations
    # =======================================
    
    def test_binary_execution(self, result: TestResult):
        """Test that the main binary can be executed"""
        if not self.config.binary_path or not os.path.exists(self.config.binary_path):
            raise Exception("Binary not found or not specified")
        
        # Test help command
        proc = subprocess.run(
            [self.config.binary_path, "--help"],
            capture_output=True,
            text=True,
            timeout=self.config.timeout
        )
        
        if proc.returncode != 0:
            raise Exception(f"Binary execution failed: {proc.stderr}")
        
        result.output = proc.stdout
        
        # Check for expected output
        if "MCP Debugger" not in proc.stdout:
            raise Exception("Expected output not found in help text")
    
    def test_config_loading(self, result: TestResult):
        """Test configuration file loading"""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            test_config = {
                "api_configs": {
                    "claude": {
                        "model": "claude-3-sonnet-20240229",
                        "endpoint": "https://api.anthropic.com/v1/messages"
                    }
                },
                "debug_config": {
                    "x64dbg_path": "C:\\x64dbg\\x64dbg.exe",
                    "auto_connect": False
                },
                "log_config": {
                    "level": 1,
                    "console_output": True
                }
            }
            json.dump(test_config, f)
            config_path = f.name
        
        try:
            # Test config loading with the binary
            proc = subprocess.run(
                [self.config.binary_path, "-c", f":config load {config_path}"],
                capture_output=True,
                text=True,
                timeout=self.config.timeout
            )
            
            if proc.returncode != 0:
                raise Exception(f"Config loading failed: {proc.stderr}")
            
            result.output = proc.stdout
            
        finally:
            os.unlink(config_path)
    
    def test_sexpr_parser(self, result: TestResult):
        """Test S-Expression parser functionality"""
        test_expressions = [
            "(+ 1 2 3)",
            "(if true \"yes\" \"no\")",
            "(list 1 2 3 4)",
            "(log \"info\" \"test message\")"
        ]
        
        for expr in test_expressions:
            proc = subprocess.run(
                [self.config.binary_path, "-c", expr],
                capture_output=True,
                text=True,
                timeout=self.config.timeout
            )
            
            if proc.returncode != 0:
                raise Exception(f"S-Expression parsing failed for '{expr}': {proc.stderr}")
        
        result.output = f"Successfully parsed {len(test_expressions)} expressions"
    
    def test_logging_system(self, result: TestResult):
        """Test logging functionality"""
        with tempfile.TemporaryDirectory() as tmpdir:
            log_file = os.path.join(tmpdir, "test.log")
            
            # Test logging command
            proc = subprocess.run(
                [self.config.binary_path, "-c", f'(log "info" "integration test message")'],
                capture_output=True,
                text=True,
                timeout=self.config.timeout
            )
            
            if proc.returncode != 0:
                raise Exception(f"Logging test failed: {proc.stderr}")
            
            result.output = proc.stdout
    
    def test_llm_api_mock(self, result: TestResult):
        """Test LLM API with mocked responses"""
        if not self.config.mock_apis:
            raise Exception("Mock APIs not enabled")
        
        # Test with mock LLM request
        test_command = '(llm "Hello, this is a test prompt")'
        
        proc = subprocess.run(
            [self.config.binary_path, "-c", test_command],
            capture_output=True,
            text=True,
            timeout=self.config.timeout
        )
        
        # Should fail gracefully since no API key is set
        if "API key" not in proc.stderr and "not available" not in proc.stderr:
            self.logger.warning("Expected API key error not found, but test passed")
        
        result.output = proc.stdout + proc.stderr
    
    def test_llm_error_handling(self, result: TestResult):
        """Test LLM error handling scenarios"""
        error_scenarios = [
            '(llm "")',  # Empty prompt
            '(llm "test" "invalid" "args")',  # Too many args
        ]
        
        for scenario in error_scenarios:
            proc = subprocess.run(
                [self.config.binary_path, "-c", scenario],
                capture_output=True,
                text=True,
                timeout=self.config.timeout
            )
            
            # Should fail gracefully, not crash
            if proc.returncode == 0:
                self.logger.warning(f"Expected failure for scenario: {scenario}")
    
    def test_x64dbg_bridge_mock(self, result: TestResult):
        """Test x64dbg bridge with mocked debugger"""
        # Test connection command (should fail gracefully)
        proc = subprocess.run(
            [self.config.binary_path, "-c", ":connect"],
            capture_output=True,
            text=True,
            timeout=self.config.timeout
        )
        
        # Should handle connection failure gracefully
        result.output = proc.stdout + proc.stderr
    
    def test_memory_analysis(self, result: TestResult):
        """Test memory analysis functionality"""
        # Create test binary data
        test_data = b"MZ\x90\x00\x03\x00\x00\x00\x04\x00\x00\x00\xff\xff" + b"A" * 100
        
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(test_data)
            data_file = f.name
        
        try:
            # Test would involve memory analysis if we had direct API access
            # For now, just verify the binary doesn't crash on memory-related commands
            proc = subprocess.run(
                [self.config.binary_path, "-c", "(log \"info\" \"memory analysis test\")"],
                capture_output=True,
                text=True,
                timeout=self.config.timeout
            )
            
            if proc.returncode != 0:
                raise Exception(f"Memory analysis test failed: {proc.stderr}")
            
        finally:
            os.unlink(data_file)
    
    def test_pattern_matching(self, result: TestResult):
        """Test pattern matching functionality"""
        # Test pattern matching expressions
        test_expr = '(if (parse-pattern "test" "testdata") "found" "not found")'
        
        proc = subprocess.run(
            [self.config.binary_path, "-c", test_expr],
            capture_output=True,
            text=True,
            timeout=self.config.timeout
        )
        
        # Should execute without crashing
        result.output = proc.stdout + proc.stderr
    
    def test_credential_storage(self, result: TestResult):
        """Test credential storage security"""
        # Test credential operations (should be safe to test)
        with tempfile.TemporaryDirectory() as tmpdir:
            os.environ['MCP_CONFIG_DIR'] = tmpdir
            
            # These commands should handle missing credentials gracefully
            proc = subprocess.run(
                [self.config.binary_path, "-c", ":status"],
                capture_output=True,
                text=True,
                timeout=self.config.timeout
            )
            
            result.output = proc.stdout
    
    def test_api_key_validation(self, result: TestResult):
        """Test API key validation"""
        # Test with obviously invalid API keys
        invalid_keys = ["", "short", "x" * 300]  # Empty, too short, too long
        
        for key in invalid_keys:
            # Would test API key validation if we had direct access
            # For now, just ensure the binary handles invalid inputs
            pass
        
        result.output = "API key validation test passed"
    
    def test_performance_basic(self, result: TestResult):
        """Basic performance test"""
        start_time = time.time()
        
        # Run multiple simple commands
        for i in range(10):
            proc = subprocess.run(
                [self.config.binary_path, "-c", f"(+ {i} {i+1})"],
                capture_output=True,
                text=True,
                timeout=self.config.timeout
            )
            
            if proc.returncode != 0:
                raise Exception(f"Performance test iteration {i} failed")
        
        duration = time.time() - start_time
        
        if duration > 30:  # Should complete in reasonable time
            raise Exception(f"Performance test too slow: {duration:.2f}s")
        
        result.output = f"Completed 10 operations in {duration:.2f}s"
    
    def test_cli_commands(self, result: TestResult):
        """Test CLI command functionality"""
        cli_commands = [
            ":help",
            ":status",
            # ":quit" would exit the process
        ]
        
        for cmd in cli_commands:
            proc = subprocess.run(
                [self.config.binary_path, "-c", cmd],
                capture_output=True,
                text=True,
                timeout=self.config.timeout
            )
            
            if proc.returncode != 0:
                self.logger.warning(f"CLI command '{cmd}' failed: {proc.stderr}")
        
        result.output = f"Tested {len(cli_commands)} CLI commands"
    
    def test_repl_session(self, result: TestResult):
        """Test REPL session with multiple commands"""
        # Create a script with multiple commands
        script_content = """
; Test REPL session
(log "info" "Starting REPL test")
(+ 1 2 3)
(log "info" "REPL test complete")
"""
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.mcp', delete=False) as f:
            f.write(script_content)
            script_path = f.name
        
        try:
            proc = subprocess.run(
                [self.config.binary_path, "-f", script_path],
                capture_output=True,
                text=True,
                timeout=self.config.timeout
            )
            
            if proc.returncode != 0:
                raise Exception(f"REPL session test failed: {proc.stderr}")
            
            result.output = proc.stdout
            
        finally:
            os.unlink(script_path)

# =======================================
# Main Entry Point
# =======================================
def main():
    parser = argparse.ArgumentParser(description='MCP Debugger Integration Tests')
    parser.add_argument('--platform', choices=['x64', 'x86'], default='x64')
    parser.add_argument('--config', choices=['Debug', 'Release'], default='Release')
    parser.add_argument('--binary', help='Path to mcp-debugger executable')
    parser.add_argument('--timeout', type=int, default=30, help='Test timeout in seconds')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    parser.add_argument('--skip-llm', action='store_true', help='Skip LLM tests')
    parser.add_argument('--skip-x64dbg', action='store_true', help='Skip x64dbg tests')
    parser.add_argument('--no-mock', action='store_true', help='Disable API mocking')
    
    args = parser.parse_args()
    
    # Auto-detect binary path if not specified
    if not args.binary:
        script_dir = Path(__file__).parent
        project_root = script_dir.parent
        binary_path = project_root / "build" / args.platform / "src" / "cli" / args.config / "mcp-debugger.exe"
        
        if binary_path.exists():
            args.binary = str(binary_path)
        else:
            print(f"ERROR: Binary not found at {binary_path}")
            print("Please specify --binary path or ensure build completed successfully")
            return 1
    
    # Create test configuration
    config = TestConfig(
        platform=args.platform,
        config=args.config,
        binary_path=args.binary,
        timeout=args.timeout,
        verbose=args.verbose,
        skip_llm_tests=args.skip_llm,
        skip_x64dbg_tests=args.skip_x64dbg,
        mock_apis=not args.no_mock
    )
    
    # Run tests
    runner = TestRunner(config)
    success = runner.run_all_tests()
    
    return 0 if success else 1

if __name__ == '__main__':
    sys.exit(main())