# MCP Debugger - Multi-Context Prompt Debugging Utility

Enterprise-grade debugging utility for x64dbg/x32dbg with LLM integration.

## Architecture

- **Core Engine**: Central orchestration and dependency injection
- **LLM Engine**: Multi-provider API integration (Claude, GPT, Gemini)
- **X64Dbg Bridge**: Direct communication with debugger core
- **S-Expression Parser**: Structured data parsing and manipulation
- **Dump Analyzer**: Memory dump analysis with ML-assisted insights
- **Security Manager**: Credential handling and secure API communication

## Build Requirements

- C++17/20 compiler (MSVC 2019+, GCC 9+, Clang 10+)
- CMake 3.16+
- Windows 10/11 (x86/x64)

## Quick Start

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Usage

```bash
# CLI mode
mcp-debugger --config config.json --mode cli

# GUI mode  
mcp-debugger --mode gui

# Plugin mode for x64dbg
# Copy mcp-plugin.dll to x64dbg/plugins/
```

This project is a template for a C++ application with a focus on creating a debugger assistant powered by Large Language Models (LLMs).

## Building the Project on Windows

This project uses CMake and vcpkg for dependency management. A PowerShell script is provided to automate the entire process.

### Prerequisites

- Windows 10/11
- PowerShell 5.1+
- Git
- Visual Studio 2019 or newer with the "Desktop development with C++" workload.

### Quick Build

1.  **Open PowerShell**: Open a new PowerShell terminal.
2.  **Set Execution Policy (if needed)**: If you haven't run PowerShell scripts before, you may need to allow them for the current session.
    ```powershell
    Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
    ```
3.  **Navigate to Project Root**: `cd` into the project's root directory.
4.  **Run the Build Script**:
    ```powershell
    # To install dependencies and build the project (first time)
    .\build.ps1 -InstallDeps

    # To just build the project (after first time)
    .\build.ps1
    
    # For a clean build
    .\build.ps1 -Clean
    ```
The script will handle installing `vcpkg`, required libraries, and compiling the project. The final executables will be located in the `build/bin/` directory.


## Project Structure