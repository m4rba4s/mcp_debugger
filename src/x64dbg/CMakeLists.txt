set(X64DBG_SOURCES
    x64dbg_bridge.cpp
)

set(X64DBG_HEADERS
    x64dbg_bridge.hpp
)

add_library(mcp-x64dbg STATIC ${X64DBG_SOURCES} ${X64DBG_HEADERS})

target_include_directories(mcp-x64dbg
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mcp-x64dbg
    PRIVATE Threads::Threads
)

# Platform-specific libraries and compilation
if(WIN32)
    target_link_libraries(mcp-x64dbg 
        PRIVATE 
        ws2_32 
        kernel32 
        user32
        psapi
    )
    
    # Enable Windows-specific features
    target_compile_definitions(mcp-x64dbg PRIVATE _WIN32_WINNT=0x0601)
    
    # Optional: Build as DLL for x64dbg plugin
    if(BUILD_PLUGIN)
        add_library(mcp-plugin SHARED 
            x64dbg_bridge.cpp
            plugin_exports.cpp
        )
        
        target_include_directories(mcp-plugin
            PUBLIC ${CMAKE_SOURCE_DIR}/include
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        )
        
        target_link_libraries(mcp-plugin
            PRIVATE 
            mcp-core
            mcp-logger
            ws2_32 
            kernel32 
            user32
        )
        
        # Plugin-specific compiler flags
        target_compile_definitions(mcp-plugin PRIVATE 
            MCP_PLUGIN_BUILD
            _WIN32_WINNT=0x0601
        )
        
        # Set output name for x64dbg plugin
        set_target_properties(mcp-plugin PROPERTIES
            OUTPUT_NAME "mcp_debugger"
            SUFFIX ".dp64"  # x64dbg plugin extension
        )
    endif()
    
else()
    # Linux/macOS specific libraries if needed
    target_link_libraries(mcp-x64dbg PRIVATE dl)
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(mcp-x64dbg PRIVATE /W4)
else()
    target_compile_options(mcp-x64dbg PRIVATE -Wall -Wextra)
endif()