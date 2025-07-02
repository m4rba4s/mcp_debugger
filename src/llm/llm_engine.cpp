#include "llm_engine.hpp"
#include "ai_providers.hpp"
#include "mcp/types.hpp"
#include <algorithm>

namespace mcp {

LLMEngine::LLMEngine(std::shared_ptr<ILogger> logger) 
    : logger_(std::move(logger)) {
    InitializeDefaultProviders();
}

void LLMEngine::InitializeDefaultProviders() {
    // Register default AI providers
    RegisterProvider(std::make_unique<ClaudeProvider>(logger_));
    RegisterProvider(std::make_unique<OpenAIProvider>(logger_));
    RegisterProvider(std::make_unique<GeminiProvider>(logger_));
    
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "LLMEngine initialized with 3 providers");
    }
}

std::future<Result<LLMResponse>> LLMEngine::SendRequest(const LLMRequest& request) {
    std::string provider_name = request.provider.empty() ? default_provider_ : request.provider;
    
    auto provider_result = GetProvider(provider_name);
    if (provider_result.IsError()) {
        if (logger_) {
            logger_->Log(ILogger::LOG_WARN, "Provider not found: " + provider_name);
        }
        
        std::promise<Result<LLMResponse>> promise;
        promise.set_value(Result<LLMResponse>::Error(provider_result.Error()));
        return promise.get_future();
    }
    
    IAIProvider* provider = provider_result.Value();
    return provider->SendRequest(request);
}

Result<LLMResponse> LLMEngine::SendRequestSync(const LLMRequest& request) {
    auto future = SendRequest(request);
    return future.get();
}

Result<void> LLMEngine::SetAPIKey(const std::string& provider, const std::string& key) {
    auto provider_result = GetProvider(provider);
    if (provider_result.IsError()) {
        return Result<void>::Error(provider_result.Error());
    }
    
    provider_result.Value()->SetAPIKey(key);
    return Result<void>::Success();
}

std::vector<std::string> LLMEngine::GetSupportedProviders() const {
    std::lock_guard<std::mutex> lock(providers_mutex_);
    
    std::vector<std::string> providers;
    providers.reserve(providers_.size());
    
    for (const auto& [name, _] : providers_) {
        providers.push_back(name);
    }
    
    return providers;
}

Result<void> LLMEngine::ValidateConnection(const std::string& provider) {
    auto provider_result = GetProvider(provider);
    if (provider_result.IsError()) {
        return Result<void>::Error(provider_result.Error());
    }
    
    // Simple validation - check if provider exists and has name
    if (provider_result.Value()->GetName().empty()) {
        return Result<void>::Error("Provider validation failed: empty name");
    }
    
    return Result<void>::Success();
}

void LLMEngine::RegisterProvider(std::unique_ptr<IAIProvider> provider) {
    if (!provider) return;
    
    std::string name = provider->GetName();
    
    {
        std::lock_guard<std::mutex> lock(providers_mutex_);
        providers_[name] = std::move(provider);
    }
    
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "Registered AI provider: " + name);
    }
}

void LLMEngine::SetDefaultProvider(const std::string& provider_name) {
    std::lock_guard<std::mutex> lock(providers_mutex_);
    if (providers_.find(provider_name) != providers_.end()) {
        default_provider_ = provider_name;
        
        if (logger_) {
            logger_->Log(ILogger::LOG_INFO, "Default provider set to: " + provider_name);
        }
    }
}

Result<IAIProvider*> LLMEngine::GetProvider(const std::string& provider_name) {
    std::lock_guard<std::mutex> lock(providers_mutex_);
    
    auto it = providers_.find(provider_name);
    if (it == providers_.end()) {
        return Result<IAIProvider*>::Error("Provider not found: " + provider_name);
    }
    
    return Result<IAIProvider*>::Success(it->second.get());
}

} // namespace mcp 