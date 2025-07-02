# x64dbg Plugin Module for MCP Debugger
# NOTE: Plugin is now built from src/x64dbg/CMakeLists.txt to avoid conflicts

if(NOT BUILD_PLUGIN)
    return()
endif()

# Only build plugin on Windows
if(NOT WIN32)
    message(STATUS "x64dbg plugin can only be built on Windows - skipping")
    return()
endif()

message(STATUS "Plugin is built from src/x64dbg/ - skipping duplicate build")

# This directory contains plugin source files that are used by src/x64dbg/CMakeLists.txt
# No targets are created here to avoid naming conflicts