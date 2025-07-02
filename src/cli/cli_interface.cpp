#include "cli_interface.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#define STDOUT_FILENO _fileno(stdout)
#define isatty _isatty
#else
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

namespace mcp {

CLIInterface* CLIInterface::instance_ = nullptr;

CLIInterface::CLIInterface(std::shared_ptr<ICoreEngine> core_engine) 
    : core_engine_(core_engine) {
    
    if (core_engine_) {
        expr_parser_ = core_engine_->GetExprParser();
        logger_ = core_engine_->GetLogger();
    }
    
    RegisterBuiltinCommands();
    SetupSignalHandlers();
    instance_ = this;
    
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "CLI Interface initialized");
    }
}

CLIInterface::~CLIInterface() {
    if (repl_running_) {
        StopREPL();
    }
    
    SaveHistory();
    instance_ = nullptr;
    
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "CLI Interface destroyed");
    }
}

int CLIInterface::Run(int argc, const char* argv[]) {
    try {
        auto parse_result = ParseCommandLine(argc, argv);
        if (!parse_result.IsSuccess()) {
            PrintError("Failed to parse command line: " + parse_result.Error());
            return 1;
        }
        
        // Load configuration
        auto config_result = LoadConfig(config_.config_file);
        if (!config_result.IsSuccess()) {
            PrintError("Failed to load config: " + config_result.Error());
            return 1;
        }
        
        // Initialize core engine
        if (core_engine_) {
            auto init_result = core_engine_->Initialize();
            if (!init_result.IsSuccess()) {
                PrintError("Failed to initialize core engine: " + init_result.Error());
                return 1;
            }
        }
        
        int exit_code = 0;
        
        switch (config_.mode) {
            case Mode::INTERACTIVE:
                exit_code = RunInteractive();
                break;
            case Mode::SCRIPT:
                exit_code = RunScript(config_.script_file);
                break;
            case Mode::COMMAND:
                exit_code = RunCommand(config_.command);
                break;
            case Mode::DAEMON:
                exit_code = RunDaemon();
                break;
        }
        
        // Shutdown core engine
        if (core_engine_) {
            core_engine_->Shutdown();
        }
        
        return exit_code;
        
    } catch (const std::exception& ex) {
        HandleException(ex);
        return 1;
    } catch (...) {
        HandleUnknownException();
        return 1;
    }
}

int CLIInterface::RunInteractive() {
    if (!config_.quiet) {
        ShowBanner();
    }
    
    LoadHistory();
    StartREPL();
    
    return 0;
}

int CLIInterface::RunScript(const std::string& script_file) {
    std::ifstream file(script_file);
    if (!file.is_open()) {
        PrintError("Failed to open script file: " + script_file);
        return 1;
    }
    
    std::string line;
    int line_number = 1;
    int error_count = 0;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';') { // Skip empty lines and comments
            line_number++;
            continue;
        }
        
        auto result = ProcessCommand(line);
        if (result.IsSuccess()) {
            if (!config_.quiet) {
                PrintOutput(result.Value());
            }
        } else {
            PrintError("Line " + std::to_string(line_number) + ": " + result.Error());
            error_count++;
        }
        
        line_number++;
    }
    
    if (error_count > 0) {
        PrintError("Script completed with " + std::to_string(error_count) + " errors");
        return 1;
    }
    
    return 0;
}

int CLIInterface::RunCommand(const std::string& command) {
    auto result = ProcessCommand(command);
    if (result.IsSuccess()) {
        PrintOutput(result.Value());
        return 0;
    } else {
        PrintError(result.Error());
        return 1;
    }
}

int CLIInterface::RunDaemon() {
    PrintInfo("Starting MCP Debugger in daemon mode...");
    
    // TODO: Implement daemon mode with TCP/pipe server
    // For now, just run interactive mode
    return RunInteractive();
}

void CLIInterface::StartREPL() {
    repl_running_ = true;
    REPLLoop();
}

void CLIInterface::StopREPL() {
    repl_running_ = false;
}

