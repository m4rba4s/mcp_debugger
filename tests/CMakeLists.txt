# Enable testing for the project
enable_testing()

# find_package(GTest REQUIRED) -- This is now in the root CMakeLists.txt

# Add our test executable
add_executable(core_tests
    simple_test.cpp
    core_engine_improved_test.cpp
)

# Link necessary libraries to the test executable
target_link_libraries(core_tests PRIVATE
    GTest::gtest_main
    mcp-core
    mcp-logger
)

# Include current directory to find mock headers
target_include_directories(core_tests PRIVATE
    .
)

# Allow CTest to discover the tests in our executable
include(GoogleTest)
gtest_discover_tests(core_tests)