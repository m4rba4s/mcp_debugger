set(ANALYZER_SOURCES
    dump_analyzer.cpp
)

set(ANALYZER_HEADERS
    dump_analyzer.hpp
)

add_library(mcp-analyzer STATIC ${ANALYZER_SOURCES} ${ANALYZER_HEADERS})

target_include_directories(mcp-analyzer
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mcp-analyzer
    PRIVATE Threads::Threads
)

# Compiler-specific options
if(MSVC)
    target_compile_options(mcp-analyzer PRIVATE /W4)
else()
    target_compile_options(mcp-analyzer PRIVATE -Wall -Wextra)
endif()