void CLIInterface::REPLLoop() {
    while (repl_running_) {
        try {
            std::string prompt = GetPrompt();
            std::string input = ReadInput(prompt);
            
            if (input.empty()) {
                continue;
            }
            
            // Check for multiline input
            if (IsMultilineInput(input)) {
                input = ReadMultilineInput(input);
            }
            
            AddToHistory(input);
            
            auto result = ProcessCommand(input);
            if (result.IsSuccess()) {
                PrintOutput(result.Value());
            } else {
                PrintError(result.Error());
            }
            
        } catch (const std::exception& ex) {
            HandleException(ex);
        } catch (...) {
            HandleUnknownException();
        }
    }
}

Result<std::string> CLIInterface::ProcessCommand(const std::string& input) {
    auto preprocess_result = PreprocessInput(input);
    if (!preprocess_result.IsSuccess()) {
        return preprocess_result;
    }
    
    std::string processed_input = preprocess_result.Value();
    
    // Handle built-in commands first
    if (!processed_input.empty() && processed_input[0] == ':') {
        std::string command = processed_input.substr(1);
        std::istringstream iss(command);
        std::string cmd_name;
        iss >> cmd_name;
        
        std::vector<SExpression> args;
        std::string arg;
        while (iss >> arg) {
            SExpression expr;
            expr.value = arg;
            args.push_back(expr);
        }
        
        auto it = builtin_commands_.find(cmd_name);
        if (it != builtin_commands_.end()) {
            return it->second(args);
        } else {
            return Result<std::string>::Error("Unknown built-in command: " + cmd_name);
        }
    }
    
    // Parse and evaluate S-expression
    return EvaluateExpression(processed_input);
}

Result<std::string> CLIInterface::EvaluateExpression(const std::string& expr) {
    if (!expr_parser_) {
        return Result<std::string>::Error("Expression parser not available");
    }
    
    auto parse_result = expr_parser_->Parse(expr);
    if (!parse_result.IsSuccess()) {
        return Result<std::string>::Error("Parse error: " + parse_result.Error());
    }
    
    // Add session variables to context
    std::unordered_map<std::string, SExpression> context;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        context = session_variables_;
    }
    
    auto eval_result = expr_parser_->EvaluateInContext(parse_result.Value(), context);
    if (!eval_result.IsSuccess()) {
        return Result<std::string>::Error("Evaluation error: " + eval_result.Error());
    }
    
    // Route command based on expression
    auto route_result = RouteCommand(eval_result.Value());
    if (!route_result.IsSuccess()) {
        return route_result;
    }
    
    return route_result;
}

Result<std::string> CLIInterface::RouteCommand(const SExpression& expr) {
    if (expr.IsAtom()) {
        // Return atom value as string
        auto format_result = expr_parser_->FormatDebugOutput(expr);
        if (format_result.IsSuccess()) {
            return format_result;
        } else {
            return Result<std::string>::Success("(atom)");
        }
    }
    
    auto list = std::get<std::vector<SExpression>>(expr.value);
    if (list.empty()) {
        return Result<std::string>::Success("()");
    }
    
    // First element should be the command
    if (!std::holds_alternative<std::string>(list[0].value)) {
        return Result<std::string>::Error("Command must be a symbol");
    }
    
    std::string command = std::get<std::string>(list[0].value);
    std::vector<SExpression> args(list.begin() + 1, list.end());
    
    // Route to appropriate handler
    if (command == "llm") {
        return HandleLLMCommand(args);
    } else if (command == "dbg") {
        return HandleDbgCommand(args);
    } else if (command == "log") {
        return HandleLogCommand(args);
    } else if (command == "config") {
        return HandleConfigCommand(args);
    } else if (command == "help") {
        return HandleHelpCommand(args);
    } else if (command == "exit" || command == "quit") {
        return HandleExitCommand(args);
    } else {
        // Let expression parser handle it as a function call
        auto serialize_result = expr_parser_->Serialize(expr);
        if (!serialize_result.IsSuccess()) {
            return Result<std::string>::Error("Failed to serialize expression");
        }
        return Result<std::string>::Success(serialize_result.Value());
    }
}

