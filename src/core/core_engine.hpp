#pragma once

#include "mcp/interfaces.hpp"
#include "mcp/types.hpp"
#include <memory>
#include <mutex>
#include <atomic>

namespace mcp {

class CoreEngine : public ICoreEngine {
public:
    // Default constructor 
    CoreEngine();
    
    // Constructor for dependency injection
    CoreEngine(std::shared_ptr<ILogger> logger,
               std::shared_ptr<ILLMEngine> llm_engine,
               std::shared_ptr<IX64DbgBridge> x64dbg_bridge);

    ~CoreEngine();

    void AnalyzeCurrentContext();

    // ICoreEngine implementation
    Result<void> Initialize() override;
    Result<void> Shutdown() override;
    std::shared_ptr<ILLMEngine> GetLLMEngine() override;
    std::shared_ptr<IX64DbgBridge> GetDebugBridge() override;
    std::shared_ptr<IExprParser> GetExprParser() override;
    std::shared_ptr<IConfigManager> GetConfigManager() override;
    std::shared_ptr<ILogger> GetLogger() override;
    std::shared_ptr<IDumpAnalyzer> GetDumpAnalyzer() override;
    std::shared_ptr<ISecurityManager> GetSecurityManager() override;

    // Extended functionality
    Result<void> LoadConfiguration(const std::string& config_file);
    Result<void> InitializeFromConfig();
    bool IsInitialized() const;

private:
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<ILLMEngine> llm_engine_;
    std::shared_ptr<IX64DbgBridge> x64dbg_bridge_;

    mutable std::mutex engine_mutex_;
    std::atomic<bool> initialized_{false};
    
    // Core modules - initialized in dependency order
    std::shared_ptr<IConfigManager> config_manager_;
    std::shared_ptr<IExprParser> expr_parser_;
    std::shared_ptr<IDumpAnalyzer> dump_analyzer_;
    std::shared_ptr<ISecurityManager> security_manager_;
    
    // Initialization helpers
    Result<void> InitializeLogger();
    Result<void> InitializeConfigManager();
    Result<void> InitializeExprParser();
    Result<void> InitializeDumpAnalyzer();
    Result<void> InitializeSecurityManager();
    Result<void> InitializeDebugBridge();
    
    // Shutdown helpers
    void ShutdownModules();
    
    // Configuration loading
    Result<void> LoadDefaultConfiguration();
    Result<void> ValidateConfiguration();
};

// Factory function for creating core engine instances
std::shared_ptr<ICoreEngine> CreateCoreEngine();

} // namespace mcp