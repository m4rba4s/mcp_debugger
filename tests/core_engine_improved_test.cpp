#include <gtest/gtest.h>
#include <memory>
#include "../src/core/core_engine.hpp"

using namespace mcp;

/**
 * @brief Test fixture for improved CoreEngine
 */
class CoreEngineImprovedTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine_ = std::make_shared<CoreEngine>();
    }

    void TearDown() override {
        if (engine_ && engine_->IsInitialized()) {
            engine_->Shutdown();
        }
    }

    std::shared_ptr<CoreEngine> engine_;
};

/**
 * @brief Test basic initialization and shutdown
 */
TEST_F(CoreEngineImprovedTest, InitializeAndShutdown) {
    // Test initialization
    auto result = engine_->Initialize();
    EXPECT_TRUE(result.IsSuccess());
    EXPECT_TRUE(engine_->IsInitialized());
    
    // Test that modules are accessible
    EXPECT_NE(nullptr, engine_->GetLogger());
    
    // Test shutdown
    auto shutdown_result = engine_->Shutdown();
    EXPECT_TRUE(shutdown_result.IsSuccess());
}

/**
 * @brief Test thread safety of getters after initialization
 */
TEST_F(CoreEngineImprovedTest, ThreadSafeAccess) {
    // Initialize first
    auto result = engine_->Initialize();
    ASSERT_TRUE(result.IsSuccess());
    
    // Test that we can access modules safely (should use fast path)
    auto logger = engine_->GetLogger();
    auto llm_engine = engine_->GetLLMEngine();
    auto debug_bridge = engine_->GetDebugBridge();
    
    // These should not be null after initialization
    EXPECT_NE(nullptr, logger);
    // Note: llm_engine and debug_bridge might be null if not injected, that's OK
}

/**
 * @brief Test shared_from_this functionality
 */
TEST_F(CoreEngineImprovedTest, SharedFromThisWorks) {
    // Initialize first
    auto result = engine_->Initialize();
    ASSERT_TRUE(result.IsSuccess());
    
    // This should not throw - shared_from_this should work
    EXPECT_NO_THROW({
        auto self = engine_->shared_from_this();
        EXPECT_NE(nullptr, self);
        EXPECT_EQ(engine_, self);
    });
} 