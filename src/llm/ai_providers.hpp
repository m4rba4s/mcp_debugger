#pragma once

#include "mcp/ai_provider_interface.hpp"
#include "mcp/interfaces.hpp"
#include "mcp/types.hpp"
#include <memory>
#include <unordered_map>
#include <string>

namespace mcp {

// Base provider with common functionality
class BaseAIProvider : public IAIProvider, public std::enable_shared_from_this<BaseAIProvider> {
protected:
    std::string name_;
    std::string host_;
    std::string api_key_;
    std::shared_ptr<ILogger> logger_;

public:
    BaseAIProvider(std::string name, std::string host, std::shared_ptr<ILogger> logger);
    
    const std::string& GetName() const override;
    void SetAPIKey(const std::string& api_key) override;
    
    // Helper methods for derived classes
    virtual std::unordered_map<std::string, std::string> GetCommonHeaders();

protected:
    // Safe async execution that prevents use-after-free
    template<typename Func>
    std::future<Result<LLMResponse>> ExecuteAsync(Func&& func) {
        auto self = shared_from_this();
        return std::async(std::launch::async, [self, func = std::forward<Func>(func)]() {
            return func();
        });
    }
};

// OpenAI Provider
class OpenAIProvider : public BaseAIProvider {
public:
    explicit OpenAIProvider(std::shared_ptr<ILogger> logger);
    std::future<Result<LLMResponse>> SendRequest(const LLMRequest& request) override;
private:
    std::string FormatRequest(const LLMRequest& request);
    Result<LLMResponse> ParseResponse(const std::string& response_body, int status_code);
    Result<LLMResponse> SendRequestImpl(const LLMRequest& request);
};

// Claude Provider
class ClaudeProvider : public BaseAIProvider {
public:
    explicit ClaudeProvider(std::shared_ptr<ILogger> logger);
    std::future<Result<LLMResponse>> SendRequest(const LLMRequest& request) override;
    std::unordered_map<std::string, std::string> GetCommonHeaders() override;
private:
    std::string FormatRequest(const LLMRequest& request);
    Result<LLMResponse> ParseResponse(const std::string& response_body, int status_code);
    Result<LLMResponse> SendRequestImpl(const LLMRequest& request);
};

// Gemini Provider
class GeminiProvider : public BaseAIProvider {
public:
    explicit GeminiProvider(std::shared_ptr<ILogger> logger);
    std::future<Result<LLMResponse>> SendRequest(const LLMRequest& request) override;
private:
    std::string FormatRequest(const LLMRequest& request);
    Result<LLMResponse> ParseResponse(const std::string& response_body, int status_code);
    Result<LLMResponse> SendRequestImpl(const LLMRequest& request);
};

} // namespace mcp 