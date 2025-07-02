set(CONFIG_SOURCES
    config_manager.cpp
)

set(CONFIG_HEADERS
    config_manager.hpp
)

add_library(mcp-config STATIC ${CONFIG_SOURCES} ${CONFIG_HEADERS})

target_include_directories(mcp-config PUBLIC
    ../../include
    .
)

target_link_libraries(mcp-config PRIVATE 
    nlohmann_json::nlohmann_json
)