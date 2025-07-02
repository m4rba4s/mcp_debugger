#include "x64dbg_bridge.hpp"
#include <iostream>
#include <sstream>
#include <regex>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <string>

namespace mcp {

#ifdef _WIN32
bool X64DbgBridge::plugin_initialized_ = false;
X64DbgBridge* X64DbgBridge::plugin_instance_ = nullptr;
#endif

X64DbgBridge::X64DbgBridge(std::shared_ptr<ILogger> logger) 
    : logger_(logger) {
    
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "X64DbgBridge initialized");
    }

#ifdef _WIN32
    InitializeWinAPI();
#endif
}

X64DbgBridge::~X64DbgBridge() {
    Disconnect();
    
    if (event_thread_running_) {
        event_thread_running_ = false;
        event_condition_.notify_all();
        if (event_thread_.joinable()) {
            event_thread_.join();
        }
    }
    
#ifdef _WIN32
    // КРИТИЧНО: освобождаем Winsock ресурсы
    if (winsock_initialized_) {
        WSACleanup();
        winsock_initialized_ = false;
    }
#endif
    
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "X64DbgBridge destroyed");
    }
}

Result<void> X64DbgBridge::Connect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (connected_) {
        return Result<void>::Success();
    }
    
    Result<void> connect_result;
    
    switch (connection_mode_) {
        case ConnectionMode::PLUGIN:
            connect_result = ConnectAsPlugin();
            break;
        case ConnectionMode::EXTERNAL:
            connect_result = ConnectExternal();
            break;
        case ConnectionMode::PIPE:
            connect_result = ConnectPipe();
            break;
        case ConnectionMode::TCP:
            connect_result = ConnectTCP();
            break;
        default:
            return Result<void>::Error("Invalid connection mode");
    }
    
    if (connect_result.IsSuccess()) {
        connected_ = true;
        
        // Start event processing thread
        event_thread_running_ = true;
        event_thread_ = std::thread(&X64DbgBridge::EventProcessingLoop, this);
        
        if (logger_) {
            logger_->Log(ILogger::LOG_INFO, "Connected to x64dbg");
        }
    }
    
    return connect_result;
}

Result<void> X64DbgBridge::Disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (!connected_) {
        return Result<void>::Success();
    }
    
    // Stop event processing
    event_thread_running_ = false;
    event_condition_.notify_all();
    
    if (event_thread_.joinable()) {
        event_thread_.join();
    }
    
    DisconnectInternal();
    connected_ = false;
    
    if (logger_) {
        logger_->Log(ILogger::LOG_INFO, "Disconnected from x64dbg");
    }
    
    return Result<void>::Success();
}

Result<std::string> X64DbgBridge::ExecuteCommand(const std::string& command) {
    if (!connected_) {
        return Result<std::string>::Error("Not connected to debugger");
    }
    
    if (command.empty()) {
        return Result<std::string>::Error("Command cannot be empty");
    }
    
    auto send_result = SendCommand(command);
    if (!send_result.IsSuccess()) {
        return send_result;
    }
    
    auto parse_result = ParseCommandResponse(send_result.Value());
    if (!parse_result.IsSuccess()) {
        return parse_result;
    }
    
    if (logger_) {
        logger_->LogFormatted(ILogger::LOG_DEBUG, "Executed command: %s", command.c_str());
    }
    
    return parse_result;
}

Result<MemoryDump> X64DbgBridge::ReadMemory(uintptr_t address, size_t size) {
    if (!connected_) {
        return Result<MemoryDump>::Error("Not connected to debugger");
    }
    
    auto validate_result = ValidateMemoryAccess(address, size);
    if (!validate_result.IsSuccess()) {
        return Result<MemoryDump>::Error(validate_result.Error());
    }
    
    auto data_result = ReadMemoryRaw(address, size);
    if (!data_result.IsSuccess()) {
        return Result<MemoryDump>::Error(data_result.Error());
    }
    
    MemoryDump dump;
    dump.base_address = address;
    dump.data = data_result.Value();
    dump.size = size;
    dump.timestamp = std::chrono::system_clock::now();
    
    // Try to identify the module
    auto symbol_result = GetSymbolAt(address);
    if (symbol_result.IsSuccess()) {
        dump.module_name = symbol_result.Value();
    }
    
    return Result<MemoryDump>::Success(dump);
}

