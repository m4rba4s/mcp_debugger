#pragma once

#include "mcp/interfaces.hpp"
#include "mcp/types.hpp"
#include <nlohmann/json.hpp>
#include <mutex>
#include <string>
#include <memory>

namespace mcp {

using json = nlohmann::json;

class ConfigManager : public IConfigManager {
public:
    ConfigManager();
    ~ConfigManager() = default;

    // IConfigManager implementation
    Result<void> LoadConfig(const std::string& path) override;
    Result<void> SaveConfig(const std::string& path) override;
    Result<void> SetDefaults() override;
    Result<std::string> GetValue(const std::string& key) override;
    Result<void> SetValue(const std::string& key, const std::string& value) override;
    const Config& GetConfig() const override;
    
    // Additional methods
    const json& GetConfigJson() const;
    
    template<typename T>
    Result<T> GetValue(const std::string& key) const;

private:
    mutable std::mutex config_mutex_;
    json config_data_;
    std::string config_path_;
    Config config_obj_;  // For interface compatibility
    
    void UpdateConfigFromJson();
};

} // namespace mcp