Result<std::string> CLIInterface::HandleLLMCommand(const std::vector<SExpression>& args) {
    if (args.empty()) {
        return Result<std::string>::Error("LLM command requires a prompt");
    }
    
    if (!core_engine_) {
        return Result<std::string>::Error("Core engine not available");
    }
    
    auto llm_engine = core_engine_->GetLLMEngine();
    if (!llm_engine) {
        return Result<std::string>::Error("LLM engine not available");
    }
    
    // Extract prompt from first argument
    std::string prompt;
    if (std::holds_alternative<std::string>(args[0].value)) {
        prompt = std::get<std::string>(args[0].value);
    } else {
        return Result<std::string>::Error("Prompt must be a string");
    }
    
    // Build LLM request
    LLMRequest request;
    request.prompt = prompt;
    request.provider = "claude"; // TODO: Make configurable
    request.model = "claude-3-sonnet-20240229";
    request.max_tokens = 1024;
    
    // Add context from additional arguments
    for (size_t i = 1; i < args.size(); ++i) {
        if (std::holds_alternative<std::string>(args[i].value)) {
            request.context.push_back(std::get<std::string>(args[i].value));
        }
    }
    
    PrintInfo("Sending request to LLM...");
    
    auto response_result = llm_engine->SendRequestSync(request);
    if (!response_result.IsSuccess()) {
        return Result<std::string>::Error("LLM request failed: " + response_result.Error());
    }
    
    LLMResponse response = response_result.Value();
    std::ostringstream output;
    output << "LLM Response (" << response.provider << ", " 
           << response.response_time.count() << "ms, " 
           << response.tokens_used << " tokens):\n";
    output << response.content;
    
    return Result<std::string>::Success(output.str());
}

Result<std::string> CLIInterface::HandleDbgCommand(const std::vector<SExpression>& args) {
    if (args.empty()) {
        return Result<std::string>::Error("Debug command requires a command string");
    }
    
    if (!core_engine_) {
        return Result<std::string>::Error("Core engine not available");
    }
    
    auto debug_bridge = core_engine_->GetDebugBridge();
    if (!debug_bridge) {
        return Result<std::string>::Error("Debug bridge not available");
    }
    
    if (!debug_bridge->IsConnected()) {
        return Result<std::string>::Error("Not connected to debugger");
    }
    
    // Extract command from first argument
    std::string command;
    if (std::holds_alternative<std::string>(args[0].value)) {
        command = std::get<std::string>(args[0].value);
    } else {
        return Result<std::string>::Error("Command must be a string");
    }
    
    auto result = debug_bridge->ExecuteCommand(command);
    if (!result.IsSuccess()) {
        return Result<std::string>::Error("Debug command failed: " + result.Error());
    }
    
    return Result<std::string>::Success("Debug output:\n" + result.Value());
}

Result<std::string> CLIInterface::HandleLogCommand(const std::vector<SExpression>& args) {
    if (args.empty()) {
        return Result<std::string>::Error("Log command requires a message");
    }
    
    if (!logger_) {
        return Result<std::string>::Error("Logger not available");
    }
    
    // Extract log level and message
    ILogger::Level level = ILogger::LOG_INFO;
    std::string message;
    
    if (args.size() >= 2) {
        // First arg is level, second is message
        if (std::holds_alternative<std::string>(args[0].value)) {
            std::string level_str = std::get<std::string>(args[0].value);
            if (level_str == "debug") level = ILogger::LOG_DEBUG;
            else if (level_str == "info") level = ILogger::LOG_INFO;
            else if (level_str == "warn") level = ILogger::LOG_WARN;
            else if (level_str == "error") level = ILogger::LOG_ERROR;
            else if (level_str == "fatal") level = ILogger::LOG_FATAL;
        }
        
        if (std::holds_alternative<std::string>(args[1].value)) {
            message = std::get<std::string>(args[1].value);
        }
    } else {
        // Single argument is the message
        if (std::holds_alternative<std::string>(args[0].value)) {
            message = std::get<std::string>(args[0].value);
        }
    }
    
    if (message.empty()) {
        return Result<std::string>::Error("Log message cannot be empty");
    }
    
    logger_->Log(level, message);
    return Result<std::string>::Success("Logged: " + message);
}

