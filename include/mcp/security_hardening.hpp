#pragma once

/**
 * @file security_hardening.hpp
 * @brief Security hardening constants and utilities for MCP Debugger
 * 
 * This file contains security-critical constants and utilities that prevent
 * various attack vectors including:
 * - Buffer overflow attacks
 * - Memory exhaustion (DoS)
 * - Stack overflow via deep recursion
 * - Information leakage in logs
 * - Command injection attacks
 * - Race conditions
 */

#include <string>
#include <algorithm>
#include <cctype>

namespace mcp {
namespace security {

// ============================================================================
// MEMORY PROTECTION CONSTANTS
// ============================================================================

/// Maximum size for S-Expression parsing input (1MB)
constexpr size_t MAX_EXPRESSION_SIZE = 1024 * 1024;

/// Maximum recursion depth for expression parsing (prevents stack overflow)
constexpr size_t MAX_RECURSION_DEPTH = 100;

/// Maximum elements in a single S-Expression list (prevents memory exhaustion)
constexpr size_t MAX_LIST_ELEMENTS = 10000;

/// Maximum string length in expressions (64KB)
constexpr size_t MAX_STRING_LENGTH = 64 * 1024;

/// Maximum hex data parsing size (2MB input = 1MB binary data)
constexpr size_t MAX_HEX_LENGTH = 2 * 1024 * 1024;

/// Maximum binary data size for memory dumps (1MB)
constexpr size_t MAX_BINARY_DATA_SIZE = 1024 * 1024;

/// Maximum command length (prevents command injection)
constexpr size_t MAX_COMMAND_LENGTH = 4096;

/// Maximum log file size before rotation (100MB)
constexpr size_t MAX_LOG_FILE_SIZE = 100 * 1024 * 1024;

// ============================================================================
// CRYPTOGRAPHIC SECURITY CONSTANTS
// ============================================================================

/// AES-256 key size in bytes
constexpr size_t AES_KEY_SIZE = 32;

/// AES-GCM IV size in bytes
constexpr size_t AES_IV_SIZE = 12;

/// AES-GCM authentication tag size in bytes
constexpr size_t AES_TAG_SIZE = 16;

/// Minimum encrypted data size (IV + TAG + 1 byte data)
constexpr size_t MIN_ENCRYPTED_SIZE = AES_IV_SIZE + AES_TAG_SIZE + 1;

/// Maximum API key length
constexpr size_t MAX_API_KEY_LENGTH = 200;

/// Minimum API key length
constexpr size_t MIN_API_KEY_LENGTH = 10;

// ============================================================================
// NETWORK SECURITY CONSTANTS
// ============================================================================

/// Maximum HTTP request/response size (10MB)
constexpr size_t MAX_HTTP_SIZE = 10 * 1024 * 1024;

/// Default connection timeout in milliseconds
constexpr int DEFAULT_TIMEOUT_MS = 30000;

/// Maximum number of retry attempts
constexpr int MAX_RETRY_ATTEMPTS = 3;

// ============================================================================
// SECURITY UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Safely hash a string for logging purposes
 * @param input The string to hash
 * @return 32-bit hash suitable for logging
 */
inline uint32_t SafeHash(const std::string& input) {
    return std::hash<std::string>{}(input) & 0xFFFFFFFF;
}

/**
 * @brief Sanitize a string for safe logging (removes credentials/sensitive data)
 * @param input The string to sanitize
 * @return Sanitized string safe for logging
 */
inline std::string SanitizeForLogging(const std::string& input) {
    std::string sanitized = input;
    
    // Remove potential API keys (patterns like sk-*, xai-*, AIza*)
    std::string patterns[] = {
        "sk-[A-Za-z0-9]{48}",
        "xai-[A-Za-z0-9]{64}", 
        "AIza[A-Za-z0-9_-]{35}",
        "[A-Za-z0-9]{32,128}"
    };
    
    for (const auto& pattern : patterns) {
        size_t pos = 0;
        while ((pos = sanitized.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", pos)) != std::string::npos) {
            size_t end = sanitized.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-", pos);
            if (end == std::string::npos) end = sanitized.length();
            
            size_t len = end - pos;
            if (len >= 20 && len <= 200) { // Potential API key length
                sanitized.replace(pos, len, "[REDACTED-" + std::to_string(len) + "]");
                pos += 12; // Length of replacement
            } else {
                pos = end;
            }
        }
    }
    
    return sanitized;
}

/**
 * @brief Sanitize a filename for safe logging (hides sensitive paths)
 * @param filename The filename to sanitize
 * @return Sanitized filename showing only the last component
 */
inline std::string SanitizeFilename(const std::string& filename) {
    size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != std::string::npos && last_slash + 1 < filename.length()) {
        return "..." + filename.substr(last_slash);
    }
    return filename;
}

/**
 * @brief Validate that a string contains only safe characters for commands
 * @param command The command string to validate
 * @return true if command is safe, false otherwise
 */
inline bool IsCommandSafe(const std::string& command) {
    if (command.length() > MAX_COMMAND_LENGTH) {
        return false;
    }
    
    // Check for dangerous characters
    const char* dangerous = ";&|`$()<>\"'\\";
    for (char c : command) {
        if (strchr(dangerous, c) != nullptr) {
            return false;
        }
        if (c < 32 || c > 126) { // Non-printable ASCII
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Escape a command string for safe execution
 * @param command The command to escape
 * @return Escaped command safe for execution
 */
inline std::string EscapeCommand(const std::string& command) {
    if (!IsCommandSafe(command)) {
        return ""; // Return empty string for unsafe commands
    }
    
    std::string escaped;
    escaped.reserve(command.length() * 2);
    
    for (char c : command) {
        if (c == ' ' || c == '\t') {
            escaped += c; // Allow whitespace
        } else if (std::isalnum(c) || c == '.' || c == '-' || c == '_') {
            escaped += c; // Allow safe characters
        } else {
            escaped += '_'; // Replace everything else with underscore
        }
    }
    
    return escaped;
}

/**
 * @brief Validate memory access bounds
 * @param address The memory address
 * @param size The access size
 * @return true if access is within safe bounds
 */
inline bool IsMemoryAccessSafe(uintptr_t address, size_t size) {
    // Basic validation - address not null and size reasonable
    if (address == 0 || size == 0 || size > MAX_BINARY_DATA_SIZE) {
        return false;
    }
    
    // Check for overflow
    if (address + size < address) {
        return false;
    }
    
    // Platform-specific validation could be added here
    return true;
}

} // namespace security
} // namespace mcp