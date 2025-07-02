set(PARSER_SOURCES
    sexpr_parser.cpp
)

set(PARSER_HEADERS
    sexpr_parser.hpp
)

add_library(mcp-parser STATIC ${PARSER_SOURCES} ${PARSER_HEADERS})

target_include_directories(mcp-parser
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mcp-parser
    PRIVATE Threads::Threads
)

# Compiler-specific options
if(MSVC)
    target_compile_options(mcp-parser PRIVATE /W4)
else()
    target_compile_options(mcp-parser PRIVATE -Wall -Wextra)
endif()