Result<void> X64DbgBridge::SetBreakpoint(uintptr_t address) {
    if (!connected_) {
        return Result<void>::Error("Not connected to debugger");
    }
    
    std::string command = "bp " + AddressToString(address);
    auto result = ExecuteCommand(command);
    
    if (result.IsSuccess()) {
        if (logger_) {
            logger_->LogFormatted(ILogger::LOG_INFO, "Set breakpoint at 0x%llx", (unsigned long long)address);
        }
        return Result<void>::Success();
    } else {
        return Result<void>::Error("Failed to set breakpoint: " + result.Error());
    }
}

void X64DbgBridge::RegisterEventHandler(std::function<void(const DebugEvent&)> handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    
    EventHandlerEntry entry;
    entry.id = next_handler_id_++;
    entry.handler = handler;
    
    event_handlers_.push_back(entry);
    
    if (logger_) {
        logger_->LogFormatted(ILogger::LOG_DEBUG, "Registered event handler with ID %u", entry.id);
    }
}

bool X64DbgBridge::IsConnected() const {
    return connected_.load();
}

Result<void> X64DbgBridge::SetConnectionMode(ConnectionMode mode) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    
    if (connected_) {
        return Result<void>::Error("Cannot change connection mode while connected");
    }
    
    connection_mode_ = mode;
    return Result<void>::Success();
}

Result<void> X64DbgBridge::SetDebuggerPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    debugger_path_ = path;
    return Result<void>::Success();
}

Result<void> X64DbgBridge::SetConnectionTimeout(int timeout_ms) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    connection_timeout_ms_ = timeout_ms;
    return Result<void>::Success();
}

Result<std::string> X64DbgBridge::GetSymbolAt(uintptr_t address) {
    if (!connected_) {
        return Result<std::string>::Error("Not connected to debugger");
    }
    
    // Mock implementation - return generic symbol info
    std::ostringstream oss;
    oss << "symbol_at_" << std::hex << address;
    return Result<std::string>::Success(oss.str());
}

Result<std::vector<uint8_t>> X64DbgBridge::ReadMemoryRaw(uintptr_t address, size_t size) {
    if (!connected_) {
        return Result<std::vector<uint8_t>>::Error("Not connected to debugger");
    }
    
#ifdef _WIN32
    if (connection_mode_ == ConnectionMode::EXTERNAL && process_handle_ != INVALID_HANDLE_VALUE) {
        return ReadProcessMemoryWin(address, size);
    }
#endif
    
    // Fallback to command-based memory reading
    std::string command = FormatMemoryCommand("dump", address, size);
    auto result = ExecuteCommand(command);
    
    if (!result.IsSuccess()) {
        return Result<std::vector<uint8_t>>::Error(result.Error());
    }
    
    auto data = ParseHexData(result.Value());
    return Result<std::vector<uint8_t>>::Success(data);
}

