#include "x64dbg_bridge.hpp"
#include "../core/core_engine.hpp"
#include "../logger/logger.hpp"
#include "../config/config_manager.hpp"
#include "../llm/llm_engine.hpp"

#include <memory>
#include <windows.h> // For GetModuleHandle
#include <filesystem>

// Global instance of our engine
static std::unique_ptr<mcp::CoreEngine> g_CoreEngine;
static int g_pluginHandle;

// Command handler for "mcp_analyze"
bool McpAnalyzeCommand(int argc, char* argv[]) {
    _plugin_logprint("[MCP] Analyze command triggered!\n");
    if (g_CoreEngine) {
        g_CoreEngine->AnalyzeCurrentContext();
    } else {
        _plugin_logprint("[MCP] Error: CoreEngine not initialized.\n");
    }
    return true;
}

DLL_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct) {
    initStruct->pluginVersion = 1;
    initStruct->sdkVersion = PLUG_SDKVERSION;
    strcpy_s(initStruct->pluginName, "MCP");
    g_pluginHandle = initStruct->pluginHandle;

    // --- Dependency Injection ---
    // 1. Create dependencies
    auto logger = std::make_shared<mcp::Logger>();
    logger->info("MCP Plugin initializing...");
    
    auto config_manager = std::make_shared<mcp::ConfigManager>();
    auto llm_engine = std::make_shared<mcp::LLMEngine>(logger);
    auto x64dbg_bridge = std::make_shared<mcp::X64DbgBridge>(logger);

    // 2. Load config and set API keys
    std::string config_path = "plugins/mcp/config.json"; // Relative to x64dbg root
    if (std::filesystem::exists(config_path)) {
        auto load_result = config_manager->LoadConfig(config_path);
        if (load_result.IsOk()) {
            auto providers = config_manager->GetConfig()["llm_providers"];
            for (auto& [name, conf] : providers.items()) {
                llm_engine->SetAPIKey(name, conf["api_key"]);
            }
        }
    } else {
        logger->warn("MCP config not found at '{}'", config_path);
    }
    
    // 3. Assemble CoreEngine
    g_CoreEngine = std::make_unique<mcp::CoreEngine>(logger, llm_engine, x64dbg_bridge);

    // 4. Register command
    _plugin_registercommand(g_pluginHandle, "mcp_analyze", McpAnalyzeCommand, false);

    logger->info("MCP Plugin initialized successfully.");
    return true;
}

DLL_EXPORT bool plugstop() {
    _plugin_unregistercommand(g_pluginHandle, "mcp_analyze");
    g_CoreEngine.reset();
    return true;
}

DLL_EXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct) {
    // We can add menu items here
}