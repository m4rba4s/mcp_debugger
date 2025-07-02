# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## MCP Debugger - Multi-Context Prompt Debugging Utility

This is an enterprise-grade debugging utility that bridges x64dbg/x32dbg with LLM capabilities (Claude, GPT, Gemini) through a custom S-Expression DSL.

## Build Commands

### Quick Syntax Test
```bash
./build_test.sh
```

### Standard Development Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DBUILD_GUI=ON
cmake --build . --config Debug
```

### Release Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Run Tests
```bash
ctest --output-on-failure
# OR for specific test
ctest -R test_name --verbose
```

### Plugin-Only Build (for x64dbg integration)
```bash
cmake .. -DBUILD_GUI=OFF -DBUILD_TESTS=OFF -DBUILD_PLUGIN=ON
```

## Core Architecture

### Interface-Based Dependency Injection
The system uses a central `ICoreEngine` as a service locator/DI container. All major components are accessed through interfaces:

- `ILLMEngine` - Multi-provider LLM abstraction (Claude/GPT/Gemini)
- `IX64DbgBridge` - Direct debugger communication
- `IExprParser` - S-Expression DSL evaluation
- `IConfigManager` - JSON configuration with hot-reload
- `IDumpAnalyzer` - Memory analysis with ML patterns
- `ISecurityManager` - Encrypted credential storage

### Result<T> Error Handling Pattern
All fallible operations return `Result<T>` instead of exceptions:
```cpp
auto result = engine->SendRequest(request);
if (result.IsSuccess()) {
    auto response = result.Value();
} else {
    LOG_ERROR(result.Error());
}
```

### S-Expression DSL
The system uses Lisp-like expressions for debugging commands:
```lisp
(llm "Analyze this memory dump" (read-memory 0x401000 256))
(dbg "bp 0x401000")
(if (= (read-memory base-addr 4) 0x12345678) 
    (log "Found magic bytes")
    (log "Magic bytes not found"))
```

Key built-in functions:
- `read-memory`, `format-hex`, `parse-pattern` - Memory operations
- `+`, `-`, `*`, `/`, `=`, `if` - Math and logic
- `car`, `cdr`, `cons`, `list` - List operations

## Module Structure

### Static Library Dependencies
Each `src/` subdirectory builds to a static library:
- `mcp-core` - Central orchestration (depends on all others)
- `mcp-config` - JSON parsing and validation
- `mcp-logger` - Async logging with file rotation  
- `mcp-parser` - S-Expression evaluation engine
- `mcp-llm` - HTTP clients for LLM APIs
- `mcp-x64dbg` - Windows debugging API bridge
- `mcp-security` - AES encryption for credentials

### Configuration System
Uses JSON with structured validation in `Config` struct. Default config includes:
- API endpoints and models for each LLM provider
- x64dbg executable path and connection settings
- Logging levels, file rotation, and output paths
- Security settings for credential encryption

Key files: `src/config/config_manager.hpp`, sample configs in root directory.

## LLM Integration Architecture

### Multi-Provider Pattern
`LLMEngine` abstracts different providers through unified `LLMRequest`/`LLMResponse` types:
```cpp
LLMRequest request;
request.provider = "claude";  // or "openai", "gemini"
request.model = "claude-3-sonnet-20240229";
request.prompt = "Analyze this assembly code...";
```

Providers are configured via JSON with separate API keys, endpoints, and models.

### Async Request Handling
All LLM calls return `std::future<Result<LLMResponse>>` for non-blocking operation.

## Memory Analysis Pipeline

`IDumpAnalyzer` provides structured analysis of memory dumps:
- Pattern matching for malware signatures
- String extraction (ASCII/Unicode)
- PE header and metadata parsing
- ML-assisted confidence scoring

Results are returned as typed structs (`PatternMatch`, `StringMatch`, etc.) rather than raw text.

## x64dbg Bridge Protocol

`IX64DbgBridge` communicates with debugger through:
- Command execution with string responses
- Memory reading with typed `MemoryDump` results
- Event registration for breakpoints, exceptions, etc.
- Connection management with timeout handling

## Development Patterns

### Adding New LLM Providers
1. Extend `LLMEngine` with new provider case in request handling
2. Add provider config to `APIConfig` in `types.hpp`
3. Implement HTTP client for provider's specific API format
4. Add provider validation in `SecurityManager`

### Adding S-Expression Functions
1. Register function in `SExprParser::RegisterBuiltinFunctions()`
2. Implement function following `Result<SExpression> BuiltinName(const std::vector<SExpression>& args)` pattern
3. Add type checking using `IsNumber()`, `IsString()`, etc. helpers

### Extending Memory Analysis
1. Add new pattern types to `PatternMatch` struct
2. Implement detection logic in `DumpAnalyzer`
3. Register patterns in analyzer initialization
4. Expose through S-Expression functions if needed

## Testing Strategy

Tests are organized by module in `tests/` directory. Each module has unit tests that mock dependencies through interfaces. Integration tests verify end-to-end workflows.

Key test patterns:
- Mock `ILLMEngine` for testing without API calls
- Mock `IX64DbgBridge` for testing without debugger
- Use `Result<T>` pattern consistently in test assertions

## Security Considerations

- API keys stored encrypted in `SecurityManager`
- All HTTP communications use TLS with certificate validation
- Memory dumps are sanitized before LLM transmission
- Credential rotation policies configurable per provider

## Recent Compilation Fixes

### Windows Header Conflicts (Fixed)
- Reordered `winsock2.h` before `windows.h` in x64dbg bridge
- Added `WIN32_LEAN_AND_MEAN` to prevent header conflicts
- Fixed socket redefinition errors in Windows builds

### Template Issues (Fixed)
- Result<T> template now accepts both lvalue and rvalue references
- Added proper const& overloads for Success() method
- Fixed template specialization for Result<void>

### Missing Methods (Fixed)
- Added LogFormatted() method to ILogger interface
- Ensured all implementations match interface contracts
- Fixed method signature mismatches across modules

### Math Library Issues (Fixed)
- Added `#include <cmath>` to analyzer module
- Replaced `std::log2()` with portable log(x)/log(2.0) formula
- Ensures compatibility with older compilers

### Optional Dependencies (Fixed)
- Made libcurl optional with `#ifdef HAVE_CURL` guards
- Graceful fallback for HTTP functionality when unavailable
- Windows uses WinHTTP, Linux can use CURL when available