Result<void> X64DbgBridge::WriteMemory(uintptr_t address, const std::vector<uint8_t>& data) {
    if (!connected_) {
        return Result<void>::Error("Not connected to debugger");
    }
    
#ifdef _WIN32
    if (connection_mode_ == ConnectionMode::EXTERNAL && process_handle_ != INVALID_HANDLE_VALUE) {
        return WriteProcessMemoryWin(address, data);
    }
#endif
    
    // Convert data to hex string
    std::ostringstream hex_stream;
    for (uint8_t byte : data) {
        hex_stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    
    std::string command = "fill " + AddressToString(address) + " " + hex_stream.str();
    auto result = ExecuteCommand(command);
    
    if (result.IsSuccess()) {
        return Result<void>::Success();
    } else {
        return Result<void>::Error("Failed to write memory: " + result.Error());
    }
}

Result<uintptr_t> X64DbgBridge::GetRegisterValue(const std::string& register_name) {
    if (!connected_) {
        return Result<uintptr_t>::Error("Not connected to debugger");
    }
    
    std::string command = "r " + register_name;
    auto result = ExecuteCommand(command);
    
    if (!result.IsSuccess()) {
        return Result<uintptr_t>::Error(result.Error());
    }
    
    // Parse register value from response
    std::regex reg_regex(register_name + R"(\s*=\s*([0-9A-Fa-f]+))");
    std::smatch match;
    
    if (std::regex_search(result.Value(), match, reg_regex)) {
        try {
            uintptr_t value = std::stoull(match[1].str(), nullptr, 16);
            return Result<uintptr_t>::Success(value);
        } catch (...) {
            return Result<uintptr_t>::Error("Failed to parse register value");
        }
    }
    
    return Result<uintptr_t>::Error("Register value not found in response");
}

Result<void> X64DbgBridge::ConnectAsPlugin() {
#ifdef _WIN32
    if (plugin_initialized_ && plugin_instance_) {
        return Result<void>::Success();
    }
#endif
    return Result<void>::Error("Plugin mode not available");
}

Result<void> X64DbgBridge::ConnectExternal() {
    if (debugger_path_.empty()) {
        auto found_path = X64DbgBridgeFactory::FindX64DbgExecutable();
        if (found_path.empty()) {
            return Result<void>::Error("x64dbg executable not found");
        }
        debugger_path_ = found_path;
    }
    
    // TODO: Launch x64dbg process and establish communication
    // For now, return success as a stub
    
    if (logger_) {
        logger_->LogFormatted(ILogger::LOG_INFO, "Connecting to x64dbg at: %s", debugger_path_.c_str());
    }
    
    return Result<void>::Success();
}

Result<void> X64DbgBridge::ConnectPipe() {
#ifdef _WIN32
    // Try to connect to x64dbg named pipe
    std::string pipe_name = R"(\\.\pipe\x64dbg_bridge)";
    
    pipe_handle_ = CreateFileA(
        pipe_name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    
    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return Result<void>::Error("Failed to connect to pipe: " + std::to_string(error));
    }
    
    return Result<void>::Success();
#else
    return Result<void>::Error("Named pipe connection not supported on this platform");
#endif
}

Result<void> X64DbgBridge::ConnectTCP() {
    // TODO: Implement TCP connection to x64dbg
    return Result<void>::Error("TCP connection not yet implemented");
}

void X64DbgBridge::DisconnectInternal() {
#ifdef _WIN32
    if (process_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(process_handle_);
        process_handle_ = INVALID_HANDLE_VALUE;
    }
    
    if (pipe_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
    }
    
    if (tcp_socket_ != INVALID_SOCKET) {
        closesocket(tcp_socket_);
        tcp_socket_ = INVALID_SOCKET;
    }
#endif
}

Result<std::string> X64DbgBridge::SendCommand(const std::string& command) {
    if (!connected_) {
        return Result<std::string>::Error("Not connected");
    }
    
    std::string escaped_command = EscapeCommand(command);
    
    // Mock implementation - in production would send to actual debugger
    if (logger_) {
        logger_->LogFormatted(ILogger::LOG_DEBUG, "Sending command: %s", escaped_command.c_str());
    }
    
    // Simulate command responses
    if (command.substr(0, 3) == "bp ") {
        return Result<std::string>::Success("Breakpoint set successfully");
    } else if (command.substr(0, 5) == "dump ") {
        return Result<std::string>::Success("48 89 E5 48 83 EC 20 C7 45 FC 00 00 00 00");
    } else if (command.substr(0, 2) == "r ") {
        return Result<std::string>::Success("RAX=0000000000401000");
    } else {
        return Result<std::string>::Success("Command executed");
    }
}

Result<std::string> X64DbgBridge::ParseCommandResponse(const std::string& raw_response) {
    // Simple response parsing - in production would handle x64dbg output format
    std::string response = raw_response;
    
    // Remove any control characters
    response.erase(std::remove_if(response.begin(), response.end(), 
                                 [](char c) { return c < 32 && c != '\t' && c != '\n'; }), 
                  response.end());
    
    return Result<std::string>::Success(response);
}

void X64DbgBridge::EventProcessingLoop() {
    while (event_thread_running_) {
        std::unique_lock<std::mutex> lock(event_queue_mutex_);
        event_condition_.wait(lock, [this] { 
            return !event_queue_.empty() || !event_thread_running_; 
        });
        
        while (!event_queue_.empty() && event_thread_running_) {
            DebugEvent event = event_queue_.front();
            event_queue_.pop();
            lock.unlock();
            
            NotifyEventHandlers(event);
            
            lock.lock();
        }
    }
}

void X64DbgBridge::NotifyEventHandlers(const DebugEvent& event) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    
    for (const auto& handler_entry : event_handlers_) {
        try {
            handler_entry.handler(event);
        } catch (...) {
            if (logger_) {
                logger_->LogFormatted(ILogger::LOG_ERROR, "Exception in event handler %u", handler_entry.id);
            }
        }
    }
}

