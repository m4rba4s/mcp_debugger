#include "core_engine.hpp"
#include "../logger/logger.hpp"
#include "../llm/llm_engine.hpp"
#include "../x64dbg/x64dbg_bridge.hpp"
#include "../config/config_manager.hpp"
#include "../parser/sexpr_parser.hpp"
#include "../analyzer/dump_analyzer.hpp"
#include "../security/security_manager.hpp"

#include <memory>
#include <mutex>
#include <thread>
#include <future>
#include <string>
#include <exception>

namespace mcp {

// Default constructor
CoreEngine::CoreEngine() = default;

// Constructor for dependency injection
CoreEngine::CoreEngine(std::shared_ptr<ILogger> logger,
                       std::shared_ptr<ILLMEngine> llm_engine,
                       std::shared_ptr<IX64DbgBridge> x64dbg_bridge)
    : logger_(std::move(logger)),
      llm_engine_(std::move(llm_engine)),
      x64dbg_bridge_(std::move(x64dbg_bridge)) {
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "CoreEngine created with injected dependencies.");
    }
}

CoreEngine::~CoreEngine() {
    if (initialized_) {
        Shutdown();
    }
}

Result<void> CoreEngine::Initialize() {
    const std::lock_guard<std::mutex> lock(engine_mutex_);
    
    if (initialized_) {
        return Result<void>::Success();
    }
    
    // Initialize in dependency order
    auto logger_result = InitializeLogger();
    if (!logger_result.IsSuccess()) {
        return logger_result;
    }
    
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "CoreEngine initializing...");
    }
    
    auto security_result = InitializeSecurityManager();
    if (!security_result.IsSuccess()) {
        return security_result;
    }
    
    auto config_result = InitializeConfigManager();
    if (!config_result.IsSuccess()) {
        return config_result;
    }
    
    auto parser_result = InitializeExprParser();
    if (!parser_result.IsSuccess()) {
        return parser_result;
    }
    
    auto analyzer_result = InitializeDumpAnalyzer();
    if (!analyzer_result.IsSuccess()) {
        return analyzer_result;
    }
    
    auto bridge_result = InitializeDebugBridge();
    if (!bridge_result.IsSuccess()) {
        return bridge_result;
    }
    
    // Initialize LLM engine if not injected
    if (!llm_engine_) {
        llm_engine_ = std::make_shared<LLMEngine>(logger_);
    }
    
    initialized_ = true;
    modules_immutable_.store(true, std::memory_order_release); // Modules are now immutable
    
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "CoreEngine initialized successfully.");
    }
    
    return Result<void>::Success();
}

Result<void> CoreEngine::Shutdown() {
    const std::lock_guard<std::mutex> lock(engine_mutex_);
    
    if (!initialized_) {
        return Result<void>::Success();
    }
    
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "Shutting down core engine");
    }
    
    ShutdownModules();
    initialized_ = false;
    
    return Result<void>::Success();
}

std::shared_ptr<ILLMEngine> CoreEngine::GetLLMEngine() {
    // Fast path: no locking after initialization is complete
    if (modules_immutable_.load(std::memory_order_acquire)) {
        return llm_engine_;
    }
    const std::lock_guard<std::mutex> lock(engine_mutex_);
    return llm_engine_;
}

std::shared_ptr<IX64DbgBridge> CoreEngine::GetDebugBridge() {
    if (modules_immutable_.load(std::memory_order_acquire)) {
        return x64dbg_bridge_;
    }
    const std::lock_guard<std::mutex> lock(engine_mutex_);
    return x64dbg_bridge_;
}

std::shared_ptr<IExprParser> CoreEngine::GetExprParser() {
    if (modules_immutable_.load(std::memory_order_acquire)) {
        return expr_parser_;
    }
    const std::lock_guard<std::mutex> lock(engine_mutex_);
    return expr_parser_;
}

std::shared_ptr<IConfigManager> CoreEngine::GetConfigManager() {
    if (modules_immutable_.load(std::memory_order_acquire)) {
        return config_manager_;
    }
    const std::lock_guard<std::mutex> lock(engine_mutex_);
    return config_manager_;
}

