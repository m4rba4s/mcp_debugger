# MCP Debugger - Windows Setup Guide

## Overview
MCP (Multi-Context Prompt) Debugger is a powerful debugging utility that integrates x64dbg/x32dbg with Large Language Models (Claude, GPT, Gemini) through a custom S-Expression DSL.

## System Requirements
- Windows 10/11 (x64/x86)
- Visual Studio 2019+ or Build Tools
- CMake 3.16+
- x64dbg/x32dbg (for debugging functionality)

## Quick Setup

### Option 1: Automated Setup (Recommended)
1. **Open PowerShell as Administrator**
2. **Run the dependency installer:**
   ```powershell
   cd path\to\mcp-debugger
   .\scripts\install-deps-windows.ps1
   ```
3. **Build the project:**
   ```cmd
   build-windows.bat
   ```

### Option 2: Manual Setup
1. **Install Visual Studio Build Tools:**
   - Download from: https://visualstudio.microsoft.com/downloads/
   - Select "C++ build tools" workload

2. **Install CMake:**
   - Download from: https://cmake.org/download/
   - Add to PATH during installation

3. **Clone and build:**
   ```cmd
   git clone <repository-url>
   cd mcp-debugger
   build-windows.bat
   ```

## Usage

### CLI Mode
```cmd
# Interactive REPL
mcp-debugger.exe

# Execute single command
mcp-debugger.exe -c "(llm \"Explain this assembly code\" (dbg \"disasm main\"))"

# Run script file
mcp-debugger.exe -f scripts\demo-session.mcp
```

### x64dbg Plugin Mode
1. Copy `mcp_debugger.dp64` to `x64dbg\plugins\`
2. Restart x64dbg
3. Use MCP commands in x64dbg command line

## Configuration

### API Keys Setup
Create or edit `config.json`:
```json
{
  "api_configs": {
    "claude": {
      "model": "claude-3-sonnet-20240229",
      "endpoint": "https://api.anthropic.com/v1/messages"
    }
  }
}
```

Set API keys securely:
```cmd
mcp-debugger.exe -c "(config set-api-key claude \"your-api-key-here\")"
```

### x64dbg Integration
Update config for your x64dbg installation:
```json
{
  "debug_config": {
    "x64dbg_path": "C:\\x64dbg\\release\\x64\\x64dbg.exe",
    "auto_connect": true
  }
}
```

## Example S-Expression Commands

### Basic Debugging
```lisp
; Connect to debugger
:connect

; Set breakpoints
(dbg "bp main")
(dbg "bp CreateFileA")

; Analyze with LLM
(llm "What does this function do?" (dbg "disasm main L10"))

; Memory analysis
(llm "Analyze this memory dump" (read-memory 0x401000 256))
```

### Advanced Workflows
```lisp
; Conditional analysis
(if (= (read-memory eip 2) 0x4889)
    (llm "Analyze this x64 instruction" (dbg "u eip L5"))
    (log "warn" "Unexpected instruction pattern"))

; Pattern matching
(llm "Does this contain malware signatures?" 
     (parse-pattern "suspicious-bytes" (read-memory base-addr 1024)))

; Session variables
(set current-func (dbg "? main"))
(llm "Analyze function at" current-func (dbg "u" current-func "L20"))
```

## Troubleshooting

### Build Issues
- **"cl is not recognized"**: Run from Visual Studio Developer Command Prompt
- **CMake not found**: Ensure CMake is in PATH, restart command prompt
- **Missing dependencies**: Run `install-deps-windows.ps1` as Administrator

### Runtime Issues
- **x64dbg not connecting**: Check x64dbg path in config.json
- **API errors**: Verify API keys are set correctly
- **Plugin not loading**: Ensure x64dbg has write permissions to plugins folder

### Common Solutions
```cmd
# Reset configuration
del config.json credentials.encrypted

# Verbose logging
mcp-debugger.exe --verbose

# Test without x64dbg
mcp-debugger.exe -c "(llm \"Hello world\")"
```

## Security Notes
- API keys are encrypted and stored locally
- Memory dumps are sanitized before LLM transmission
- Plugin runs with x64dbg permissions only
- No data transmitted without explicit LLM commands

## Architecture
```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   CLI/Plugin    │    │   Core Engine   │    │   LLM APIs      │
│   Interface     │◄──►│   (DI Container)│◄──►│   (Claude/GPT)  │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         │              ┌─────────────────┐              │
         └─────────────►│   x64dbg Bridge │              │
                        │   (Named Pipes) │              │
                        └─────────────────┘              │
                                 │                       │
                        ┌─────────────────┐              │
                        │   Memory/Pattern│              │
                        │   Analyzer      │              │
                        └─────────────────┘──────────────┘
```

## Development
- **Language**: C++17 with MSVC
- **Build System**: CMake
- **Testing**: Custom test framework
- **Architecture**: Interface-based DI with SOLID principles

For advanced development, see `CLAUDE.md` for architectural details.