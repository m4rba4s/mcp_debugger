#include "logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdarg>
#include <filesystem>
#include <algorithm>
#include <string>
#include <chrono>
#include <mutex>
#include <thread>
#include <memory>
#include <utility>
#include <exception>
#include <cstdio>
#include <ctime>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace mcp {

Logger::Logger(const LogConfig& config) : config_(config) {
    InitializeLogFile();
    
    if (async_enabled_) {
        log_thread_ = std::thread(&Logger::ProcessLogQueue, this);
    }
}

Logger::~Logger() {
    shutdown_requested_ = true;
    log_condition_.notify_all();
    
    if (log_thread_.joinable()) {
        log_thread_.join();
    }
    
    Flush();
    
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::Log(Level level, const std::string& message) {
    if (!ShouldLog(level)) {
        return;
    }
    
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.timestamp = std::chrono::system_clock::now();
    entry.thread_id = std::this_thread::get_id();
    
    if (async_enabled_) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_queue_.push(entry);
        log_condition_.notify_one();
    } else {
        WriteLogEntry(entry);
    }
}

void Logger::SetLevel(Level level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    config_.level = static_cast<LogConfig::Level>(level);
}

void Logger::SetOutput(const std::string& path) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    config_.output_path = path;
    
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    InitializeLogFile();
}

void Logger::LogFormatted(Level level, const char* format, ...) {
    if (!ShouldLog(level)) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    // Calculate required size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);
    
    if (size <= 0) {
        va_end(args);
        return;
    }
    
    // Format the string
    std::string buffer(size + 1, '\0');
    vsnprintf(&buffer[0], size + 1, format, args);
    va_end(args);
    
    buffer.resize(size); // Remove null terminator
    Log(level, buffer);
}

void Logger::LogWithContext(Level level, const std::string& message, const std::string& context) {
    if (!ShouldLog(level)) {
        return;
    }
    
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.context = context;
    entry.timestamp = std::chrono::system_clock::now();
    entry.thread_id = std::this_thread::get_id();
    
    if (async_enabled_) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_queue_.push(entry);
        log_condition_.notify_one();
    } else {
        WriteLogEntry(entry);
    }
}


void Logger::LogException(const std::exception& exception, const std::string& context) {
    std::string message = "Exception: ";
    message += exception.what();
    LogWithContext(ILogger::LOG_ERROR, message, context);
}

void Logger::LogMemoryDump(const MemoryDump& dump) {
    std::string serialized = SerializeMemoryDump(dump);
    LogWithContext(ILogger::LOG_DEBUG, serialized, "MEMORY_DUMP");
}

void Logger::LogDebugEvent(const DebugEvent& event) {
    std::string serialized = SerializeDebugEvent(event);
    LogWithContext(ILogger::LOG_INFO, serialized, "DEBUG_EVENT");
}

void Logger::UpdateConfig(const LogConfig& config) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    bool path_changed = (config_.output_path != config.output_path);
    config_ = config;
    
    if (path_changed && log_file_.is_open()) {
        log_file_.close();
        InitializeLogFile();
    }
}

void Logger::Flush() {
    // Блокируем весь объект для flush операции
    std::lock_guard<std::mutex> global_lock(log_mutex_);
    
    if (async_enabled_) {
        // Process remaining entries in queue synchronously
        while (!log_queue_.empty()) {
            LogEntry entry = std::move(log_queue_.front());
            log_queue_.pop();
            
            // Write entry safely under mutex protection
            WriteLogEntryUnsafe(entry);
        }
    }
    
    // Синхронный flush всех потоков вывода (уже под mutex)
    if (log_file_.is_open()) {
        log_file_.flush();
    }
    
    // БЕЗОПАСНОСТЬ: читаем config_ под mutex и фильтруем консольный вывод
    if (config_.console_output) {
        // Flush консоли под защитой mutex для консистентности
        std::cout.flush();
        std::cerr.flush();
    }
}

