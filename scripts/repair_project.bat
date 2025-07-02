@echo off
REM MCP Debugger Project Repair Script
REM Creates missing CMakeLists.txt files and directories

echo ========================================
echo MCP Debugger Project Repair
echo ========================================
echo.

REM Create missing directories
echo Creating missing directories...

for %%d in (src\config src\logger src\parser src\llm src\x64dbg src\security src\analyzer src\core src\cli src\gui src\plugin tests third_party include\mcp scripts) do (
    if not exist "%%d" (
        mkdir "%%d"
        echo Created: %%d
    )
)

echo.
echo Creating missing CMakeLists.txt files...

REM Create src\parser\CMakeLists.txt if missing
if not exist "src\parser\CMakeLists.txt" (
    echo Creating src\parser\CMakeLists.txt...
    (
        echo set^(PARSER_SOURCES
        echo     sexpr_parser.cpp
        echo ^)
        echo.
        echo set^(PARSER_HEADERS  
        echo     sexpr_parser.hpp
        echo ^)
        echo.
        echo add_library^(mcp-parser STATIC ${PARSER_SOURCES} ${PARSER_HEADERS}^)
        echo.
        echo target_include_directories^(mcp-parser
        echo     PUBLIC ${CMAKE_SOURCE_DIR}/include
        echo     PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        echo ^)
        echo.
        echo target_link_libraries^(mcp-parser
        echo     PRIVATE Threads::Threads
        echo ^)
    ) > "src\parser\CMakeLists.txt"
)

REM Create src\gui\CMakeLists.txt if missing
if not exist "src\gui\CMakeLists.txt" (
    echo Creating src\gui\CMakeLists.txt...
    (
        echo # GUI Module - Not implemented yet
        echo if^(NOT BUILD_GUI^)
        echo     return^(^)
        echo endif^(^)
        echo message^(STATUS "GUI module not implemented - skipping"^)
    ) > "src\gui\CMakeLists.txt"
)

REM Create src\plugin\CMakeLists.txt if missing  
if not exist "src\plugin\CMakeLists.txt" (
    echo Creating src\plugin\CMakeLists.txt...
    (
        echo # x64dbg Plugin
        echo if^(NOT BUILD_PLUGIN^)
        echo     return^(^)
        echo endif^(^)
        echo.
        echo if^(NOT WIN32^)
        echo     return^(^)
        echo endif^(^)
        echo.
        echo set^(PLUGIN_SOURCES plugin_main.cpp^)
        echo add_library^(mcp-plugin SHARED ${PLUGIN_SOURCES}^)
        echo target_link_libraries^(mcp-plugin PRIVATE mcp-core^)
        echo.
        echo if^(CMAKE_SIZEOF_VOID_P EQUAL 8^)
        echo     set_target_properties^(mcp-plugin PROPERTIES OUTPUT_NAME "mcp_debugger" SUFFIX ".dp64"^)
        echo else^(^)
        echo     set_target_properties^(mcp-plugin PROPERTIES OUTPUT_NAME "mcp_debugger" SUFFIX ".dp32"^)
        echo endif^(^)
    ) > "src\plugin\CMakeLists.txt"
)

REM Create third_party\CMakeLists.txt if missing
if not exist "third_party\CMakeLists.txt" (
    echo Creating third_party\CMakeLists.txt...
    (
        echo # Third-party dependencies
        echo message^(STATUS "No third-party dependencies required"^)
    ) > "third_party\CMakeLists.txt"
)

REM Create minimal plugin files if missing
if not exist "src\plugin\plugin_main.cpp" (
    echo Creating src\plugin\plugin_main.cpp...
    (
        echo // MCP Plugin stub
        echo #ifdef _WIN32
        echo #include ^<windows.h^>
        echo extern "C" __declspec^(dllexport^) bool pluginit^(int handle^) { return true; }
        echo extern "C" __declspec^(dllexport^) bool plugstop^(^) { return true; }
        echo extern "C" __declspec^(dllexport^) void plugsetup^(void* setup^) {}
        echo BOOL APIENTRY DllMain^(HMODULE h, DWORD reason, LPVOID reserved^) { return TRUE; }
        echo #endif
    ) > "src\plugin\plugin_main.cpp"
)

echo.
echo ========================================  
echo REPAIR COMPLETED!
echo ========================================
echo.
echo Project structure has been repaired.
echo You can now try building with:
echo   build-windows-fixed.bat
echo.

pause