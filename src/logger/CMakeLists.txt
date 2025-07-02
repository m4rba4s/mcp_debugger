set(LOGGER_SOURCES
    logger.cpp
)

set(LOGGER_HEADERS
    logger.hpp
)

add_library(mcp-logger STATIC ${LOGGER_SOURCES} ${LOGGER_HEADERS})

target_include_directories(mcp-logger
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mcp-logger
    PRIVATE Threads::Threads
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(mcp-logger PRIVATE kernel32)
endif()