std::shared_ptr<ILogger> CoreEngine::GetLogger() {
    if (modules_immutable_.load(std::memory_order_acquire)) {
        return logger_;
    }
    const std::lock_guard<std::mutex> lock(engine_mutex_);
    return logger_;
}

std::shared_ptr<IDumpAnalyzer> CoreEngine::GetDumpAnalyzer() {
    if (modules_immutable_.load(std::memory_order_acquire)) {
        return dump_analyzer_;
    }
    const std::lock_guard<std::mutex> lock(engine_mutex_);
    return dump_analyzer_;
}

std::shared_ptr<ISecurityManager> CoreEngine::GetSecurityManager() {
    if (modules_immutable_.load(std::memory_order_acquire)) {
        return security_manager_;
    }
    const std::lock_guard<std::mutex> lock(engine_mutex_);
    return security_manager_;
}

bool CoreEngine::IsInitialized() const {
    return initialized_.load();
}

Result<void> CoreEngine::LoadConfiguration(const std::string& config_file) {
    if (!config_manager_) {
        return Result<void>::Error("Config manager not initialized");
    }
    
    auto load_result = config_manager_->LoadConfig(config_file);
    if (!load_result.IsSuccess()) {
        return load_result;
    }
    
    return InitializeFromConfig();
}

Result<void> CoreEngine::InitializeFromConfig() {
    if (!config_manager_) {
        return Result<void>::Error("Config manager not available");
    }
    
    const Config& config = config_manager_->GetConfig();
    
    // Update logger configuration
    if (logger_) {
        auto logger_impl = std::dynamic_pointer_cast<Logger>(logger_);
        if (logger_impl) {
            logger_impl->UpdateConfig(config.log_config);
        }
    }
    
    // Configure LLM engine with API configs - simplified for now
    if (llm_engine_) {
        auto llm_impl = std::dynamic_pointer_cast<LLMEngine>(llm_engine_);
        if (llm_impl && logger_) {
            logger_->Log(ILogger::LOG_INFO, "LLM engine configuration loaded");
        }
    }
    
    // Configure debug bridge
    if (x64dbg_bridge_) {
        auto bridge_impl = std::dynamic_pointer_cast<X64DbgBridge>(x64dbg_bridge_);
        if (bridge_impl) {
            bridge_impl->SetDebuggerPath(config.debug_config.x64dbg_path);
            bridge_impl->SetConnectionTimeout(config.debug_config.connection_timeout_ms);
        }
    }
    
    return Result<void>::Success();
}

Result<void> CoreEngine::InitializeLogger() {
    try {
        LogConfig default_config;
        logger_ = std::make_shared<Logger>(default_config);
        return Result<void>::Success();
    } catch (const std::exception& ex) {
        return Result<void>::Error("Failed to initialize logger: " + std::string(ex.what()));
    }
}

Result<void> CoreEngine::InitializeSecurityManager() {
    try {
        security_manager_ = std::make_shared<SecurityManager>(logger_);
        return Result<void>::Success();
    } catch (const std::exception& ex) {
        return Result<void>::Error("Failed to initialize security manager: " + std::string(ex.what()));
    }
}

Result<void> CoreEngine::InitializeConfigManager() {
    try {
        config_manager_ = std::make_shared<ConfigManager>();
        
        // Load default configuration
        auto defaults_result = config_manager_->SetDefaults();
        if (!defaults_result.IsSuccess()) {
            return Result<void>::Error(defaults_result.Error());
        }
        
        return Result<void>::Success();
    } catch (const std::exception& ex) {
        return Result<void>::Error("Failed to initialize config manager: " + std::string(ex.what()));
    }
}

Result<void> CoreEngine::InitializeExprParser() {
    try {
        expr_parser_ = std::make_shared<SExprParser>();
        return Result<void>::Success();
    } catch (const std::exception& ex) {
        return Result<void>::Error("Failed to initialize expression parser: " + std::string(ex.what()));
    }
}

