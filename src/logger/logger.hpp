#pragma once

#include "mcp/interfaces.hpp"
#include "mcp/types.hpp"
#include <memory>
#include <mutex>
#include <fstream>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>

namespace mcp {

class Logger : public ILogger {
public:
    explicit Logger(const LogConfig& config = LogConfig{});
    ~Logger() override;

    // ILogger implementation
    void Log(Level level, const std::string& message) override;
    void SetLevel(Level level) override;
    void SetOutput(const std::string& path) override;

    // Enhanced logging methods
    void LogFormatted(Level level, const char* format, ...) override;
    void LogWithContext(Level level, const std::string& message, const std::string& context);
    void LogException(const std::exception& exception, const std::string& context = "") override;
    void LogMemoryDump(const MemoryDump& dump);
    void LogDebugEvent(const DebugEvent& event);
    
    // Configuration
    void UpdateConfig(const LogConfig& config);
    void Flush();
    void EnableAsyncLogging(bool enable);

private:
    struct LogEntry {
        Level level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::thread::id thread_id;
        std::string context;
    };

    LogConfig config_;
    mutable std::mutex log_mutex_;
    std::ofstream log_file_;
    
    // Async logging
    std::queue<LogEntry> log_queue_;
    std::thread log_thread_;
    std::condition_variable log_condition_;
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<bool> async_enabled_{true};
    
    // File rotation
    size_t current_file_size_ = 0;
    int current_file_index_ = 0;
    
    void InitializeLogFile();
    void ProcessLogQueue();
    void WriteLogEntry(const LogEntry& entry);
    void WriteLogEntryUnsafe(const LogEntry& entry); // Thread-unsafe version for internal use
    void RotateLogFile();
    
    std::string FormatLogEntry(const LogEntry& entry) const;
    std::string LevelToString(Level level) const;
    std::string GetTimestamp(const std::chrono::system_clock::time_point& time) const;
    bool ShouldLog(Level level) const;
    
    void WriteToConsole(const std::string& formatted_message, Level level);
    void WriteToFile(const std::string& formatted_message);
    
    // Helper methods for structured data
    std::string SerializeMemoryDump(const MemoryDump& dump) const;
    std::string SerializeDebugEvent(const DebugEvent& event) const;
};

// Global logger instance management
class LoggerManager {
public:
    static void Initialize(const LogConfig& config = LogConfig{});
    static std::shared_ptr<Logger> GetInstance();
    static void Shutdown();

private:
    static std::shared_ptr<Logger> instance_;
    static std::mutex instance_mutex_;
};

// Convenience macros
#define LOG_DEBUG(msg) do { auto logger = LoggerManager::GetInstance(); if (logger) logger->Log(ILogger::Level::DEBUG, msg); } while(0)
#define LOG_INFO(msg) do { auto logger = LoggerManager::GetInstance(); if (logger) logger->Log(ILogger::Level::INFO, msg); } while(0)
#define LOG_WARN(msg) do { auto logger = LoggerManager::GetInstance(); if (logger) logger->Log(ILogger::Level::WARN, msg); } while(0)
#define LOG_ERROR(msg) do { auto logger = LoggerManager::GetInstance(); if (logger) logger->Log(ILogger::Level::ERROR, msg); } while(0)
#define LOG_FATAL(msg) do { auto logger = LoggerManager::GetInstance(); if (logger) logger->Log(ILogger::Level::FATAL, msg); } while(0)

#define LOG_DEBUG_FMT(fmt, ...) do { auto logger = LoggerManager::GetInstance(); if (logger) logger->LogFormatted(ILogger::Level::DEBUG, fmt, __VA_ARGS__); } while(0)
#define LOG_INFO_FMT(fmt, ...) do { auto logger = LoggerManager::GetInstance(); if (logger) logger->LogFormatted(ILogger::Level::INFO, fmt, __VA_ARGS__); } while(0)
#define LOG_WARN_FMT(fmt, ...) do { auto logger = LoggerManager::GetInstance(); if (logger) logger->LogFormatted(ILogger::Level::WARN, fmt, __VA_ARGS__); } while(0)
#define LOG_ERROR_FMT(fmt, ...) do { auto logger = LoggerManager::GetInstance(); if (logger) logger->LogFormatted(ILogger::Level::ERROR, fmt, __VA_ARGS__); } while(0)
#define LOG_FATAL_FMT(fmt, ...) do { auto logger = LoggerManager::GetInstance(); if (logger) logger->LogFormatted(ILogger::Level::FATAL, fmt, __VA_ARGS__); } while(0)

} // namespace mcp