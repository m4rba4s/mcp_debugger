#pragma once

#include "mcp/ai_provider_interface.hpp"
#include "mcp/interfaces.hpp"
#include <memory>
#include <unordered_map>
#include <string>

namespace mcp {

// Base class for common provider logic
class BaseAIProvider : public IAIProvider {
public:
    BaseAIProvider(std::string name, std::string host, std::shared_ptr<ILogger> logger);

    const std::string& GetName() const override;
    void SetAPIKey(const std::string& api_key) override;

protected:
    std::string name_;
    std::string host_;
    std::string api_key_;
    std::shared_ptr<ILogger> logger_;
    
    // Each provider might need its own client due to different auth/headers  
    virtual std::unordered_map<std::string, std::string> GetCommonHeaders();
};

// OpenAI Provider
class OpenAIProvider : public BaseAIProvider {
public:
    OpenAIProvider(std::shared_ptr<ILogger> logger);
    std::future<Result<LLMResponse>> SendRequest(const LLMRequest& request) override;
private:
    std::string FormatRequest(const LLMRequest& request);
    Result<LLMResponse> ParseResponse(const std::string& response_body, int status_code);
};

// Claude Provider
class ClaudeProvider : public BaseAIProvider {
public:
    ClaudeProvider(std::shared_ptr<ILogger> logger);
    std::future<Result<LLMResponse>> SendRequest(const LLMRequest& request) override;
private:
    std::string FormatRequest(const LLMRequest& request);
    Result<LLMResponse> ParseResponse(const std::string& response_body, int status_code);
    std::unordered_map<std::string, std::string> GetCommonHeaders() override;
};

// Gemini Provider
class GeminiProvider : public BaseAIProvider {
public:
    GeminiProvider(std::shared_ptr<ILogger> logger);
    std::future<Result<LLMResponse>> SendRequest(const LLMRequest& request) override;
private:
    std::string FormatRequest(const LLMRequest& request);
    Result<LLMResponse> ParseResponse(const std::string& response_body, int status_code);
};

} // namespace mcp 