Result<std::string> CLIInterface::HandleExitCommand(const std::vector<SExpression>& /* args */) {
    PrintInfo("Goodbye!");
    StopREPL();
    return Result<std::string>::Success("Exiting...");
}

void CLIInterface::RegisterBuiltinCommands() {
    builtin_commands_["help"] = [this](const std::vector<SExpression>& args) { return BuiltinHelp(args); };
    builtin_commands_["quit"] = [this](const std::vector<SExpression>& args) { return BuiltinQuit(args); };
    builtin_commands_["exit"] = [this](const std::vector<SExpression>& args) { return BuiltinQuit(args); };
    builtin_commands_["clear"] = [this](const std::vector<SExpression>& args) { return BuiltinClear(args); };
    builtin_commands_["history"] = [this](const std::vector<SExpression>& args) { return BuiltinHistory(args); };
    builtin_commands_["session"] = [this](const std::vector<SExpression>& args) { return BuiltinSession(args); };
    builtin_commands_["config"] = [this](const std::vector<SExpression>& args) { return BuiltinConfig(args); };
    builtin_commands_["status"] = [this](const std::vector<SExpression>& args) { return BuiltinStatus(args); };
    builtin_commands_["connect"] = [this](const std::vector<SExpression>& args) { return BuiltinConnect(args); };
    builtin_commands_["disconnect"] = [this](const std::vector<SExpression>& args) { return BuiltinDisconnect(args); };
}

Result<std::string> CLIInterface::BuiltinHelp(const std::vector<SExpression>& /* args */) {
    std::ostringstream help;
    help << "MCP Debugger - Multi-Context Prompt Debugging Utility\n\n";
    help << "Built-in Commands (prefix with :):\n";
    help << "  :help               - Show this help message\n";
    help << "  :quit, :exit        - Exit the program\n";
    help << "  :clear              - Clear the screen\n";
    help << "  :history            - Show command history\n";
    help << "  :session            - Manage session variables\n";
    help << "  :config             - Show configuration\n";
    help << "  :status             - Show system status\n";
    help << "  :connect            - Connect to debugger\n";
    help << "  :disconnect         - Disconnect from debugger\n\n";
    help << "S-Expression Commands:\n";
    help << "  (llm \"prompt\")       - Send prompt to LLM\n";
    help << "  (dbg \"command\")      - Execute debugger command\n";
    help << "  (log \"message\")      - Log a message\n";
    help << "  (+ 1 2 3)            - Arithmetic operations\n";
    help << "  (read-memory addr)   - Read memory from debugger\n\n";
    help << "Example Session:\n";
    help << "  > :connect\n";
    help << "  > (dbg \"bp main\")\n";
    help << "  > (llm \"Explain this assembly code\" (dbg \"disasm main\"))\n";
    
    return Result<std::string>::Success(help.str());
}

Result<std::string> CLIInterface::BuiltinQuit(const std::vector<SExpression>& /* args */) {
    StopREPL();
    return Result<std::string>::Success("Goodbye!");
}

Result<std::string> CLIInterface::BuiltinStatus(const std::vector<SExpression>& /* args */) {
    std::ostringstream status;
    status << "MCP Debugger Status:\n";
    status << "  Version: " << GetVersionString() << "\n";
    status << "  Mode: " << (repl_running_ ? "Interactive" : "Non-interactive") << "\n";
    
    if (core_engine_) {
        auto debug_bridge = core_engine_->GetDebugBridge();
        if (debug_bridge) {
            status << "  Debugger: " << (debug_bridge->IsConnected() ? "Connected" : "Disconnected") << "\n";
        }
        
        auto llm_engine = core_engine_->GetLLMEngine();
        if (llm_engine) {
            auto providers = llm_engine->GetSupportedProviders();
            status << "  LLM Providers: ";
            for (size_t i = 0; i < providers.size(); ++i) {
                if (i > 0) status << ", ";
                status << providers[i];
            }
            status << "\n";
        }
    }
    
    status << "  History Size: " << command_history_.size() << "\n";
    status << "  Session Variables: " << session_variables_.size() << "\n";
    
    return Result<std::string>::Success(status.str());
}

