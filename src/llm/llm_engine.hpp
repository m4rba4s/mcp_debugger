#pragma once

#include "mcp/interfaces.hpp"
#include "mcp/ai_provider_interface.hpp"
#include "mcp/types.hpp"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <future>

namespace mcp {

// Forward declarations
class ISecurityManager;
class ILogger;
class HTTPClient;
class IAIProvider;

class LLMEngine : public ILLMEngine {
public:
    explicit LLMEngine(std::shared_ptr<ILogger> logger);
    ~LLMEngine() override = default;

    // ILLMEngine implementation
    std::future<Result<LLMResponse>> SendRequest(const LLMRequest& request) override;
    Result<LLMResponse> SendRequestSync(const LLMRequest& request) override;
    Result<void> SetAPIKey(const std::string& provider, const std::string& key) override;
    std::vector<std::string> GetSupportedProviders() const override;
    Result<void> ValidateConnection(const std::string& provider) override;

    // Provider management
    void RegisterProvider(std::shared_ptr<IAIProvider> provider);
    void SetDefaultProvider(const std::string& provider_name);
    
    // Request management
    void SetMaxConcurrentRequests(size_t max_concurrent);
    void CancelAllRequests();
    size_t GetActiveRequestCount() const;

private:
    std::shared_ptr<ILogger> logger_;
    
    mutable std::mutex providers_mutex_;
    std::unordered_map<std::string, std::shared_ptr<IAIProvider>> providers_;
    std::string default_provider_ = "claude";
    
    // Helper methods
    Result<std::shared_ptr<IAIProvider>> GetProvider(const std::string& provider_name);
    void InitializeDefaultProviders();
};

} // namespace mcp