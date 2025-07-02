#pragma once

#include "mcp/interfaces.hpp"
#include "mcp/types.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace mcp {

// Forward declarations
class ICoreEngine;
class IExprParser;
class ILogger;

class CLIInterface {
public:
    enum class Mode {
        INTERACTIVE,    // REPL mode
        SCRIPT,         // Execute script file
        COMMAND,        // Single command execution
        DAEMON          // Background service mode
    };

    struct CLIConfig {
        Mode mode = Mode::INTERACTIVE;
        std::string script_file;
        std::string command;
        std::string config_file = "mcp-config.json";
        std::string log_file;
        bool verbose = false;
        bool quiet = false;
        bool enable_colors = true;
        bool enable_history = true;
        std::string history_file = ".mcp_history";
        size_t max_history_size = 1000;
    };

    explicit CLIInterface(std::shared_ptr<ICoreEngine> core_engine);
    ~CLIInterface();

    // Main entry points
    int Run(int argc, const char* argv[]);
    int RunInteractive();
    int RunScript(const std::string& script_file);
    int RunCommand(const std::string& command);
    int RunDaemon();

    // Configuration
    Result<void> ParseCommandLine(int argc, const char* argv[]);
    Result<void> LoadConfig(const std::string& config_file);
    const CLIConfig& GetConfig() const { return config_; }

    // REPL functionality
    void StartREPL();
    void StopREPL();
    bool IsREPLRunning() const;

    // Command processing
    Result<std::string> ProcessCommand(const std::string& input);
    Result<std::string> EvaluateExpression(const std::string& expr);

    // Session management
    void SetSessionVariable(const std::string& name, const SExpression& value);
    Result<SExpression> GetSessionVariable(const std::string& name);
    void ClearSession();
    void SaveSession(const std::string& filename);
    Result<void> LoadSession(const std::string& filename);

private:
    std::shared_ptr<ICoreEngine> core_engine_;
    std::shared_ptr<IExprParser> expr_parser_;
    std::shared_ptr<ILogger> logger_;
    
    CLIConfig config_;
    std::atomic<bool> repl_running_{false};
    
    // Session state
    mutable std::mutex session_mutex_;
    std::unordered_map<std::string, SExpression> session_variables_;
    std::vector<std::string> command_history_;
    size_t history_index_ = 0;

    // Built-in command handlers
    std::unordered_map<std::string, std::function<Result<std::string>(const std::vector<SExpression>&)>> builtin_commands_;

    // REPL implementation
    void REPLLoop();
    std::string ReadInput(const std::string& prompt);
    void AddToHistory(const std::string& command);
    std::string GetPrompt() const;
    
    // Input processing
    Result<std::string> PreprocessInput(const std::string& input);
    bool IsMultilineInput(const std::string& input);
    std::string ReadMultilineInput(const std::string& initial_input);
    
    // Command routing
    Result<std::string> RouteCommand(const SExpression& expr);
    Result<std::string> HandleLLMCommand(const std::vector<SExpression>& args);
    Result<std::string> HandleDbgCommand(const std::vector<SExpression>& args);
    Result<std::string> HandleLogCommand(const std::vector<SExpression>& args);
    Result<std::string> HandleConfigCommand(const std::vector<SExpression>& args);
    Result<std::string> HandleHelpCommand(const std::vector<SExpression>& args);
    Result<std::string> HandleExitCommand(const std::vector<SExpression>& args);
    
    // Built-in commands
    void RegisterBuiltinCommands();
    Result<std::string> BuiltinHelp(const std::vector<SExpression>& args);
    Result<std::string> BuiltinQuit(const std::vector<SExpression>& args);
    Result<std::string> BuiltinClear(const std::vector<SExpression>& args);
    Result<std::string> BuiltinHistory(const std::vector<SExpression>& args);
    Result<std::string> BuiltinSession(const std::vector<SExpression>& args);
    Result<std::string> BuiltinConfig(const std::vector<SExpression>& args);
    Result<std::string> BuiltinStatus(const std::vector<SExpression>& args);
    Result<std::string> BuiltinConnect(const std::vector<SExpression>& args);
    Result<std::string> BuiltinDisconnect(const std::vector<SExpression>& args);
    
    // Output formatting
    void PrintOutput(const std::string& output);
    void PrintError(const std::string& error);
    void PrintInfo(const std::string& info);
    void PrintPrompt(const std::string& prompt);
    std::string FormatOutput(const std::string& content, const std::string& type = "");
    std::string ColorizeOutput(const std::string& text, const std::string& color) const;
    
    // History management
    void LoadHistory();
    void SaveHistory();
    std::vector<std::string> SearchHistory(const std::string& pattern);
    
    // Tab completion
    std::vector<std::string> GetCompletions(const std::string& partial_input);
    std::vector<std::string> CompleteCommand(const std::string& partial_command);
    std::vector<std::string> CompleteFunction(const std::string& partial_function);
    std::vector<std::string> CompleteVariable(const std::string& partial_variable);
    
    // Utility methods
    void ShowBanner();
    void ShowUsage();
    void ShowVersion();
    std::string GetVersionString() const;
    bool ShouldUseColors() const;
    
    // Signal handling
    void SetupSignalHandlers();
    static void SignalHandler(int signal);
    static CLIInterface* instance_;
    
    // Error handling
    void HandleException(const std::exception& ex);
    void HandleUnknownException();
};

// Command line argument parser
class ArgumentParser {
public:
    struct Argument {
        std::string short_name;
        std::string long_name;
        std::string description;
        bool has_value = false;
        bool required = false;
        std::string default_value;
    };

    ArgumentParser(const std::string& program_name, const std::string& description);
    
    void AddArgument(const Argument& arg);
    Result<std::unordered_map<std::string, std::string>> Parse(int argc, const char* argv[]);
    
    void PrintHelp() const;
    void PrintUsage() const;

private:
    std::string program_name_;
    std::string description_;
    std::vector<Argument> arguments_;
    std::vector<std::string> positional_args_;
    
    bool IsShortOption(const std::string& arg) const;
    bool IsLongOption(const std::string& arg) const;
    const Argument* FindArgument(const std::string& name) const;
};

// Interactive features
class REPLHistory {
public:
    explicit REPLHistory(size_t max_size = 1000);
    
    void Add(const std::string& command);
    std::string GetPrevious();
    std::string GetNext();
    std::vector<std::string> Search(const std::string& pattern);
    void Clear();
    
    void LoadFromFile(const std::string& filename);
    void SaveToFile(const std::string& filename);
    
    size_t Size() const;
    bool IsEmpty() const;

private:
    std::vector<std::string> history_;
    size_t max_size_;
    size_t current_index_;
};

class TabCompletion {
public:
    TabCompletion();
    
    void AddCompletions(const std::vector<std::string>& completions);
    void AddFunction(const std::string& function_name);
    void AddVariable(const std::string& variable_name);
    
    std::vector<std::string> Complete(const std::string& partial_input);
    
private:
    std::vector<std::string> functions_;
    std::vector<std::string> variables_;
    std::vector<std::string> commands_;
    
    std::vector<std::string> FilterMatches(const std::vector<std::string>& candidates, 
                                          const std::string& prefix);
};

} // namespace mcp