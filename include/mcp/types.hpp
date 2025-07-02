#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <variant>

namespace mcp {

// LLM Request/Response structures
struct LLMRequest {
    std::string provider;           // "claude", "gpt", "gemini"
    std::string model;              // "claude-3-sonnet", "gpt-4", etc.
    std::string prompt;
    std::vector<std::string> context;
    std::unordered_map<std::string, std::string> parameters;
    double temperature = 0.7;
    int max_tokens = 1024;
    std::optional<std::string> system_prompt;
};

struct LLMResponse {
    std::string content;
    std::string provider;
    std::string model;
    int tokens_used = 0;
    std::chrono::milliseconds response_time{0};
    bool success = false;
    std::optional<std::string> error;
};

// Debugging structures
struct DebugEvent {
    enum class Type {
        BREAKPOINT_HIT,
        EXCEPTION,
        PROCESS_CREATED,
        PROCESS_TERMINATED,
        MODULE_LOADED,
        MODULE_UNLOADED,
        THREAD_CREATED,
        THREAD_TERMINATED
    };
    
    Type type;
    uintptr_t address = 0;
    uint32_t process_id = 0;
    uint32_t thread_id = 0;
    std::string module_name;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;
};

struct MemoryDump {
    uintptr_t base_address;
    std::vector<uint8_t> data;
    size_t size;
    std::string module_name;
    std::unordered_map<std::string, std::string> headers;
    std::chrono::system_clock::time_point timestamp;
};

// S-Expression structures
struct SExpression {
    using Value = std::variant<
        std::string,
        int64_t,
        double,
        bool,
        std::vector<SExpression>
    >;
    
    Value value;
    std::optional<std::string> type_hint;
    
    bool IsAtom() const {
        return !std::holds_alternative<std::vector<SExpression>>(value);
    }
    
    bool IsList() const {
        return std::holds_alternative<std::vector<SExpression>>(value);
    }
    
    template<typename T>
    T Get() const {
        return std::get<T>(value);
    }
};

// Configuration structures
struct APIConfig {
    std::string provider;
    std::string model;
    std::string endpoint;
    std::unordered_map<std::string, std::string> headers;
    int timeout_ms = 30000;
    int max_retries = 3;
    bool validate_ssl = true;
};

struct DebugConfig {
    std::string x64dbg_path;
    std::vector<std::string> plugin_paths;
    bool auto_connect = true;
    int connection_timeout_ms = 5000;
    std::vector<std::string> startup_commands;
};

struct LogConfig {
    enum class Level { DEBUG, INFO, WARN, ERROR, FATAL };
    
    Level level = Level::INFO;
    std::string output_path;
    bool console_output = true;
    bool file_output = true;
    size_t max_file_size_mb = 100;
    int max_files = 10;
    std::string format = "[{timestamp}] [{level}] {message}";
};

// Operators for LogConfig::Level enum
inline bool operator==(LogConfig::Level lhs, LogConfig::Level rhs) {
    return static_cast<int>(lhs) == static_cast<int>(rhs);
}

inline bool operator>=(LogConfig::Level lhs, LogConfig::Level rhs) {
    return static_cast<int>(lhs) >= static_cast<int>(rhs);
}

struct SecurityConfig {
    std::string encryption_key_path;
    std::string credential_store_path;
    bool require_api_key_validation = true;
    bool encrypt_credentials = true;
    int key_rotation_days = 90;
};

struct Config {
    std::unordered_map<std::string, APIConfig> api_configs;
    DebugConfig debug_config;
    LogConfig log_config;
    SecurityConfig security_config;
    std::unordered_map<std::string, std::string> custom_settings;
};

// Analysis results
struct PatternMatch {
    uintptr_t address;
    size_t size;
    std::string pattern_name;
    std::string description;
    double confidence;
    std::unordered_map<std::string, std::string> metadata;
};

struct StringMatch {
    uintptr_t address;
    std::string value;
    std::string encoding;
    size_t length;
    bool is_wide = false;
};

struct AnalysisResult {
    std::vector<PatternMatch> patterns;
    std::vector<StringMatch> strings;
    std::unordered_map<std::string, std::string> metadata;
    std::chrono::system_clock::time_point timestamp;
};

} // namespace mcp