std::string CLIInterface::ReadInput(const std::string& prompt) {
    PrintPrompt(prompt);
    
    std::string input;
    std::getline(std::cin, input);
    
    return input;
}

void CLIInterface::AddToHistory(const std::string& command) {
    if (command.empty() || !config_.enable_history) {
        return;
    }
    
    // Don't add duplicate consecutive commands
    if (!command_history_.empty() && command_history_.back() == command) {
        return;
    }
    
    command_history_.push_back(command);
    
    // Limit history size
    if (command_history_.size() > config_.max_history_size) {
        command_history_.erase(command_history_.begin());
    }
}

std::string CLIInterface::GetPrompt() const {
    if (core_engine_) {
        auto debug_bridge = core_engine_->GetDebugBridge();
        if (debug_bridge && debug_bridge->IsConnected()) {
            return ColorizeOutput("mcp[dbg]> ", "green");
        }
    }
    
    return ColorizeOutput("mcp> ", "blue");
}

void CLIInterface::PrintOutput(const std::string& output) {
    std::cout << FormatOutput(output, "output") << std::endl;
}

void CLIInterface::PrintError(const std::string& error) {
    std::cerr << ColorizeOutput("Error: ", "red") << error << std::endl;
}

void CLIInterface::PrintInfo(const std::string& info) {
    if (!config_.quiet) {
        std::cout << ColorizeOutput("Info: ", "cyan") << info << std::endl;
    }
}

void CLIInterface::PrintPrompt(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();
}

std::string CLIInterface::ColorizeOutput(const std::string& text, const std::string& color) const {
    if (!ShouldUseColors()) {
        return text;
    }
    
    // ANSI color codes
    std::string color_code;
    if (color == "red") color_code = "\033[31m";
    else if (color == "green") color_code = "\033[32m";
    else if (color == "blue") color_code = "\033[34m";
    else if (color == "cyan") color_code = "\033[36m";
    else if (color == "yellow") color_code = "\033[33m";
    else if (color == "magenta") color_code = "\033[35m";
    else return text;
    
    return color_code + text + "\033[0m";
}

void CLIInterface::ShowBanner() {
    std::cout << ColorizeOutput("=== MCP Debugger ===", "cyan") << std::endl;
    std::cout << "Multi-Context Prompt Debugging Utility" << std::endl;
    std::cout << "Version: " << GetVersionString() << std::endl;
    std::cout << "Type :help for help, :quit to exit" << std::endl;
    std::cout << std::endl;
}

std::string CLIInterface::GetVersionString() const {
    return "1.0.0-alpha";
}

bool CLIInterface::ShouldUseColors() const {
    return config_.enable_colors && isatty(STDOUT_FILENO);
}

void CLIInterface::SetupSignalHandlers() {
#ifdef _WIN32
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
#else
    struct sigaction sa;
    sa.sa_handler = SignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
#endif
}

void CLIInterface::SignalHandler(int signal) {
    if (instance_) {
        instance_->PrintInfo("\nReceived signal " + std::to_string(signal));
        instance_->StopREPL();
    }
}

void CLIInterface::HandleException(const std::exception& ex) {
    PrintError("Exception: " + std::string(ex.what()));
    if (logger_) {
        logger_->LogException(ex, "CLI");
    }
}

void CLIInterface::HandleUnknownException() {
    PrintError("Unknown exception occurred");
    if (logger_) {
        logger_->Log(ILogger::LOG_ERROR, "Unknown exception in CLI");
    }
}

