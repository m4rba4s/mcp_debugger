#pragma once

#include <gmock/gmock.h>
#include "mcp/interfaces.hpp"

class MockLLMEngine : public mcp::ILLMEngine {
public:
    MOCK_METHOD(std::future<mcp::Result<mcp::LLMResponse>>, SendRequest, (const mcp::LLMRequest& request), (override));
    MOCK_METHOD(mcp::Result<mcp::LLMResponse>, SendRequestSync, (const mcp::LLMRequest& request), (override));
    MOCK_METHOD(mcp::Result<void>, SetAPIKey, (const std::string& provider, const std::string& key), (override));
    MOCK_METHOD(std::vector<std::string>, GetSupportedProviders, (), (const, override));
    MOCK_METHOD(mcp::Result<void>, ValidateConnection, (const std::string& provider), (override));
}; 