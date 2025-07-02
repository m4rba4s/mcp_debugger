#include "config_manager.hpp"
#include "mcp/types.hpp"
#include <fstream>

namespace mcp {

ConfigManager::ConfigManager() {}

Result<void> ConfigManager::LoadConfig(const std::string& path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_path_ = path;

    std::ifstream file(path);
    if (!file.is_open()) {
        return mcp::Result<void>::Error("Failed to open config file: " + path);
    }

    try {
        config_data_ = json::parse(file);
        // Update config_obj_ from JSON
        UpdateConfigFromJson();
    } catch (const json::parse_error& e) {
        return mcp::Result<void>::Error("Failed to parse config file: " + std::string(e.what()));
    }

    return mcp::Result<void>::Success();
}

Result<void> ConfigManager::SaveConfig(const std::string& path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return mcp::Result<void>::Error("Failed to open config file for writing: " + path);
    }

    try {
        file << config_data_.dump(4); // Pretty print with 4 spaces
    } catch (const std::exception& e) {
        return mcp::Result<void>::Error("Failed to write config file: " + std::string(e.what()));
    }

    return mcp::Result<void>::Success();
}

Result<void> ConfigManager::SetDefaults() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // Set default configuration
    config_data_ = json{
        {"llm_providers", {
            {"openai", {
                {"api_key", "YOUR_OPENAI_API_KEY_HERE"},
                {"base_url", "https://api.openai.com/v1"},
                {"model", "gpt-3.5-turbo"}
            }},
            {"claude", {
                {"api_key", "YOUR_CLAUDE_API_KEY_HERE"},
                {"base_url", "https://api.anthropic.com"},
                {"model", "claude-3-sonnet-20240229"}
            }}
        }},
        {"default_provider", "openai"},
        {"debug_config", {
            {"x64dbg_path", "C:\\x64dbg\\x64dbg.exe"},
            {"connection_timeout_ms", 5000}
        }},
        {"log_config", {
            {"level", "INFO"},
            {"file_path", "mcp_debugger.log"},
            {"max_size_mb", 10}
        }}
    };
    
    UpdateConfigFromJson();
    return mcp::Result<void>::Success();
}

Result<std::string> ConfigManager::GetValue(const std::string& key) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    try {
        json::json_pointer ptr(key);
        json value = config_data_.at(ptr);
        if (value.is_string()) {
            return mcp::Result<std::string>::Success(value.get<std::string>());
        } else {
            return mcp::Result<std::string>::Success(value.dump());
        }
    } catch (const json::exception& e) {
        return mcp::Result<std::string>::Error("Config key '" + key + "' not found: " + e.what());
    }
}

Result<void> ConfigManager::SetValue(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    try {
        json::json_pointer ptr(key);
        config_data_[ptr] = value;
        UpdateConfigFromJson();
        return mcp::Result<void>::Success();
    } catch (const json::exception& e) {
        return mcp::Result<void>::Error("Failed to set config key '" + key + "': " + e.what());
    }
}

const Config& ConfigManager::GetConfig() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_obj_;
}

const json& ConfigManager::GetConfigJson() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_data_;
}

void ConfigManager::UpdateConfigFromJson() {
    // Convert JSON to Config struct
    // For now, just set basic defaults - this would be expanded as needed
    if (config_data_.contains("debug_config")) {
        auto& debug = config_data_["debug_config"];
        config_obj_.debug_config.x64dbg_path = debug.value("x64dbg_path", "C:\\x64dbg\\x64dbg.exe");
        config_obj_.debug_config.connection_timeout_ms = debug.value("connection_timeout_ms", 5000);
    }
    
    if (config_data_.contains("log_config")) {
        auto& log = config_data_["log_config"];
        std::string level_str = log.value("level", "INFO");
        config_obj_.log_config.output_path = log.value("file_path", "mcp_debugger.log");
        // Convert level string to enum if needed
        if (level_str == "DEBUG") config_obj_.log_config.level = LogConfig::Level::DEBUG;
        else if (level_str == "INFO") config_obj_.log_config.level = LogConfig::Level::INFO;
        else if (level_str == "WARN") config_obj_.log_config.level = LogConfig::Level::WARN;
        else if (level_str == "ERROR") config_obj_.log_config.level = LogConfig::Level::ERROR;
        else if (level_str == "FATAL") config_obj_.log_config.level = LogConfig::Level::FATAL;
    }
}

template<typename T>
Result<T> ConfigManager::GetValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    try {
        // Use json_pointer for nested access, e.g., "/llm_providers/openai/api_key"
        json::json_pointer ptr(key);
        return mcp::Result<T>::Success(config_data_.at(ptr));
    } catch (const json::exception& e) {
        return mcp::Result<T>::Error("Config key '" + key + "' not found or type mismatch: " + e.what());
    }
}

// Explicit template instantiations to avoid linker errors
template mcp::Result<std::string> ConfigManager::GetValue<std::string>(const std::string& key) const;
template mcp::Result<int> ConfigManager::GetValue<int>(const std::string& key) const;
template mcp::Result<bool> ConfigManager::GetValue<bool>(const std::string& key) const;
template mcp::Result<json> ConfigManager::GetValue<json>(const std::string& key) const;

} // namespace mcp 