# Enable testing for the project
enable_testing()

# find_package(GTest REQUIRED) -- This is now in the root CMakeLists.txt

# Add our test executable
add_executable(core_tests
    core/core_engine_tests.cpp
)

# Link necessary libraries to the test executable
target_link_libraries(core_tests PRIVATE
    GTest::gtest_main
    mcp-core
    mcp-llm
    mcp-config
    mcp-logger
    mcp-x64dbg
)

# Include current directory to find mock headers
target_include_directories(core_tests PRIVATE
    .
)

# Allow CTest to discover the tests in our executable
include(GoogleTest)
gtest_discover_tests(core_tests)