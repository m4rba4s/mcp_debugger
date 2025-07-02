set(LLM_SOURCES
    llm_engine.cpp
    ai_providers.cpp
)

set(LLM_HEADERS
    llm_engine.hpp
    ai_providers.hpp
)

add_library(mcp-llm STATIC ${LLM_SOURCES} ${LLM_HEADERS})

target_include_directories(mcp-llm PUBLIC
    ../../include
    .
)

target_link_libraries(mcp-llm PRIVATE
    mcp-config
    mcp-logger
    mcp-security
    Threads::Threads
    nlohmann_json::nlohmann_json
    httplib::httplib
)

# Platform-specific HTTP client libraries
if(WIN32)
    target_link_libraries(mcp-llm PRIVATE winhttp)
else()
    # Find libcurl
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(CURL libcurl)
        if(CURL_FOUND)
            target_link_libraries(mcp-llm PRIVATE ${CURL_LIBRARIES})
            target_include_directories(mcp-llm PRIVATE ${CURL_INCLUDE_DIRS})
            target_compile_options(mcp-llm PRIVATE ${CURL_CFLAGS_OTHER})
        endif()
    endif()
    
    # Fallback to find_package if pkg-config not available
    if(NOT CURL_FOUND)
        find_package(CURL QUIET)
        if(CURL_FOUND)
            target_link_libraries(mcp-llm PRIVATE ${CURL_LIBRARIES})
            target_include_directories(mcp-llm PRIVATE ${CURL_INCLUDE_DIRS})
        endif()
    endif()
endif()