Result<void> X64DbgBridge::ValidateMemoryAccess(uintptr_t address, size_t size) {
    if (size == 0) {
        return Result<void>::Error("Size cannot be zero");
    }
    
    if (size > 1024 * 1024) { // 1MB limit
        return Result<void>::Error("Size too large (max 1MB)");
    }
    
    if (!IsValidAddress(address)) {
        return Result<void>::Error("Invalid memory address");
    }
    
    return Result<void>::Success();
}

std::string X64DbgBridge::FormatMemoryCommand(const std::string& operation, 
                                            uintptr_t address, size_t size) {
    std::ostringstream cmd;
    cmd << operation << " " << AddressToString(address) << " " << std::hex << size;
    return cmd.str();
}

std::vector<uint8_t> X64DbgBridge::ParseHexData(const std::string& hex_string) {
    // ЗАЩИТА ОТ DOS: ограничиваем максимальный размер parsing
    constexpr size_t MAX_HEX_LENGTH = 2 * 1024 * 1024; // 2MB of hex = 1MB of data
    if (hex_string.length() > MAX_HEX_LENGTH) {
        if (logger_) {
            logger_->LogFormatted(ILogger::LOG_ERROR, "ParseHexData: input too large (%zu bytes), truncating to %zu", hex_string.length(), MAX_HEX_LENGTH);
        }
        // Возвращаем пустой вектор вместо обрезки для безопасности
        return {};
    }
    
    std::vector<uint8_t> data;
    // Резервируем память заранее для оптимизации
    data.reserve(hex_string.length() / 2 + 1);
    
    // Парсинг без потокового ввода для безопасности
    for (size_t i = 0; i + 1 < hex_string.length(); i += 2) {
        if (data.size() >= 1024 * 1024) { // Дополнительная проверка на 1MB
            if (logger_) {
                logger_->Log(ILogger::LOG_WARN, "ParseHexData: reached 1MB limit, stopping parse");
            }
            break;
        }
        
        char hex_chars[3] = {hex_string[i], hex_string[i + 1], '\0'};
        
        // Проверяем что это валидные hex символы
        if (!std::isxdigit(hex_chars[0]) || !std::isxdigit(hex_chars[1])) {
            continue; // Пропускаем невалидные пары
        }
        
        try {
            unsigned long value = std::stoul(hex_chars, nullptr, 16);
            if (value <= 0xFF) { // Дополнительная проверка диапазона
                data.push_back(static_cast<uint8_t>(value));
            }
        } catch (const std::exception&) {
            // Безопасно игнорируем invalid hex pairs
            continue;
        }
    }
    
    return data;
}

