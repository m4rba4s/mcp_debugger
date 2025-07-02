set(CLI_SOURCES
    cli_interface.cpp
    main.cpp
)

set(CLI_HEADERS
    cli_interface.hpp
)

# Create the main executable
add_executable(mcp-debugger ${CLI_SOURCES} ${CLI_HEADERS})

target_include_directories(mcp-debugger
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mcp-debugger
    PRIVATE 
    mcp-core
    mcp-config
    mcp-logger
    mcp-parser
    mcp-llm
    mcp-x64dbg
    mcp-analyzer
    mcp-security
    Threads::Threads
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(mcp-debugger PRIVATE 
        kernel32 
        user32
        ws2_32
    )
else()
    # Unix/Linux libraries
    target_link_libraries(mcp-debugger PRIVATE 
        dl
        pthread
    )
    
    # Check for readline library for better CLI experience
    find_library(READLINE_LIBRARY readline)
    if(READLINE_LIBRARY)
        target_link_libraries(mcp-debugger PRIVATE ${READLINE_LIBRARY})
        target_compile_definitions(mcp-debugger PRIVATE HAVE_READLINE)
    endif()
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(mcp-debugger PRIVATE /W4)
    # Enable console subsystem
    set_target_properties(mcp-debugger PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
else()
    target_compile_options(mcp-debugger PRIVATE -Wall -Wextra)
endif()

# Install targets
install(TARGETS mcp-debugger
    RUNTIME DESTINATION bin
)