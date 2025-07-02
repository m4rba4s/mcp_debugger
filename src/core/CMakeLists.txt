set(CORE_SOURCES
    core_engine.cpp
)

set(CORE_HEADERS
    core_engine.hpp
)

add_library(mcp-core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(mcp-core
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# Core depends on all other modules
target_link_libraries(mcp-core
    PUBLIC
    mcp-config
    mcp-logger  
    mcp-parser
    mcp-llm
    mcp-x64dbg
    mcp-analyzer
    mcp-security
    nlohmann_json::nlohmann_json
    
    PRIVATE 
    Threads::Threads
)

# Compiler-specific options
if(MSVC)
    target_compile_options(mcp-core PRIVATE /W4)
else()
    target_compile_options(mcp-core PRIVATE -Wall -Wextra)
endif()