std::string X64DbgBridge::EscapeCommand(const std::string& command) {
    // КРИТИЧНО: защита от command injection атак
    constexpr size_t MAX_COMMAND_LENGTH = 4096;
    if (command.length() > MAX_COMMAND_LENGTH) {
        if (logger_) {
            logger_->LogFormatted(ILogger::LOG_ERROR, "EscapeCommand: command too long (%zu chars), rejecting", command.length());
        }
        return ""; // Возвращаем пустую строку для безопасности
    }
    
    std::string escaped;
    escaped.reserve(command.length() * 2); // Резервируем место для escape символов
    
    // Фильтруем опасные символы
    for (char c : command) {
        switch (c) {
            case ';':  // Разделитель команд
            case '|':  // Pipe
            case '&':  // Background/AND
            case '`':  // Command substitution
            case '$':  // Variable expansion
            case '(':  // Subshell
            case ')':  // Subshell
            case '<':  // Redirect
            case '>':  // Redirect
            case '"':  // Quote
            case '\'': // Quote
            case '\n': // Newline
            case '\r': // Carriage return
            case '\0': // Null byte
                // Заменяем опасные символы на underscore
                escaped += '_';
                break;
            default:
                // Разрешаем только printable ASCII символы
                if (c >= 32 && c <= 126) {
                    escaped += c;
                } else {
                    escaped += '_';
                }
                break;
        }
    }
    
    return escaped;
}

bool X64DbgBridge::IsValidAddress(uintptr_t address) {
    // Basic validation - in production would check against process memory layout
    return address != 0 && address < 0x7FFFFFFEFFFF; // Windows user space limit
}

std::string X64DbgBridge::AddressToString(uintptr_t address) {
    std::ostringstream oss;
    oss << "0x" << std::hex << address;
    return oss.str();
}

uintptr_t X64DbgBridge::StringToAddress(const std::string& address_str) {
    if (address_str.empty()) {
        if (logger_) {
            logger_->Log(ILogger::LOG_ERROR, "StringToAddress: empty address string");
        }
        return 0;
    }
    
    try {
        // БЕЗОПАСНОСТЬ: проверяем длину строки
        if (address_str.length() > 20) { // max length for 64-bit hex + 0x
            if (logger_) {
                logger_->Log(ILogger::LOG_ERROR, "StringToAddress: address string too long");
            }
            return 0;
        }
        
        std::string hex_part;
        if (address_str.length() >= 2 && 
            (address_str.substr(0, 2) == "0x" || address_str.substr(0, 2) == "0X")) {
            hex_part = address_str.substr(2);
        } else {
            hex_part = address_str;
        }
        
        // Проверяем что все символы валидные hex
        for (char c : hex_part) {
            if (!std::isxdigit(c)) {
                if (logger_) {
                    logger_->LogFormatted(ILogger::LOG_ERROR, "StringToAddress: invalid hex character '%c'", c);
                }
                return 0;
            }
        }
        
        return std::stoull(hex_part, nullptr, 16);
        
    } catch (const std::out_of_range&) {
        if (logger_) {
            logger_->Log(ILogger::LOG_ERROR, "StringToAddress: address out of range");
        }
        return 0;
    } catch (const std::invalid_argument&) {
        if (logger_) {
            logger_->Log(ILogger::LOG_ERROR, "StringToAddress: invalid address format");
        }
        return 0;
    } catch (...) {
        if (logger_) {
            logger_->Log(ILogger::LOG_ERROR, "StringToAddress: unknown error");
        }
        return 0;
    }
}

#ifdef _WIN32
Result<void> X64DbgBridge::InitializeWinAPI() {
    // БЕЗОПАСНОСТЬ: проверяем что Winsock еще не инициализирован
    if (winsock_initialized_) {
        return Result<void>::Success(); // Уже инициализирован
    }
    
    // Initialize Winsock for TCP connections
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return Result<void>::Error("WSAStartup failed: " + std::to_string(result));
    }
    
    // КРИТИЧНО: отмечаем успешную инициализацию
    winsock_initialized_ = true;
    
    return Result<void>::Success();
}

