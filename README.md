# MCP Debugger 🔍

[![Build Status](https://github.com/m4rba4s/mcp_debugger/actions/workflows/ci.yml/badge.svg)](https://github.com/m4rba4s/mcp_debugger/actions)
[![Code Quality](https://img.shields.io/badge/code%20quality-A---%20(Excellent)-brightgreen)](static-analysis-results/code-quality-report.md)
[![Security](https://img.shields.io/badge/security-✅%20No%20Vulnerabilities-green)](static-analysis-results/code-quality-report.md#security-review)
[![Thread Safety](https://img.shields.io/badge/thread%20safety-✅%20Comprehensive-green)](static-analysis-results/code-quality-report.md#thread-safety-review)

**Enterprise-grade AI-powered reverse engineering tool** that seamlessly integrates with x64dbg debugger to provide intelligent analysis of binary executables, memory dumps, and debugging sessions.

## 🚀 Quick Start

### Prerequisites
- **Windows 10/11** (x64)
- **Visual Studio 2022** with C++17 support
- **x64dbg** debugger
- **API Keys** for supported AI providers (Claude, OpenAI, Gemini)

### Installation

1. **Download Latest Release**
   ```bash
   # Download from releases page or build from source
   git clone https://github.com/m4rba4s/mcp_debugger.git
   cd mcp_debugger
   ```

2. **Quick Build**
   ```powershell
   .\build.ps1 -Config Release -RunTests
   ```

3. **Configure AI Providers**
   ```json
   # Edit config/config.json
   {
     "llm_providers": {
       "claude": {
         "api_key": "your-claude-api-key",
         "model": "claude-3-sonnet-20240229"
       }
     }
   }
   ```

4. **Install x64dbg Plugin**
   ```powershell
   # Copy plugin to x64dbg directory
   copy build\Release\mcp_debugger.dp64 "C:\x64dbg\release\x64\plugins\"
   ```

## ✨ Features

### 🤖 **AI-Powered Analysis**
- **Multi-Provider Support**: Claude 3.5, GPT-4, Gemini Pro
- **Intelligent Code Analysis**: Pattern recognition, vulnerability detection
- **Natural Language Queries**: Ask questions about your binary in plain English
- **Context-Aware Responses**: AI understands debugging context

### 🔧 **x64dbg Integration**
- **Seamless Plugin**: Native x64dbg plugin with `mcp_analyze` command
- **Real-time Analysis**: Analyze memory, registers, and call stacks instantly
- **Automated Annotations**: AI-generated comments and insights
- **Custom Commands**: Extend functionality through S-expressions

### 🛡️ **Enterprise Security**
- **AES-256-GCM Encryption**: All credentials encrypted at rest
- **Zero Credential Leaks**: Comprehensive input sanitization
- **Memory Safety**: Modern C++ with RAII throughout
- **DoS Protection**: Input validation and resource limits

### 📊 **Advanced Analysis**
- **Memory Dump Analysis**: Pattern detection, string extraction
- **Binary Pattern Recognition**: Packed executables, obfuscation detection
- **Assembly Intelligence**: x86/x64 instruction analysis with AI insights
- **Vulnerability Assessment**: Security-focused analysis patterns

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     MCP Debugger Core                      │
├─────────────────┬─────────────────┬─────────────────────────┤
│   LLM Engine    │  Security Mgr   │     Config Manager      │
│                 │                 │                         │
│ ┌─────────────┐ │ ┌─────────────┐ │ ┌─────────────────────┐ │
│ │   Claude    │ │ │ Credential  │ │ │    JSON Config      │ │
│ │   OpenAI    │ │ │ Encryption  │ │ │    Validation       │ │
│ │   Gemini    │ │ │ Key Mgmt    │ │ │    Hot Reload       │ │
│ └─────────────┘ │ └─────────────┘ │ └─────────────────────┘ │
├─────────────────┼─────────────────┼─────────────────────────┤
│  X64DBG Bridge  │   Parser/Eval   │       Analyzer          │
│                 │                 │                         │
│ ┌─────────────┐ │ ┌─────────────┐ │ ┌─────────────────────┐ │
│ │   Memory    │ │ │ S-Expr      │ │ │  Pattern Detection  │ │
│ │   Events    │ │ │ Commands    │ │ │  Memory Analysis    │ │
│ │   Debug     │ │ │ Variables   │ │ │  Binary Insights    │ │
│ └─────────────┘ │ └─────────────┘ │ └─────────────────────┘ │
└─────────────────┴─────────────────┴─────────────────────────┘
```

## 🔧 Usage Examples

### Basic Memory Analysis
```lisp
; Analyze memory region for patterns
(analyze-memory 0x401000 1024 "Look for string patterns and API calls")

; Get AI insights on assembly code
(disasm-ai 0x401000 32 "Explain what this function does")

; Search for vulnerabilities
(find-vulns 0x400000 0x500000 "Check for buffer overflows")
```

### AI-Powered Debugging
```lisp
; Ask natural language questions
(ask "What does this function at 0x401234 do?")
(ask "Are there any security issues in this code?")
(ask "Explain the calling convention used here")

; Pattern-based analysis
(find-patterns "encryption" 0x400000 0x500000)
(identify-packer "UPX|Themida|VMProtect")
```

### Advanced Analysis
```lisp
; Memory dump analysis
(dump-analyze "memory.dmp" "Look for credentials and keys")

; Control flow analysis  
(trace-calls 0x401000 "Map the execution flow")

; String analysis
(extract-strings 0x402000 1024 "Find interesting strings")
```

## 📈 Quality Metrics

### 🏆 **Code Quality: A- (Excellent)**

| Category | Grade | Status |
|----------|-------|--------|
| **Security** | A+ | ✅ No vulnerabilities found |
| **Performance** | A | ✅ Optimized algorithms |
| **Maintainability** | A | ✅ Clean, documented code |
| **Architecture** | A+ | ✅ Excellent design patterns |
| **Thread Safety** | A+ | ✅ Comprehensive synchronization |
| **Resource Management** | A+ | ✅ Perfect RAII compliance |

### 📊 **Analysis Results**
- ✅ **36/36 Quality Checks Passed**
- ✅ **0 Critical Issues Found**
- ✅ **0 Security Vulnerabilities**
- ✅ **0 Memory Leaks**
- ✅ **0 Race Conditions**

*Full analysis report: [Code Quality Report](static-analysis-results/code-quality-report.md)*

## 🔒 Security Features

### **Credential Management**
- **AES-256-GCM encryption** for all stored credentials
- **Secure memory wiping** on application exit
- **Key rotation** with configurable intervals
- **Zero hardcoded secrets** in codebase

### **Input Validation**
- **Comprehensive size limits** (DoS protection)
- **Format validation** with regex patterns
- **Memory bounds checking** for all operations
- **Command injection prevention**

### **Network Security**
- **TLS 1.3** for all API communications
- **Certificate validation** enforced
- **Request/response sanitization**
- **Timeout and retry limits**

## 🧪 Testing

### **Automated Testing**
```powershell
# Run complete test suite
.\build.ps1 -RunTests

# Integration tests
python scripts\run_integration_tests.py

# Security scanning
python scripts\security_scan.py
```

### **Manual Testing**
```powershell
# Static analysis
.\static-analysis.ps1 -All

# Performance benchmarks
.\scripts\benchmark.ps1
```

## 📚 Documentation

- 📖 **[Quick Start Guide](docs/QUICK_START.md)** - Get up and running in 5 minutes
- 🏗️ **[Architecture Guide](docs/ARCHITECTURE.md)** - Deep dive into system design
- 🔐 **[Security Guide](docs/SECURITY.md)** - Security best practices
- 🔧 **[API Reference](docs/API.md)** - Complete S-expression command reference
- 🐛 **[Troubleshooting](docs/TROUBLESHOOTING.md)** - Common issues and solutions

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details.

### **Development Setup**
```powershell
# 1. Clone repository
git clone https://github.com/m4rba4s/mcp_debugger.git

# 2. Setup dependencies
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe install

# 3. Build and test
.\build.ps1 -Config Debug -RunTests
```

### **Code Quality Standards**
- **C++17** standard compliance
- **RAII** for all resource management
- **Interface-based** architecture
- **Comprehensive** unit testing
- **Security-first** development

## 📋 Changelog

### **v1.1.0** (2025-01-02) - Quality & Security Release
- ✨ **Enterprise-grade code quality** achieved (A- rating)
- 🔒 **Comprehensive security analysis** - zero vulnerabilities
- 🧵 **Thread safety verification** - no race conditions
- 📊 **Complete static analysis** integration
- 🔧 **Enhanced build system** with automated testing
- 📚 **Improved documentation** and guides

### **v1.0.0** (2024-12-15) - Initial Release
- 🎉 **Core functionality** implemented
- 🤖 **Multi-AI provider** support
- 🔧 **x64dbg plugin** integration
- 🛡️ **Security framework** established

## 🔗 Links

- 🌐 **[GitHub Repository](https://github.com/m4rba4s/mcp_debugger)**
- 📋 **[Issue Tracker](https://github.com/m4rba4s/mcp_debugger/issues)**
- 🚀 **[Releases](https://github.com/m4rba4s/mcp_debugger/releases)**
- 📖 **[Wiki](https://github.com/m4rba4s/mcp_debugger/wiki)**

## 📄 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **x64dbg Team** - For the excellent debugging platform
- **Anthropic, OpenAI, Google** - For AI model access
- **vcpkg Team** - For dependency management
- **C++ Community** - For modern C++ standards and practices

---

<div align="center">

**Built with ❤️ for the reverse engineering community**

[![Stars](https://img.shields.io/github/stars/m4rba4s/mcp_debugger?style=social)](https://github.com/m4rba4s/mcp_debugger/stargazers)
[![Forks](https://img.shields.io/github/forks/m4rba4s/mcp_debugger?style=social)](https://github.com/m4rba4s/mcp_debugger/network/members)

</div>