Result<std::string> CLIInterface::PreprocessInput(const std::string& input) {
    std::string processed = input;
    
    // Trim whitespace
    processed.erase(0, processed.find_first_not_of(" \t\n\r"));
    processed.erase(processed.find_last_not_of(" \t\n\r") + 1);
    
    return Result<std::string>::Success(processed);
}

bool CLIInterface::IsMultilineInput(const std::string& input) {
    // Simple check for unmatched parentheses
    int paren_count = 0;
    for (char c : input) {
        if (c == '(') paren_count++;
        else if (c == ')') paren_count--;
    }
    return paren_count > 0;
}

std::string CLIInterface::ReadMultilineInput(const std::string& initial_input) {
    std::string result = initial_input;
    std::string line;
    
    while (IsMultilineInput(result)) {
        PrintPrompt("... ");
        std::getline(std::cin, line);
        result += " " + line;
    }
    
    return result;
}

void CLIInterface::LoadHistory() {
    // TODO: Implement history loading from file
}

void CLIInterface::SaveHistory() {
    // TODO: Implement history saving to file
}

// Stub implementations for remaining methods
Result<void> CLIInterface::ParseCommandLine(int argc, const char* argv[]) {
    // Simple argument parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            ShowUsage();
            config_.mode = Mode::COMMAND;
            config_.command = ":help";
        } else if (arg == "-v" || arg == "--version") {
            ShowVersion();
            config_.mode = Mode::COMMAND;
            config_.command = ":version";
        } else if (arg == "-q" || arg == "--quiet") {
            config_.quiet = true;
        } else if (arg == "-c" || arg == "--command") {
            if (i + 1 < argc) {
                config_.mode = Mode::COMMAND;
                config_.command = argv[++i];
            }
        } else if (arg == "-f" || arg == "--file") {
            if (i + 1 < argc) {
                config_.mode = Mode::SCRIPT;
                config_.script_file = argv[++i];
            }
        }
    }
    
    return Result<void>::Success();
}

Result<void> CLIInterface::LoadConfig(const std::string& /* config_file */) {
    // TODO: Load configuration from file
    return Result<void>::Success();
}

void CLIInterface::ShowUsage() {
    std::cout << "Usage: mcp-debugger [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help         Show help message\n";
    std::cout << "  -v, --version      Show version\n";
    std::cout << "  -q, --quiet        Quiet mode\n";
    std::cout << "  -c, --command CMD  Execute single command\n";
    std::cout << "  -f, --file FILE    Execute script file\n";
}

void CLIInterface::ShowVersion() {
    std::cout << "MCP Debugger " << GetVersionString() << std::endl;
}

// Missing method implementations
bool CLIInterface::IsREPLRunning() const {
    return repl_running_.load();
}

void CLIInterface::SetSessionVariable(const std::string& name, const SExpression& value) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    session_variables_[name] = value;
}

Result<SExpression> CLIInterface::GetSessionVariable(const std::string& name) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    auto it = session_variables_.find(name);
    if (it != session_variables_.end()) {
        return Result<SExpression>::Success(it->second);
    }
    return Result<SExpression>::Error("Session variable not found: " + name);
}

void CLIInterface::ClearSession() {
    std::lock_guard<std::mutex> lock(session_mutex_);
    session_variables_.clear();
    command_history_.clear();
}

void CLIInterface::SaveSession(const std::string& /* filename */) {
    // TODO: Implement session saving to file
}

Result<void> CLIInterface::LoadSession(const std::string& /* filename */) {
    // TODO: Implement session loading from file
    return Result<void>::Success();
}

std::string CLIInterface::FormatOutput(const std::string& content, const std::string& type) {
    if (type.empty()) {
        return content;
    }
    
    // Add formatting based on type
    if (type == "output") {
        return ColorizeOutput(content, "green");
    } else if (type == "error") {
        return ColorizeOutput(content, "red");
    } else if (type == "info") {
        return ColorizeOutput(content, "cyan");
    }
    
    return content;
}

Result<std::string> CLIInterface::HandleHelpCommand(const std::vector<SExpression>& args) {
    return BuiltinHelp(args);
}

