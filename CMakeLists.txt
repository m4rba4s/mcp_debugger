cmake_minimum_required(VERSION 3.16)
project(mcp-debugger VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the vcpkg toolchain file if it exists
if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Build configuration
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_GUI "Build GUI interface" ON)
option(BUILD_PLUGIN "Build x64dbg plugin" ON)

# Dependencies
find_package(Threads REQUIRED)

# Find common third-party packages needed by multiple modules
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(httplib CONFIG REQUIRED)
if(BUILD_TESTS)
    find_package(GTest REQUIRED)
endif()

# Third-party libraries
add_subdirectory(third_party)

# Modules (order matters for dependencies)
add_subdirectory(src/config)
add_subdirectory(src/logger)
add_subdirectory(src/security)
add_subdirectory(src/parser)
add_subdirectory(src/analyzer)
add_subdirectory(src/llm)
add_subdirectory(src/x64dbg)

# Core library (depends on all modules)
add_subdirectory(src/core)

# Interfaces
add_subdirectory(src/cli)

# Optional components
add_subdirectory(src/gui)
add_subdirectory(src/plugin)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install configuration
install(TARGETS mcp-debugger
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)