void Logger::EnableAsyncLogging(bool enable) {
    // Simplified and safer approach to avoid complex locking
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (enable == async_enabled_.load()) {
        return; // No change needed
    }
    
    if (!enable && log_thread_.joinable()) {
        // Graceful shutdown: signal and wait for thread completion
        shutdown_requested_.store(true);
        log_condition_.notify_all();
        
        // Release mutex temporarily to avoid deadlock during join
        log_mutex_.unlock();
        log_thread_.join();
        log_mutex_.lock();
        
        shutdown_requested_.store(false);
    }
    
    async_enabled_.store(enable);
    
    if (enable && !log_thread_.joinable()) {
        log_thread_ = std::thread(&Logger::ProcessLogQueue, this);
    }
}

void Logger::InitializeLogFile() {
    if (!config_.file_output || config_.output_path.empty()) {
        return;
    }
    
    // Create directory if it doesn't exist
    std::filesystem::path log_path(config_.output_path);
    if (log_path.has_parent_path()) {
        std::filesystem::create_directories(log_path.parent_path());
    }
    
    log_file_.open(config_.output_path, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << config_.output_path << '\n';
        return;
    }
    
    // Write startup message
    log_file_ << "=== MCP Debugger Log Started at " 
              << GetTimestamp(std::chrono::system_clock::now()) << " ===\n";
    log_file_.flush();
}

void Logger::ProcessLogQueue() {
    while (!shutdown_requested_) {
        std::unique_lock<std::mutex> lock(log_mutex_);
        log_condition_.wait(lock, [this] { 
            return !log_queue_.empty() || shutdown_requested_; 
        });
        
        while (!log_queue_.empty()) {
            LogEntry entry = log_queue_.front();
            log_queue_.pop();
            lock.unlock();
            
            WriteLogEntry(entry);
            
            lock.lock();
        }
    }
}

void Logger::WriteLogEntry(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    WriteLogEntryUnsafe(entry);
}

void Logger::WriteLogEntryUnsafe(const LogEntry& entry) {
    std::string formatted = FormatLogEntry(entry);
    
    if (config_.console_output) {
        WriteToConsole(formatted, entry.level);
    }
    
    if (config_.file_output && log_file_.is_open()) {
        WriteToFile(formatted);
    }
}

void Logger::RotateLogFile() {
    if (!log_file_.is_open() || current_file_size_ < config_.max_file_size_mb * 1024 * 1024) {
        return;
    }
    
    log_file_.close();
    
    // Rename current file
    std::string rotated_name = config_.output_path + "." + std::to_string(current_file_index_);
    std::filesystem::rename(config_.output_path, rotated_name);
    
    current_file_index_++;
    current_file_size_ = 0;
    
    // Remove old files if exceeding max count
    if (current_file_index_ >= config_.max_files) {
        std::string old_file = config_.output_path + "." + 
                              std::to_string(current_file_index_ - config_.max_files);
        if (std::filesystem::exists(old_file)) {
            std::filesystem::remove(old_file);
        }
    }
    
    InitializeLogFile();
}

std::string Logger::FormatLogEntry(const LogEntry& entry) const {
    std::ostringstream oss;
    
    // Replace format placeholders
    std::string format = config_.format;
    
    // Timestamp
    size_t pos = format.find("{timestamp}");
    if (pos != std::string::npos) {
        format.replace(pos, 11, GetTimestamp(entry.timestamp));
    }
    
    // Level
    pos = format.find("{level}");
    if (pos != std::string::npos) {
        format.replace(pos, 7, LevelToString(entry.level));
    }
    
    // Thread ID
    pos = format.find("{thread}");
    if (pos != std::string::npos) {
        std::ostringstream thread_oss;
        thread_oss << entry.thread_id;
        format.replace(pos, 8, thread_oss.str());
    }
    
    // Context
    pos = format.find("{context}");
    if (pos != std::string::npos) {
        std::string context_str = entry.context.empty() ? "" : "[" + entry.context + "] ";
        format.replace(pos, 9, context_str);
    }
    
    // Message
    pos = format.find("{message}");
    if (pos != std::string::npos) {
        format.replace(pos, 9, entry.message);
    }
    
    return format;
}

