#pragma once

#include "mcp/types.hpp"
#include "mcp/interfaces.hpp" // for Result<T>
#include <future>
#include <string>

namespace mcp {

/// @brief Interface for all Large Language Model (LLM) providers.
/// This abstraction allows the LLMEngine to support various AI services
/// in a plug-and-play manner. Each implementation will handle the
/// specifics of a particular API (e.g., OpenAI, Claude, Gemini).
class IAIProvider {
public:
    virtual ~IAIProvider() = default;

    /// @brief Gets the name of the provider.
    /// @return The unique name of the provider (e.g., "openai", "gemini").
    virtual const std::string& GetName() const = 0;

    /// @brief Sends a request to the LLM provider asynchronously.
    /// @param request The LLMRequest object containing the prompt and other parameters.
    /// @return A std::future containing a Result<LLMResponse>. The result will
    ///         contain either the successful response or an error.
    virtual std::future<Result<LLMResponse>> SendRequest(const LLMRequest& request) = 0;
    
    /// @brief Sets the API key for the provider.
    /// @param api_key The API key to use for authentication.
    virtual void SetAPIKey(const std::string& api_key) = 0;
};

} // namespace mcp 