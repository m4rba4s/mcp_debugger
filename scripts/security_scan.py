#!/usr/bin/env python3
"""
MCP Debugger Security Scanner

Comprehensive security analysis including:
- Static code analysis for vulnerabilities
- Dependency vulnerability scanning
- API key and credential leak detection
- Binary security features verification
- Network communication analysis
"""

import os
import sys
import re
import json
import subprocess
import hashlib
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import logging

class SecurityScanner:
    def __init__(self, project_root: str):
        self.project_root = Path(project_root)
        self.logger = self._setup_logging()
        self.findings = []
        
    def _setup_logging(self) -> logging.Logger:
        logger = logging.getLogger('mcp_security')
        logger.setLevel(logging.INFO)
        
        handler = logging.StreamHandler()
        formatter = logging.Formatter('[%(levelname)s] %(message)s')
        handler.setFormatter(formatter)
        logger.addHandler(handler)
        
        return logger
    
    def run_full_scan(self) -> bool:
        """Run complete security scan"""
        self.logger.info("Starting MCP Debugger Security Scan")
        
        # Code analysis
        self.scan_source_code()
        
        # Credential leaks
        self.scan_credential_leaks()
        
        # Dependency vulnerabilities
        self.scan_dependencies()
        
        # Configuration security
        self.scan_configurations()
        
        # Binary security features
        self.scan_binary_security()
        
        # Generate report
        return self._generate_report()
    
    def scan_source_code(self):
        """Scan source code for security vulnerabilities"""
        self.logger.info("Scanning source code for vulnerabilities...")
        
        # Dangerous function patterns
        dangerous_patterns = [
            (r'strcpy\s*\(', 'Use of unsafe strcpy function'),
            (r'strcat\s*\(', 'Use of unsafe strcat function'),
            (r'sprintf\s*\(', 'Use of unsafe sprintf function'),
            (r'gets\s*\(', 'Use of unsafe gets function'),
            (r'system\s*\(', 'Use of system() function - potential command injection'),
            (r'eval\s*\(', 'Use of eval() function - code injection risk'),
            (r'CreateProcess\w*\s*\([^)]*NULL', 'CreateProcess with potential NULL security descriptor'),
            (r'memcpy\s*\([^,]*,[^,]*,\s*[^)]*\)', 'Potential buffer overflow in memcpy'),
            (r'#include\s+<windows\.h>', 'Windows-specific code - ensure security best practices'),
        ]
        
        # SQL injection patterns (if any database code)
        sql_patterns = [
            (r'SELECT\s+.*\+.*', 'Potential SQL injection - string concatenation'),
            (r'INSERT\s+.*\+.*', 'Potential SQL injection - string concatenation'),
            (r'UPDATE\s+.*\+.*', 'Potential SQL injection - string concatenation'),
        ]
        
        # Path traversal patterns
        path_patterns = [
            (r'\.\./', 'Potential path traversal vulnerability'),
            (r'\.\.\\\\', 'Potential path traversal vulnerability'),
            (r'fopen\s*\([^)]*[^"]\.\.', 'Potential path traversal in file operations'),
        ]
        
        all_patterns = dangerous_patterns + sql_patterns + path_patterns
        
        # Scan all source files
        source_extensions = ['.cpp', '.hpp', '.c', '.h']
        
        for ext in source_extensions:
            for file_path in self.project_root.rglob(f'*{ext}'):
                if 'third_party' in str(file_path) or 'build' in str(file_path):
                    continue
                
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                        
                    for line_num, line in enumerate(content.split('\\n'), 1):
                        for pattern, description in all_patterns:
                            if re.search(pattern, line, re.IGNORECASE):
                                self.findings.append({
                                    'type': 'code_vulnerability',
                                    'severity': 'medium',
                                    'file': str(file_path.relative_to(self.project_root)),
                                    'line': line_num,
                                    'description': description,
                                    'pattern': pattern
                                })
                
                except Exception as e:
                    self.logger.warning(f"Error scanning {file_path}: {e}")
    
    def scan_credential_leaks(self):
        """Scan for hardcoded credentials and secrets"""
        self.logger.info("Scanning for credential leaks...")
        
        # Credential patterns
        credential_patterns = [
            (r'password\s*=\s*["\'][^"\']{3,}["\']', 'Hardcoded password'),
            (r'api[_-]?key\s*[=:]\s*["\'][^"\']{10,}["\']', 'Hardcoded API key'),
            (r'secret\s*[=:]\s*["\'][^"\']{10,}["\']', 'Hardcoded secret'),
            (r'token\s*[=:]\s*["\'][^"\']{10,}["\']', 'Hardcoded token'),
            (r'sk-[a-zA-Z0-9]{32,}', 'OpenAI API key pattern'),
            (r'xai-[a-zA-Z0-9]{64}', 'Anthropic API key pattern'),
            (r'AIza[a-zA-Z0-9_-]{35}', 'Google API key pattern'),
            (r'-----BEGIN\\s+(RSA\\s+)?PRIVATE\\s+KEY-----', 'Private key'),
            (r'["\'][a-zA-Z0-9+/]{40,}={0,2}["\']', 'Potential base64 encoded secret'),
        ]
        
        # Files to scan
        scan_extensions = ['.cpp', '.hpp', '.c', '.h', '.json', '.txt', '.md', '.bat', '.ps1', '.py']
        
        for ext in scan_extensions:
            for file_path in self.project_root.rglob(f'*{ext}'):
                if 'build' in str(file_path) or '.git' in str(file_path):
                    continue
                
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                    
                    for line_num, line in enumerate(content.split('\\n'), 1):
                        for pattern, description in credential_patterns:
                            matches = re.finditer(pattern, line, re.IGNORECASE)
                            for match in matches:
                                # Skip obvious test/example patterns
                                if any(keyword in line.lower() for keyword in ['example', 'test', 'sample', 'placeholder', 'your-key']):
                                    continue
                                
                                self.findings.append({
                                    'type': 'credential_leak',
                                    'severity': 'high',
                                    'file': str(file_path.relative_to(self.project_root)),
                                    'line': line_num,
                                    'description': description,
                                    'match': match.group()[:20] + '...' if len(match.group()) > 20 else match.group()
                                })
                
                except Exception as e:
                    self.logger.warning(f"Error scanning {file_path}: {e}")
    
    def scan_dependencies(self):
        """Scan for vulnerable dependencies"""
        self.logger.info("Scanning dependencies for vulnerabilities...")
        
        # Check CMake files for external dependencies
        cmake_files = list(self.project_root.rglob('CMakeLists.txt'))
        
        for cmake_file in cmake_files:
            try:
                with open(cmake_file, 'r') as f:
                    content = f.read()
                
                # Look for external package usage
                external_deps = re.findall(r'find_package\\s*\\(\\s*([^)\\s]+)', content, re.IGNORECASE)
                vcpkg_deps = re.findall(r'vcpkg_install\\s*\\(\\s*([^)\\s]+)', content, re.IGNORECASE)
                
                for dep in external_deps + vcpkg_deps:
                    # Check against known vulnerable packages (simplified)
                    known_vulnerable = {
                        'openssl': 'Check OpenSSL version for known CVEs',
                        'curl': 'Check libcurl version for known CVEs',
                        'zlib': 'Check zlib version for known CVEs',
                    }
                    
                    if dep.lower() in known_vulnerable:
                        self.findings.append({
                            'type': 'dependency_vulnerability',
                            'severity': 'medium',
                            'file': str(cmake_file.relative_to(self.project_root)),
                            'description': f'Dependency {dep}: {known_vulnerable[dep.lower()]}',
                            'dependency': dep
                        })
            
            except Exception as e:
                self.logger.warning(f"Error scanning {cmake_file}: {e}")
    
    def scan_configurations(self):
        """Scan configuration files for security issues"""
        self.logger.info("Scanning configuration files...")
        
        config_files = list(self.project_root.rglob('*.json')) + list(self.project_root.rglob('config*'))
        
        for config_file in config_files:
            if 'build' in str(config_file):
                continue
            
            try:
                with open(config_file, 'r') as f:
                    content = f.read()
                
                # Check for insecure configurations
                insecure_configs = [
                    (r'"validate_ssl"\\s*:\\s*false', 'SSL validation disabled'),
                    (r'"debug"\\s*:\\s*true', 'Debug mode enabled in production config'),
                    (r'"log_level"\\s*:\\s*"debug"', 'Debug logging enabled'),
                    (r'"auto_connect"\\s*:\\s*true', 'Auto-connect enabled (security risk)'),
                ]
                
                for pattern, description in insecure_configs:
                    if re.search(pattern, content, re.IGNORECASE):
                        self.findings.append({
                            'type': 'configuration_issue',
                            'severity': 'low',
                            'file': str(config_file.relative_to(self.project_root)),
                            'description': description
                        })
            
            except Exception as e:
                self.logger.warning(f"Error scanning {config_file}: {e}")
    
    def scan_binary_security(self):
        """Scan compiled binaries for security features"""
        self.logger.info("Scanning binary security features...")
        
        # Look for compiled binaries
        binary_paths = []
        
        # Common build output locations
        build_dirs = ['build', 'Release', 'Debug']
        for build_dir in build_dirs:
            build_path = self.project_root / build_dir
            if build_path.exists():
                binary_paths.extend(build_path.rglob('*.exe'))
                binary_paths.extend(build_path.rglob('*.dll'))
        
        for binary_path in binary_paths:
            if binary_path.stat().st_size < 1024:  # Skip very small files
                continue
            
            try:
                # Check for basic security features using dumpbin (if available)
                # This is Windows-specific
                if os.name == 'nt':
                    try:
                        result = subprocess.run(
                            ['dumpbin', '/headers', str(binary_path)],
                            capture_output=True,
                            text=True,
                            timeout=30
                        )
                        
                        if result.returncode == 0:
                            headers = result.stdout
                            
                            # Check for security features
                            security_features = {
                                'ASLR': 'Dynamic base' in headers,
                                'DEP': 'NX compatible' in headers,
                                'SafeSEH': 'Safe SEH' in headers,
                                'GS': '/GS' in headers or 'Buffer Security Check' in headers,
                            }
                            
                            missing_features = [feature for feature, present in security_features.items() if not present]
                            
                            if missing_features:
                                self.findings.append({
                                    'type': 'binary_security',
                                    'severity': 'medium',
                                    'file': str(binary_path.relative_to(self.project_root)),
                                    'description': f'Missing security features: {", ".join(missing_features)}'
                                })
                    
                    except (subprocess.TimeoutExpired, FileNotFoundError):
                        pass  # dumpbin not available
                
                # Basic PE header analysis (cross-platform)
                with open(binary_path, 'rb') as f:
                    header = f.read(1024)
                    
                    # Check for debug information
                    if b'.pdb' in header or b'RSDS' in header:
                        self.findings.append({
                            'type': 'binary_security',
                            'severity': 'low',
                            'file': str(binary_path.relative_to(self.project_root)),
                            'description': 'Binary contains debug information'
                        })
            
            except Exception as e:
                self.logger.warning(f"Error analyzing binary {binary_path}: {e}")
    
    def _generate_report(self) -> bool:
        """Generate security report"""
        self.logger.info("\\n" + "="*60)
        self.logger.info("SECURITY SCAN REPORT")
        self.logger.info("="*60)
        
        # Group findings by severity
        by_severity = {'high': [], 'medium': [], 'low': []}
        for finding in self.findings:
            by_severity[finding['severity']].append(finding)
        
        total_findings = len(self.findings)
        
        if total_findings == 0:
            self.logger.info("✓ No security issues found!")
            return True
        
        # Print summary
        for severity in ['high', 'medium', 'low']:
            count = len(by_severity[severity])
            if count > 0:
                self.logger.info(f"{severity.upper()}: {count} findings")
        
        # Print detailed findings
        for severity in ['high', 'medium', 'low']:
            findings = by_severity[severity]
            if not findings:
                continue
            
            self.logger.info(f"\\n{severity.upper()} SEVERITY FINDINGS:")
            for i, finding in enumerate(findings, 1):
                self.logger.info(f"  {i}. {finding['description']}")
                self.logger.info(f"     File: {finding['file']}")
                if 'line' in finding:
                    self.logger.info(f"     Line: {finding['line']}")
                if 'match' in finding:
                    self.logger.info(f"     Match: {finding['match']}")
        
        # Save detailed report
        report_data = {
            'timestamp': str(Path().absolute()),
            'total_findings': total_findings,
            'summary': {severity: len(findings) for severity, findings in by_severity.items()},
            'findings': self.findings
        }
        
        report_path = self.project_root / "security_report.json"
        with open(report_path, 'w') as f:
            json.dump(report_data, f, indent=2)
        
        self.logger.info(f"\\nDetailed report saved to: {report_path}")
        
        # Return True if no high severity issues
        return len(by_severity['high']) == 0

def main():
    parser = argparse.ArgumentParser(description='MCP Debugger Security Scanner')
    parser.add_argument('--project-root', default='.', help='Project root directory')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    # Create scanner
    scanner = SecurityScanner(args.project_root)
    
    if args.verbose:
        scanner.logger.setLevel(logging.DEBUG)
    
    # Run scan
    success = scanner.run_full_scan()
    
    return 0 if success else 1

if __name__ == '__main__':
    sys.exit(main())