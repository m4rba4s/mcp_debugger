#include "ai_providers.hpp"
#include <nlohmann/json.hpp>
#include <future>
#include <httplib.h>
#include <string>
#include <memory>
#include <utility>
#include <unordered_map>

namespace mcp {

using json = nlohmann::json;

// --- BaseAIProvider ---
BaseAIProvider::BaseAIProvider(std::string name, std::string host, std::shared_ptr<ILogger> logger)
    : name_(std::move(name)), host_(std::move(host)), logger_(std::move(logger)) {}

const std::string& BaseAIProvider::GetName() const { return name_; }
void BaseAIProvider::SetAPIKey(const std::string& api_key) { api_key_ = api_key; }
std::unordered_map<std::string, std::string> BaseAIProvider::GetCommonHeaders() { return {}; }

// --- OpenAIProvider ---
OpenAIProvider::OpenAIProvider(std::shared_ptr<ILogger> logger)
    : BaseAIProvider("openai", "api.openai.com", std::move(logger)) {}

std::future<Result<LLMResponse>> OpenAIProvider::SendRequest(const LLMRequest& request) {
    return ExecuteAsync([this, request]() {
        return SendRequestImpl(request);
    });
}

Result<LLMResponse> OpenAIProvider::SendRequestImpl(const LLMRequest& request) {
    auto cli = httplib::Client(host_);
    
    httplib::Headers headers;
    headers.emplace("Authorization", "Bearer " + api_key_);
    headers.emplace("Content-Type", "application/json");
    
    std::string payload = FormatRequest(request);
    auto res = cli.Post("/v1/chat/completions", headers, payload, "application/json");
    
    if (!res) {
        return Result<LLMResponse>::Error("HTTP request failed");
    }
    
    return ParseResponse(res->body, res->status);
}

std::string OpenAIProvider::FormatRequest(const LLMRequest& r) {
    json payload = {
        {"model", "gpt-4-turbo"},
        {"messages", {
            {{"role", "system"}, {"content", "You are a reverse engineering assistant."}},
            {{"role", "user"}, {"content", r.prompt}}
        }},
        {"max_tokens", 4096}
    };
    return payload.dump();
}

Result<LLMResponse> OpenAIProvider::ParseResponse(const std::string& response_body, int status_code) {
    if (status_code != 200) {
        return Result<LLMResponse>::Error("API Error: " + std::to_string(status_code));
    }
    
    try {
        json body = json::parse(response_body);
        std::string content = body["choices"][0]["message"]["content"];
        LLMResponse resp{content, GetName()};
        return Result<LLMResponse>::Success(resp);
    } catch (const json::exception& e) {
        return Result<LLMResponse>::Error("JSON parsing failed: " + std::string(e.what()));
    }
}

// --- ClaudeProvider ---
ClaudeProvider::ClaudeProvider(std::shared_ptr<ILogger> logger)
    : BaseAIProvider("claude", "api.anthropic.com", std::move(logger)) {}

std::unordered_map<std::string, std::string> ClaudeProvider::GetCommonHeaders() {
    return {
        {"x-api-key", api_key_},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"}
    };
}

std::future<Result<LLMResponse>> ClaudeProvider::SendRequest(const LLMRequest& request) {
    return ExecuteAsync([this, request]() {
        return SendRequestImpl(request);
    });
}

Result<LLMResponse> ClaudeProvider::SendRequestImpl(const LLMRequest& request) {
    auto cli = httplib::Client(host_);
    
    auto headers_map = GetCommonHeaders();
    httplib::Headers headers;
    for (const auto& [key, value] : headers_map) {
        headers.emplace(key, value);
    }
    
    std::string payload = FormatRequest(request);
    auto res = cli.Post("/v1/messages", headers, payload, "application/json");
    
    if (!res) {
        return Result<LLMResponse>::Error("HTTP request failed");
    }
    
    return ParseResponse(res->body, res->status);
}

std::string ClaudeProvider::FormatRequest(const LLMRequest& r) {
    json payload = {
        {"model", "claude-3-opus-20240229"},
        {"max_tokens", 4096},
        {"messages", {{{"role", "user"}, {"content", r.prompt}}}}
    };
    return payload.dump();
}

Result<LLMResponse> ClaudeProvider::ParseResponse(const std::string& response_body, int status_code) {
    if (status_code != 200) {
        return Result<LLMResponse>::Error("API Error: " + std::to_string(status_code));
    }
    
    try {
        json body = json::parse(response_body);
        std::string content = body["content"][0]["text"];
        LLMResponse resp{content, GetName()};
        return Result<LLMResponse>::Success(resp);
    } catch (const json::exception& e) {
        return Result<LLMResponse>::Error("JSON parsing failed: " + std::string(e.what()));
    }
}

// --- GeminiProvider ---
GeminiProvider::GeminiProvider(std::shared_ptr<ILogger> logger)
    : BaseAIProvider("gemini", "generativelanguage.googleapis.com", std::move(logger)) {}

std::future<Result<LLMResponse>> GeminiProvider::SendRequest(const LLMRequest& request) {
    return ExecuteAsync([this, request]() {
        return SendRequestImpl(request);
    });
}

Result<LLMResponse> GeminiProvider::SendRequestImpl(const LLMRequest& request) {
    auto cli = httplib::Client(host_);
    
    std::string payload = FormatRequest(request);
    std::string path = "/v1beta/models/gemini-1.5-pro-latest:generateContent?key=" + api_key_;
    auto res = cli.Post(path, payload, "application/json");
    
    if (!res) {
        return Result<LLMResponse>::Error("HTTP request failed");
    }
    
    return ParseResponse(res->body, res->status);
}

std::string GeminiProvider::FormatRequest(const LLMRequest& r) {
    json payload = {
        {"contents", {{{"parts", {{{"text", r.prompt}}}}}}}
    };
    return payload.dump();
}

Result<LLMResponse> GeminiProvider::ParseResponse(const std::string& response_body, int status_code) {
    if (status_code != 200) {
        return Result<LLMResponse>::Error("API Error: " + std::to_string(status_code));
    }
    
    try {
        json body = json::parse(response_body);
        std::string content = body["candidates"][0]["content"]["parts"][0]["text"];
        LLMResponse resp{content, GetName()};
        return Result<LLMResponse>::Success(resp);
    } catch (const json::exception& e) {
        return Result<LLMResponse>::Error("JSON parsing failed: " + std::string(e.what()));
    }
}

} // namespace mcp 