std::string Logger::LevelToString(Level level) const {
    switch (level) {
        case ILogger::LOG_DEBUG: return "DEBUG";
        case ILogger::LOG_INFO:  return "INFO ";
        case ILogger::LOG_WARN:  return "WARN ";
        case ILogger::LOG_ERROR: return "ERROR";
        case ILogger::LOG_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::GetTimestamp(const std::chrono::system_clock::time_point& time) const {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
#ifdef _WIN32
    struct tm local_time;
    localtime_s(&local_time, &time_t);
    oss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
#else
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
#endif
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

bool Logger::ShouldLog(Level level) const {
    return static_cast<LogConfig::Level>(level) >= config_.level;
}

void Logger::WriteToConsole(const std::string& formatted_message, Level level) {
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // White
    
    switch (level) {
        case ILogger::LOG_DEBUG: color = FOREGROUND_BLUE; break;
        case ILogger::LOG_INFO:  color = FOREGROUND_GREEN; break;
        case ILogger::LOG_WARN:  color = FOREGROUND_RED | FOREGROUND_GREEN; break;
        case ILogger::LOG_ERROR: color = FOREGROUND_RED; break;
        case ILogger::LOG_FATAL: color = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
        default: break;
    }
    
    SetConsoleTextAttribute(console, color);
    std::cout << formatted_message << '\n';
    SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    const char* color_code = "\033[0m"; // Reset
    
    switch (level) {
        case ILogger::LOG_DEBUG: color_code = "\033[36m"; break; // Cyan
        case ILogger::LOG_INFO:  color_code = "\033[32m"; break; // Green
        case ILogger::LOG_WARN:  color_code = "\033[33m"; break; // Yellow
        case ILogger::LOG_ERROR: color_code = "\033[31m"; break; // Red
        case ILogger::LOG_FATAL: color_code = "\033[35m"; break; // Magenta
    }
    
    std::cout << color_code << formatted_message << "\033[0m\n";
#endif
}

void Logger::WriteToFile(const std::string& formatted_message) {
    log_file_ << formatted_message << '\n';
    current_file_size_ += formatted_message.length() + 1;
    
    if (current_file_size_ >= config_.max_file_size_mb * 1024 * 1024) {
        RotateLogFile();
    }
}

std::string Logger::SerializeMemoryDump(const MemoryDump& dump) const {
    std::ostringstream oss;
    oss << "MemoryDump{";
    oss << "base=0x" << std::hex << dump.base_address;
    oss << ", size=" << std::dec << dump.size;
    oss << ", module=" << dump.module_name;
    oss << ", data_preview=";
    
    // Show first 32 bytes
    const size_t max_preview = 32;
    size_t preview_size = (dump.data.size() < max_preview) ? dump.data.size() : max_preview;
    for (size_t i = 0; i < preview_size; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(dump.data[i]);
        if (i < preview_size - 1) oss << " ";
    }
    
    if (dump.data.size() > 32) {
        oss << "...";
    }
    
    oss << "}";
    return oss.str();
}

std::string Logger::SerializeDebugEvent(const DebugEvent& event) const {
    std::ostringstream oss;
    oss << "DebugEvent{";
    oss << "type=" << static_cast<int>(event.type);
    oss << ", addr=0x" << std::hex << event.address;
    oss << ", pid=" << std::dec << event.process_id;
    oss << ", tid=" << event.thread_id;
    oss << ", module=" << event.module_name;
    oss << ", desc=" << event.description;
    oss << "}";
    return oss.str();
}

// LoggerManager implementation
std::shared_ptr<Logger> LoggerManager::instance_;
std::mutex LoggerManager::instance_mutex_;

void LoggerManager::Initialize(const LogConfig& config) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    instance_ = std::make_shared<Logger>(config);
}

std::shared_ptr<Logger> LoggerManager::GetInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::make_shared<Logger>();
    }
    return instance_;
}

void LoggerManager::Shutdown() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        instance_->Flush();
        instance_.reset();
    }
}

} // namespace mcp