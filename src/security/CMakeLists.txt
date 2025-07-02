set(SECURITY_SOURCES
    security_manager.cpp
)

set(SECURITY_HEADERS
    security_manager.hpp
)

add_library(mcp-security STATIC ${SECURITY_SOURCES} ${SECURITY_HEADERS})

target_include_directories(mcp-security
    PUBLIC ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mcp-security
    PRIVATE Threads::Threads
)

# Platform-specific crypto libraries
if(WIN32)
    target_link_libraries(mcp-security PRIVATE 
        crypt32
        advapi32
    )
else()
    # Try to find OpenSSL for encryption
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        target_link_libraries(mcp-security PRIVATE 
            OpenSSL::SSL 
            OpenSSL::Crypto
        )
        target_compile_definitions(mcp-security PRIVATE HAVE_OPENSSL)
    else()
        message(WARNING "OpenSSL not found. Security features will be limited.")
    endif()
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(mcp-security PRIVATE /W4)
else()
    target_compile_options(mcp-security PRIVATE -Wall -Wextra)
endif()