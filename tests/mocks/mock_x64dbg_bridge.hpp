#pragma once

#include <gmock/gmock.h>
#include "mcp/interfaces.hpp"

class MockX64DbgBridge : public mcp::IX64DbgBridge {
public:
    MOCK_METHOD(mcp::Result<void>, Connect, (), (override));
    MOCK_METHOD(mcp::Result<void>, Disconnect, (), (override));
    MOCK_METHOD(mcp::Result<std::string>, ExecuteCommand, (const std::string& command), (override));
    MOCK_METHOD(mcp::Result<std::string>, GetDisassembly, (uintptr_t address), (override));
    MOCK_METHOD(mcp::Result<mcp::MemoryDump>, ReadMemory, (uintptr_t address, size_t size), (override));
    MOCK_METHOD(mcp::Result<void>, SetBreakpoint, (uintptr_t address), (override));
    MOCK_METHOD(void, RegisterEventHandler, (std::function<void(const mcp::DebugEvent&)> handler), (override));
    MOCK_METHOD(bool, IsConnected, (), (const, override));
}; 