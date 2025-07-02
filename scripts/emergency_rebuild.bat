@echo off
REM Emergency rebuild script - fixes common issues and rebuilds from scratch

echo ========================================
echo MCP Debugger Emergency Rebuild
echo ========================================
echo.

echo Step 1: Verifying project structure...
call scripts\verify_project.bat

echo.
echo Step 2: Cleaning previous build...
if exist build (
    echo Removing old build directory...
    rmdir /s /q build
)

if exist artifacts (
    echo Removing old artifacts...
    rmdir /s /q artifacts
)

echo.
echo Step 3: Creating minimal required files...

REM Ensure all critical source files exist - create minimal versions if missing

if not exist "include\mcp\interfaces.hpp" (
    echo Creating minimal interfaces.hpp...
    mkdir include\mcp 2>nul
    (
        echo #pragma once
        echo #include ^<memory^>
        echo #include ^<string^>
        echo #include ^<vector^>
        echo namespace mcp {
        echo class ILogger { public: virtual ~ILogger^(^) = default; };
        echo class ILLMEngine { public: virtual ~ILLMEngine^(^) = default; };
        echo }
    ) > "include\mcp\interfaces.hpp"
)

if not exist "include\mcp\types.hpp" (
    echo Creating minimal types.hpp...
    (
        echo #pragma once
        echo #include ^<string^>
        echo namespace mcp {
        echo struct Config {};
        echo }
    ) > "include\mcp\types.hpp"
)

REM Create minimal main.cpp if missing
if not exist "src\cli\main.cpp" (
    echo Creating minimal main.cpp...
    mkdir src\cli 2>nul
    (
        echo #include ^<iostream^>
        echo int main^(int argc, const char* argv[]^) {
        echo     std::cout ^<^< "MCP Debugger v1.0.0" ^<^< std::endl;
        echo     if ^(argc ^> 1 ^&^& std::string^(argv[1]^) == "--help"^) {
        echo         std::cout ^<^< "Usage: mcp-debugger [options]" ^<^< std::endl;
        echo     }
        echo     return 0;
        echo }
    ) > "src\cli\main.cpp"
)

echo.
echo Step 4: Running repair script...
call scripts\repair_project.bat

echo.
echo Step 5: Emergency build attempt...

mkdir build
cd build

echo Configuring with minimal options...
cmake .. -G "Visual Studio 16 2019" -A x64 -DBUILD_TESTS=OFF -DBUILD_GUI=OFF -DBUILD_PLUGIN=OFF
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo CMAKE FAILED! Trying with different generator...
    cmake .. -DBUILD_TESTS=OFF -DBUILD_GUI=OFF -DBUILD_PLUGIN=OFF
)

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Building...
    cmake --build . --config Release
    
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo ========================================
        echo EMERGENCY REBUILD SUCCESSFUL!
        echo ========================================
        echo.
        echo Binary location: build\src\cli\Release\mcp-debugger.exe
        echo.
        
        REM Test the binary
        if exist "src\cli\Release\mcp-debugger.exe" (
            echo Testing binary...
            "src\cli\Release\mcp-debugger.exe" --help
        )
    ) else (
        echo.
        echo ========================================
        echo BUILD STILL FAILED!
        echo ========================================
        echo.
        echo Possible issues:
        echo 1. Missing Visual Studio components
        echo 2. Corrupted source files
        echo 3. Path issues
        echo.
        echo Try:
        echo 1. Re-copy project from source
        echo 2. Check Visual Studio installation
        echo 3. Run from VS Developer Command Prompt
    )
) else (
    echo.
    echo ========================================
    echo CMAKE CONFIGURATION FAILED!
    echo ========================================
    echo.
    echo This usually means:
    echo 1. CMake not installed properly
    echo 2. Visual Studio not found
    echo 3. Missing project files
    echo.
    echo Solutions:
    echo 1. Install CMake: https://cmake.org/download/
    echo 2. Install Visual Studio Build Tools
    echo 3. Run scripts\install-deps-windows.ps1 as admin
)

cd ..
echo.
pause