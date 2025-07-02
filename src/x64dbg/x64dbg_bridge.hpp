#pragma once

#include "mcp/interfaces.hpp"
#include "mcp/types.hpp"
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <functional>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#pragma comment(lib, "ws2_32.lib")
#endif

namespace mcp {

// Forward declarations
class ILogger;

class X64DbgBridge : public IX64DbgBridge {
public:
    enum class ConnectionMode {
        PLUGIN,        // Running as x64dbg plugin
        EXTERNAL,      // External process communication
        PIPE,          // Named pipe communication
        TCP            // TCP socket communication
    };

    explicit X64DbgBridge(std::shared_ptr<ILogger> logger);
    ~X64DbgBridge() override;

    // IX64DbgBridge implementation
    Result<void> Connect() override;
    Result<void> Disconnect() override;
    Result<std::string> ExecuteCommand(const std::string& command) override;
    Result<std::string> GetDisassembly(uintptr_t address) override;
    Result<MemoryDump> ReadMemory(uintptr_t address, size_t size) override;
    Result<void> SetBreakpoint(uintptr_t address) override;
    void RegisterEventHandler(std::function<void(const DebugEvent&)> handler) override;
    bool IsConnected() const override;

    // Extended functionality
    Result<void> SetConnectionMode(ConnectionMode mode);
    Result<void> SetDebuggerPath(const std::string& path);
    Result<void> SetConnectionTimeout(int timeout_ms);
    Result<std::string> GetSymbolAt(uintptr_t address);
    
    // Advanced debugging operations
    Result<std::vector<uint8_t>> ReadMemoryRaw(uintptr_t address, size_t size);
    Result<void> WriteMemory(uintptr_t address, const std::vector<uint8_t>& data);
    Result<void> RemoveBreakpoint(uintptr_t address);
    Result<std::vector<uintptr_t>> GetBreakpoints();
    Result<void> StepInto();
    Result<void> StepOver();
    Result<void> StepOut();
    Result<void> Continue();
    Result<void> Pause();
    
    // Process and module information
    Result<uint32_t> GetCurrentProcessId();
    Result<uint32_t> GetCurrentThreadId();
    Result<std::vector<std::string>> GetLoadedModules();
    Result<uintptr_t> GetModuleBase(const std::string& module_name);
    Result<size_t> GetModuleSize(const std::string& module_name);
    
    // Register access
    Result<uintptr_t> GetRegisterValue(const std::string& register_name);
    Result<void> SetRegisterValue(const std::string& register_name, uintptr_t value);
    
    // Symbol resolution
    Result<uintptr_t> ResolveSymbol(const std::string& symbol);

#ifdef _WIN32
    // Plugin interface when running as x64dbg plugin - public for factory access
    static bool plugin_initialized_;
    static X64DbgBridge* plugin_instance_;
#endif

private:
    struct EventHandlerEntry {
        uint32_t id;
        std::function<void(const DebugEvent&)> handler;
    };

    std::shared_ptr<ILogger> logger_;
    mutable std::mutex connection_mutex_;
    std::atomic<bool> connected_{false};
    ConnectionMode connection_mode_ = ConnectionMode::EXTERNAL;
    std::string debugger_path_;
    int connection_timeout_ms_ = 5000;

    // Event handling
    mutable std::mutex handlers_mutex_;
    std::vector<EventHandlerEntry> event_handlers_;
    std::atomic<uint32_t> next_handler_id_{1};
    
    // Event processing thread
    std::thread event_thread_;
    std::atomic<bool> event_thread_running_{false};
    std::queue<DebugEvent> event_queue_;
    std::mutex event_queue_mutex_;
    std::condition_variable event_condition_;

    // Platform-specific implementation
#ifdef _WIN32
    // Windows-specific handles and communication
    HANDLE process_handle_ = INVALID_HANDLE_VALUE;
    HANDLE pipe_handle_ = INVALID_HANDLE_VALUE;
    SOCKET tcp_socket_ = INVALID_SOCKET;
    bool winsock_initialized_ = false; // БЕЗОПАСНОСТЬ: отслеживаем WSAStartup
#endif

    // Connection management
    Result<void> ConnectAsPlugin();
    Result<void> ConnectExternal();
    Result<void> ConnectPipe();
    Result<void> ConnectTCP();
    
    void DisconnectInternal();
    
    // Command execution
    Result<std::string> SendCommand(const std::string& command);
    Result<std::string> ParseCommandResponse(const std::string& raw_response);
    
    // Event processing
    void EventProcessingLoop();
    void ProcessEventQueue();
    DebugEvent CreateDebugEvent(const std::string& event_data);
    void NotifyEventHandlers(const DebugEvent& event);
    
    // Memory operations helpers
    Result<void> ValidateMemoryAccess(uintptr_t address, size_t size);
    std::string FormatMemoryCommand(const std::string& operation, uintptr_t address, size_t size);
    std::vector<uint8_t> ParseHexData(const std::string& hex_string);
    
    // Utility methods
    std::string EscapeCommand(const std::string& command);
    bool IsValidAddress(uintptr_t address);
    std::string AddressToString(uintptr_t address);
    uintptr_t StringToAddress(const std::string& address_str);
    
    // Platform-specific helpers
#ifdef _WIN32
    Result<void> InitializeWinAPI();
    Result<std::string> ExecuteWinCommand(const std::string& command);
    Result<std::vector<uint8_t>> ReadProcessMemoryWin(uintptr_t address, size_t size);
    Result<void> WriteProcessMemoryWin(uintptr_t address, const std::vector<uint8_t>& data);
    
    // x64dbg plugin interface
    static void PluginEventCallback(int event_type, void* event_data);
    static bool PluginCommand(int argc, const char* argv[]);
#endif
};

// Plugin interface for x64dbg integration
#ifdef _WIN32
class X64DbgPlugin {
public:
    static bool Initialize(HMODULE module);
    static void Shutdown();
    static bool RegisterCommands();
    static X64DbgBridge* GetBridge();

private:
    static HMODULE module_handle_;
    static std::unique_ptr<X64DbgBridge> bridge_instance_;
    static bool initialized_;
};

// Plugin entry points for x64dbg
extern "C" {
    __declspec(dllexport) bool pluginit(int plugin_handle);
    __declspec(dllexport) bool plugstop();
    __declspec(dllexport) void plugsetup(void* setup_struct);
    __declspec(dllexport) bool cbMcpCommand(int argc, const char* argv[]);
}
#endif

// Factory for creating bridge instances
class X64DbgBridgeFactory {
public:
    static std::unique_ptr<X64DbgBridge> CreateBridge(X64DbgBridge::ConnectionMode mode,
                                                     std::shared_ptr<ILogger> logger);
    static std::unique_ptr<X64DbgBridge> CreateFromConfig(const DebugConfig& config,
                                                         std::shared_ptr<ILogger> logger);
    
    // Auto-detection of available connection methods
    static X64DbgBridge::ConnectionMode DetectBestConnectionMode();
    static bool IsX64DbgRunning();
    static std::string FindX64DbgExecutable();
};

} // namespace mcp