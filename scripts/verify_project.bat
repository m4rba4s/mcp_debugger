@echo off
REM MCP Debugger Project Verification Script
REM Checks if all required files are present and fixes missing ones

echo ========================================
echo MCP Debugger Project Verification
echo ========================================
echo.

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo ERROR: Not in MCP project root directory!
    echo Please run this from the directory containing CMakeLists.txt
    pause
    exit /b 1
)

echo Checking project structure...
echo.

REM Check main CMakeLists.txt
if exist "CMakeLists.txt" (
    echo [OK] Main CMakeLists.txt found
) else (
    echo [ERROR] Main CMakeLists.txt missing
    set HAS_ERRORS=1
)

REM Check source directories and CMakeLists.txt files
set REQUIRED_CMAKE_FILES=src\config\CMakeLists.txt src\logger\CMakeLists.txt src\parser\CMakeLists.txt src\llm\CMakeLists.txt src\x64dbg\CMakeLists.txt src\security\CMakeLists.txt src\analyzer\CMakeLists.txt src\core\CMakeLists.txt src\cli\CMakeLists.txt src\gui\CMakeLists.txt src\plugin\CMakeLists.txt tests\CMakeLists.txt third_party\CMakeLists.txt

for %%f in (%REQUIRED_CMAKE_FILES%) do (
    if exist "%%f" (
        echo [OK] %%f
    ) else (
        echo [MISSING] %%f
        set HAS_ERRORS=1
    )
)

echo.
echo Checking source files...

REM Check critical source files
set CRITICAL_FILES=src\cli\main.cpp src\core\core_engine.cpp src\config\config_manager.cpp src\logger\logger.cpp

for %%f in (%CRITICAL_FILES%) do (
    if exist "%%f" (
        echo [OK] %%f
    ) else (
        echo [MISSING] %%f
        set HAS_ERRORS=1
    )
)

echo.
echo Checking include files...

if exist "include\mcp\interfaces.hpp" (
    echo [OK] include\mcp\interfaces.hpp
) else (
    echo [MISSING] include\mcp\interfaces.hpp
    set HAS_ERRORS=1
)

if exist "include\mcp\types.hpp" (
    echo [OK] include\mcp\types.hpp
) else (
    echo [MISSING] include\mcp\types.hpp
    set HAS_ERRORS=1
)

echo.
if defined HAS_ERRORS (
    echo ========================================
    echo ERRORS FOUND! Some files are missing.
    echo ========================================
    echo.
    echo Possible solutions:
    echo 1. Re-copy the project from source
    echo 2. Run the repair script: scripts\repair_project.bat
    echo 3. Check USB drive for errors
    echo.
    echo Would you like to try automatic repair? (y/n)
    set /p REPAIR_CHOICE=
    
    if /i "%REPAIR_CHOICE%"=="y" (
        echo.
        echo Attempting automatic repair...
        call scripts\repair_project.bat
    )
) else (
    echo ========================================
    echo PROJECT STRUCTURE OK!
    echo ========================================
    echo.
    echo All required files found. You can proceed with:
    echo   build-windows-fixed.bat
    echo.
)

pause