Result<void> CoreEngine::InitializeDumpAnalyzer() {
    try {
        dump_analyzer_ = std::make_shared<DumpAnalyzer>(logger_);
        return Result<void>::Success();
    } catch (const std::exception& ex) {
        return Result<void>::Error("Failed to initialize dump analyzer: " + std::string(ex.what()));
    }
}

Result<void> CoreEngine::InitializeDebugBridge() {
    try {
        x64dbg_bridge_ = std::make_shared<X64DbgBridge>(logger_);
        return Result<void>::Success();
    } catch (const std::exception& ex) {
        return Result<void>::Error("Failed to initialize debug bridge: " + std::string(ex.what()));
    }
}

void CoreEngine::ShutdownModules() {
    // Shutdown in reverse dependency order
    x64dbg_bridge_.reset();
    llm_engine_.reset();
    dump_analyzer_.reset();
    expr_parser_.reset();
    config_manager_.reset();
    security_manager_.reset();
    
    // Logger last so we can log shutdown messages
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "All modules shut down");
        logger_.reset();
    }
}

void CoreEngine::AnalyzeCurrentContext() {
    if (!x64dbg_bridge_ || !llm_engine_) {
        if (logger_) {
            logger_->Log(ILogger::LOG_ERROR, "CoreEngine is not initialized correctly.");
        }
        return;
    }

    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "Starting context analysis...");
    }

    uintptr_t current_address = 0x140001000; // Placeholder
    auto disassembly_result = x64dbg_bridge_->GetDisassembly(current_address);
    
    if (!disassembly_result.IsSuccess()) {
        if (logger_) {
            logger_->Log(ILogger::LOG_ERROR, "Failed to get disassembly: " + disassembly_result.Error());
        }
        return;
    }

    const auto& asm_code = disassembly_result.Value();
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "Got disassembly:\n" + asm_code);
    }

    // 3. Create a prompt for the LLM
    LLMRequest request;
    request.prompt = "Please analyze the following x86_64 assembly code... ";
    // No need to set request.provider, LLMEngine will use the default.

    // 4. Send the request to the LLM engine
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "Sending request to AI provider...");
    }
    std::future<Result<LLMResponse>> future_response = llm_engine_->SendRequest(request);

    // 5. Handle the response asynchronously with safe object lifetime
    auto self = shared_from_this();
    std::thread([self, current_address, future_response = std::move(future_response)]() mutable {
        try {
            auto result = future_response.get();
            if (result.IsSuccess()) {
                const auto& response = result.Value();
                if (self->logger_) {
                    self->logger_->Log(ILogger::LOG_INFO, "AI Analysis Received: " + response.content);
                }

                // Escape the response for the command
                std::string escaped_comment = response.content;
                // Basic escaping: replace quotes. A real implementation would be more robust.
                size_t pos = 0;
                while ((pos = escaped_comment.find('"', pos)) != std::string::npos) {
                     escaped_comment.replace(pos, 1, "\"\"");
                     pos += 2;
                }
                
                std::string command = "SetCommentAt " + std::to_string(current_address) + ", \"" + escaped_comment + "\"";
                if (self->x64dbg_bridge_) {
                    self->x64dbg_bridge_->ExecuteCommand(command);
                }
                if (self->logger_) {
                    self->logger_->Log(ILogger::LOG_INFO, "Set comment at address " + std::to_string(current_address));
                }

            } else {
                if (self->logger_) {
                    self->logger_->Log(ILogger::LOG_ERROR, "AI analysis failed: " + result.Error());
                }
            }
        } catch (const std::future_error& ex) {
            if (self->logger_) {
                self->logger_->Log(ILogger::LOG_ERROR, "Future error in AI analysis: " + std::string(ex.what()));
            }
        } catch (const std::exception& ex) {
            if (self->logger_) {
                self->logger_->Log(ILogger::LOG_ERROR, "Exception in AI analysis thread: " + std::string(ex.what()));
            }
        }
    }).detach();
}

// Factory function implementation
std::shared_ptr<ICoreEngine> CreateCoreEngine() {
    auto engine = std::make_shared<CoreEngine>();
    auto init_result = engine->Initialize();
    if (!init_result.IsSuccess()) {
        return nullptr;
    }
    return engine;
}

} // namespace mcp