Result<std::vector<uint8_t>> X64DbgBridge::ReadProcessMemoryWin(uintptr_t address, size_t size) {
    if (process_handle_ == INVALID_HANDLE_VALUE) {
        return Result<std::vector<uint8_t>>::Error("Invalid process handle");
    }
    
    std::vector<uint8_t> buffer(size);
    SIZE_T bytes_read = 0;
    
    BOOL success = ReadProcessMemory(
        process_handle_,
        reinterpret_cast<LPCVOID>(address),
        buffer.data(),
        size,
        &bytes_read
    );
    
    if (!success) {
        DWORD error = GetLastError();
        return Result<std::vector<uint8_t>>::Error("ReadProcessMemory failed: " + std::to_string(error));
    }
    
    buffer.resize(bytes_read);
    return Result<std::vector<uint8_t>>::Success(buffer);
}

Result<void> X64DbgBridge::WriteProcessMemoryWin(uintptr_t address, const std::vector<uint8_t>& data) {
    if (process_handle_ == INVALID_HANDLE_VALUE) {
        return Result<void>::Error("Invalid process handle");
    }
    
    SIZE_T bytes_written = 0;
    
    BOOL success = WriteProcessMemory(
        process_handle_,
        reinterpret_cast<LPVOID>(address),
        data.data(),
        data.size(),
        &bytes_written
    );
    
    if (!success || bytes_written != data.size()) {
        DWORD error = GetLastError();
        return Result<void>::Error("WriteProcessMemory failed: " + std::to_string(error));
    }
    
    return Result<void>::Success();
}
#endif

// Factory implementations
std::unique_ptr<X64DbgBridge> X64DbgBridgeFactory::CreateBridge(X64DbgBridge::ConnectionMode mode,
                                                               std::shared_ptr<ILogger> logger) {
    auto bridge = std::make_unique<X64DbgBridge>(logger);
    bridge->SetConnectionMode(mode);
    return bridge;
}

std::unique_ptr<X64DbgBridge> X64DbgBridgeFactory::CreateFromConfig(const DebugConfig& config,
                                                                   std::shared_ptr<ILogger> logger) {
    auto bridge = std::make_unique<X64DbgBridge>(logger);
    bridge->SetDebuggerPath(config.x64dbg_path);
    bridge->SetConnectionTimeout(config.connection_timeout_ms);
    
    // Auto-detect best connection mode
    auto mode = DetectBestConnectionMode();
    bridge->SetConnectionMode(mode);
    
    return bridge;
}

X64DbgBridge::ConnectionMode X64DbgBridgeFactory::DetectBestConnectionMode() {
    // Check if running as plugin first
#ifdef _WIN32
    if (X64DbgBridge::plugin_initialized_) {
        return X64DbgBridge::ConnectionMode::PLUGIN;
    }
#endif
    
    // Check if x64dbg is running
    if (IsX64DbgRunning()) {
        return X64DbgBridge::ConnectionMode::PIPE;
    }
    
    // Default to external mode
    return X64DbgBridge::ConnectionMode::EXTERNAL;
}

bool X64DbgBridgeFactory::IsX64DbgRunning() {
#ifdef _WIN32
    // Check for x64dbg process
    // TODO: Implement actual process enumeration
    return false;
#else
    return false;
#endif
}

std::string X64DbgBridgeFactory::FindX64DbgExecutable() {
    // TODO: Search common installation paths for x64dbg
    std::vector<std::string> search_paths = {
        "C:\\x64dbg\\release\\x64\\x64dbg.exe",
        "C:\\Program Files\\x64dbg\\x64dbg.exe",
        "C:\\Program Files (x86)\\x64dbg\\x64dbg.exe"
    };
    
    for (const auto& path : search_paths) {
        // TODO: Check if file exists
        (void)path; // Suppress unused variable warning
    }
    
    return "";
}

Result<std::string> X64DbgBridge::GetDisassembly(uintptr_t address) {
    std::string msg = "Fetching disassembly at address " + AddressToString(address);
    logger_->Log(ILogger::Level::INFO, msg);
    
    std::string command = "disasm " + AddressToString(address);
    // In a real implementation, we would execute this command via the bridge
    // and parse the result.
    return mcp::Result<std::string>::Success("mov rax, rcx\nadd rax, 1\nret");
}

} // namespace mcp