#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../src/core/core_engine.hpp"
#include "mocks/mock_llm_engine.hpp"
#include "mocks/mock_x64dbg_bridge.hpp"
#include "../../src/logger/logger.hpp"

using namespace mcp;
using namespace ::testing;

// Test fixture for CoreEngine tests
class CoreEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_bridge_ = std::make_shared<NiceMock<MockX64DbgBridge>>();
        mock_llm_ = std::make_shared<NiceMock<MockLLMEngine>>();
        logger_ = std::make_shared<Logger>();
        
        core_engine_ = std::make_unique<CoreEngine>(logger_, mock_llm_, mock_bridge_);
    }

    std::shared_ptr<MockX64DbgBridge> mock_bridge_;
    std::shared_ptr<MockLLMEngine> mock_llm_;
    std::shared_ptr<Logger> logger_;
    std::unique_ptr<CoreEngine> core_engine_;
};

TEST_F(CoreEngineTest, AnalyzeCurrentContext_ShouldSetCommentOnSuccess) {
    // ARRANGE
    const uintptr_t test_addr = 0x12345678;
    const std::string fake_disasm = "mov eax, 1\nnop";
    const std::string fake_ai_response = "This is an AI analysis.";
    
    // Setup mock bridge to return fake disassembly
    ON_CALL(*mock_bridge_, GetDisassembly(_))
        .WillByDefault(Return(Result<std::string>::Success(fake_disasm)));
    
    // Setup mock LLM to return a fake response
    std::promise<Result<LLMResponse>> promise;
    LLMResponse response{fake_ai_response, "openai", std::chrono::milliseconds(100)};
    promise.set_value(Result<LLMResponse>::Success(response));
    ON_CALL(*mock_llm_, SendRequest(_))
        .WillByDefault(Return(promise.get_future()));

    // We EXPECT the ExecuteCommand to be called with a command containing the AI response
    std::string expected_command_substr = "SetCommentAt " + std::to_string(test_addr) + ", \"" + fake_ai_response + "\"";
    EXPECT_CALL(*mock_bridge_, ExecuteCommand(HasSubstr(expected_command_substr)))
        .WillOnce(Return(Result<std::string>::Success("")));

    // ACT
    core_engine_->AnalyzeCurrentContext();

    // The test will hang here if the background thread is not handled. 
    // For this test, we rely on the EXPECT_CALL to complete. 
    // A more advanced test would use condition variables.
    // To prevent the test runner from exiting before the async code runs, let's add a small delay.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST(DummyTest, AlwaysPasses) {
    SUCCEED();
} 
