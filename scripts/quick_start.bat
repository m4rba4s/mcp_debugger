@echo off
REM MCP Debugger Quick Start Script for Windows
REM This script sets up everything needed to get started quickly

echo ========================================
echo MCP Debugger Quick Start Setup
echo ========================================
echo.

REM Check if running from correct directory
if not exist "scripts\quick_start.bat" (
    echo Error: Please run this script from the MCP project root directory
    echo Current directory: %CD%
    pause
    exit /b 1
)

REM Set colors for output
for /F %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"
set "GREEN=%ESC%[32m"
set "YELLOW=%ESC%[33m"
set "RED=%ESC%[31m"
set "BLUE=%ESC%[34m"
set "NC=%ESC%[0m"

echo %BLUE%Step 1: Checking prerequisites...%NC%
echo.

REM Check for Visual Studio
where cl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo %RED%✗ Visual Studio compiler not found%NC%
    echo.
    echo Please install Visual Studio 2019+ with C++ tools, or run this from
    echo a Visual Studio Developer Command Prompt.
    echo.
    echo Alternative: Run scripts\install-deps-windows.ps1 as Administrator
    pause
    exit /b 1
) else (
    echo %GREEN%✓ Visual Studio compiler found%NC%
)

REM Check for CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo %RED%✗ CMake not found%NC%
    echo.
    echo Please install CMake from https://cmake.org/download/
    echo or run scripts\install-deps-windows.ps1 as Administrator
    pause
    exit /b 1
) else (
    echo %GREEN%✓ CMake found%NC%
)

REM Check for Python
where python >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo %YELLOW%⚠ Python not found - some scripts will not work%NC%
) else (
    echo %GREEN%✓ Python found%NC%
)

REM Check for Git
where git >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo %YELLOW%⚠ Git not found%NC%
) else (
    echo %GREEN%✓ Git found%NC%
)

echo.
echo %BLUE%Step 2: Building MCP Debugger...%NC%
echo.

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring build...
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_PLUGIN=ON
if %ERRORLEVEL% NEQ 0 (
    echo %RED%✗ CMake configuration failed%NC%
    cd ..
    pause
    exit /b 1
)

REM Build the project
echo.
echo Building project...
cmake --build . --config Release --parallel
if %ERRORLEVEL% NEQ 0 (
    echo %RED%✗ Build failed%NC%
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo %GREEN%✓ Build completed successfully!%NC%

echo.
echo %BLUE%Step 3: Running quick tests...%NC%
echo.

REM Test the binary
if exist "build\src\cli\Release\mcp-debugger.exe" (
    echo Testing binary execution...
    "build\src\cli\Release\mcp-debugger.exe" --help >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo %GREEN%✓ Binary test passed%NC%
    ) else (
        echo %YELLOW%⚠ Binary test failed, but build succeeded%NC%
    )
) else (
    echo %RED%✗ Binary not found after build%NC%
)

echo.
echo %BLUE%Step 4: Setting up configuration...%NC%
echo.

REM Copy sample configuration
if exist "sample-config.json" (
    if not exist "config.json" (
        copy "sample-config.json" "config.json" >nul
        echo %GREEN%✓ Configuration file created%NC%
    ) else (
        echo %YELLOW%⚠ Configuration file already exists%NC%
    )
)

echo.
echo %GREEN%========================================%NC%
echo %GREEN%MCP Debugger Setup Complete!%NC%
echo %GREEN%========================================%NC%
echo.

echo %BLUE%What's next:%NC%
echo.
echo %BLUE%1. Set up API keys:%NC%
echo    build\src\cli\Release\mcp-debugger.exe -c "(config set-api-key claude \"your-api-key\")"
echo.
echo %BLUE%2. Test the CLI:%NC%
echo    build\src\cli\Release\mcp-debugger.exe
echo.
echo %BLUE%3. Install x64dbg plugin:%NC%
echo    Copy build\src\x64dbg\Release\mcp_debugger.dp64 to x64dbg\plugins\
echo.
echo %BLUE%4. Run integration tests:%NC%
echo    python scripts\run_integration_tests.py --binary build\src\cli\Release\mcp-debugger.exe
echo.
echo %BLUE%5. Create distributable package:%NC%
echo    python scripts\package.py --platform x64 --config Release --output dist
echo.

echo %BLUE%Documentation:%NC%
echo - README-WINDOWS.md - Complete setup guide
echo - CLAUDE.md - Architecture documentation
echo - docs\ - Additional documentation
echo.

echo %BLUE%Build files location:%NC%
echo - Binary: build\src\cli\Release\mcp-debugger.exe
echo - Plugin: build\src\x64dbg\Release\mcp_debugger.dp64
echo - Config: config.json
echo.

echo %GREEN%Happy debugging with MCP! 🚀%NC%
echo.
pause