Result<std::string> CLIInterface::HandleConfigCommand(const std::vector<SExpression>& args) {
    if (!core_engine_) {
        return Result<std::string>::Error("Core engine not available");
    }
    
    auto config_mgr = core_engine_->GetConfigManager();
    if (!config_mgr) {
        return Result<std::string>::Error("Config manager not available");
    }
    
    if (args.empty()) {
        // Show current configuration
        const Config& cfg = config_mgr->GetConfig();
        std::ostringstream config_str;
        config_str << "Current Configuration:\n";
        config_str << "  API Configs: " << cfg.api_configs.size() << " providers\n";
        config_str << "  Debug Config: " << cfg.debug_config.x64dbg_path << "\n";
        config_str << "  Log Config: " << cfg.log_config.output_path << "\n";
        return Result<std::string>::Success(config_str.str());
    }
    
    // TODO: Implement config modification commands
    return Result<std::string>::Error("Config modification not yet implemented");
}

Result<std::string> CLIInterface::BuiltinClear(const std::vector<SExpression>& /* args */) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
    return Result<std::string>::Success("Screen cleared");
}

Result<std::string> CLIInterface::BuiltinHistory(const std::vector<SExpression>& /* args */) {
    std::ostringstream history_str;
    history_str << "Command History (" << command_history_.size() << " entries):\n";
    
    for (size_t i = 0; i < command_history_.size(); ++i) {
        history_str << "  " << (i + 1) << ": " << command_history_[i] << "\n";
    }
    
    return Result<std::string>::Success(history_str.str());
}

Result<std::string> CLIInterface::BuiltinSession(const std::vector<SExpression>& args) {
    if (args.empty()) {
        // Show session variables
        std::ostringstream session_str;
        session_str << "Session Variables (" << session_variables_.size() << " entries):\n";
        
        std::lock_guard<std::mutex> lock(session_mutex_);
        for (const auto& var : session_variables_) {
            session_str << "  " << var.first << " = ";
            if (std::holds_alternative<std::string>(var.second.value)) {
                session_str << std::get<std::string>(var.second.value);
            } else {
                session_str << "(complex value)";
            }
            session_str << "\n";
        }
        
        return Result<std::string>::Success(session_str.str());
    }
    
    // TODO: Implement session variable manipulation
    return Result<std::string>::Error("Session manipulation not yet implemented");
}

Result<std::string> CLIInterface::BuiltinConfig(const std::vector<SExpression>& args) {
    return HandleConfigCommand(args);
}

Result<std::string> CLIInterface::BuiltinConnect(const std::vector<SExpression>& /* args */) {
    if (!core_engine_) {
        return Result<std::string>::Error("Core engine not available");
    }
    
    auto debug_bridge = core_engine_->GetDebugBridge();
    if (!debug_bridge) {
        return Result<std::string>::Error("Debug bridge not available");
    }
    
    if (debug_bridge->IsConnected()) {
        return Result<std::string>::Success("Already connected to debugger");
    }
    
    auto connect_result = debug_bridge->Connect();
    if (connect_result.IsSuccess()) {
        return Result<std::string>::Success("Connected to debugger");
    } else {
        return Result<std::string>::Error("Failed to connect: " + connect_result.Error());
    }
}

Result<std::string> CLIInterface::BuiltinDisconnect(const std::vector<SExpression>& /* args */) {
    if (!core_engine_) {
        return Result<std::string>::Error("Core engine not available");
    }
    
    auto debug_bridge = core_engine_->GetDebugBridge();
    if (!debug_bridge) {
        return Result<std::string>::Error("Debug bridge not available");
    }
    
    if (!debug_bridge->IsConnected()) {
        return Result<std::string>::Success("Not connected to debugger");
    }
    
    auto disconnect_result = debug_bridge->Disconnect();
    if (disconnect_result.IsSuccess()) {
        return Result<std::string>::Success("Disconnected from debugger");
    } else {
        return Result<std::string>::Error("Failed to disconnect: " + disconnect_result.Error());
    }
}

} // namespace mcp