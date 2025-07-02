#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <future>
#include <unordered_map>
#include <optional>
#include <stdexcept>

namespace mcp {

// Forward declarations
struct LLMRequest;
struct LLMResponse;
struct DebugEvent;
struct MemoryDump;
struct SExpression;
struct Config;

// Result type for error handling
template<typename T>
class Result {
private:
    struct SuccessTag {};
    struct ErrorTag {};
    
public:
    // Default constructor creates error state - SAFE for std::future
    Result() : value_{}, error_("Uninitialized Result") {}
    
    // DELETED - prevents accidental default construction
    // Result() = delete;
    
    static Result Success(T value) { return Result(SuccessTag{}, std::move(value)); }
    static Result Error(const std::string& error) { return Result(ErrorTag{}, error); }
    
    bool IsSuccess() const noexcept { return !error_.has_value(); }
    bool IsError() const noexcept { return error_.has_value(); }
    
    // БЕЗОПАСНЫЙ доступ к значению с проверкой
    const T& Value() const {
        if (IsError()) {
            throw std::runtime_error("Attempting to access Value() on error Result: " + *error_);
        }
        return value_;
    }
    
    // БЕЗОПАСНЫЙ доступ к ошибке
    const std::string& Error() const {
        if (IsSuccess()) {
            throw std::runtime_error("Attempting to access Error() on success Result");
        }
        return *error_;
    }
    
    // Безопасные альтернативы
    T ValueOr(const T& default_value) const noexcept {
        return IsSuccess() ? value_ : default_value;
    }
    
    T ValueOr(T&& default_value) const noexcept {
        return IsSuccess() ? value_ : std::move(default_value);
    }

private:
    explicit Result(SuccessTag, T value) : value_(std::move(value)) {}
    explicit Result(ErrorTag, const std::string& error) : error_(error) {}
    
    T value_;
    std::optional<std::string> error_;
};

// Specialization for Result<void>
template<>
class Result<void> {
private:
    struct ErrorTag {};
    
public:
    // Default constructor for std::future - defaults to success for void
    Result() noexcept {}
    
    static Result Success() noexcept { return Result(); }
    static Result Error(const std::string& error) { return Result(ErrorTag{}, error); }
    
    bool IsSuccess() const noexcept { return !error_.has_value(); }
    bool IsError() const noexcept { return error_.has_value(); }
    
    const std::string& Error() const {
        if (IsSuccess()) {
            throw std::runtime_error("Attempting to access Error() on success Result<void>");
        }
        return *error_;
    }

private:
    explicit Result(ErrorTag, const std::string& error) : error_(error) {}
    
    std::optional<std::string> error_;
};

// Core LLM Engine Interface
class ILLMEngine {
public:
    virtual ~ILLMEngine() = default;
    
    virtual std::future<Result<LLMResponse>> SendRequest(const LLMRequest& request) = 0;
    virtual Result<LLMResponse> SendRequestSync(const LLMRequest& request) = 0;
    virtual Result<void> SetAPIKey(const std::string& provider, const std::string& key) = 0;
    virtual std::vector<std::string> GetSupportedProviders() const = 0;
    virtual Result<void> ValidateConnection(const std::string& provider) = 0;
};

// X64Dbg Bridge Interface  
class IX64DbgBridge {
public:
    virtual ~IX64DbgBridge() = default;
    
    virtual Result<void> Connect() = 0;
    virtual Result<void> Disconnect() = 0;
    virtual Result<std::string> ExecuteCommand(const std::string& command) = 0;
    virtual Result<std::string> GetDisassembly(uintptr_t address) = 0;
    virtual Result<MemoryDump> ReadMemory(uintptr_t address, size_t size) = 0;
    virtual Result<void> SetBreakpoint(uintptr_t address) = 0;
    virtual void RegisterEventHandler(std::function<void(const DebugEvent&)> handler) = 0;
    virtual bool IsConnected() const = 0;
};

// S-Expression Parser Interface
class IExprParser {
public:
    virtual ~IExprParser() = default;
    
    virtual Result<SExpression> Parse(const std::string& expr) = 0;
    virtual Result<std::string> Serialize(const SExpression& expr) = 0;
    virtual Result<SExpression> Evaluate(const SExpression& expr) = 0;
    virtual Result<SExpression> EvaluateInContext(const SExpression& expr) = 0;
    virtual Result<SExpression> EvaluateInContext(const SExpression& expr, 
                                                 const std::unordered_map<std::string, SExpression>& context) = 0;
    virtual Result<std::string> FormatDebugOutput(const SExpression& expr) = 0;
};

// Configuration Manager Interface
class IConfigManager {
public:
    virtual ~IConfigManager() = default;
    
    virtual Result<void> LoadConfig(const std::string& path) = 0;
    virtual Result<void> SaveConfig(const std::string& path) = 0;
    virtual Result<void> SetDefaults() = 0;
    virtual Result<std::string> GetValue(const std::string& key) = 0;
    virtual Result<void> SetValue(const std::string& key, const std::string& value) = 0;
    virtual const Config& GetConfig() const = 0;
};

// Logger Interface
class ILogger {
public:
    enum class Level { DEBUG, INFO, WARN, ERROR, FATAL };
    typedef Level LogLevel; // MSVC compatibility alias
    
    // MSVC-friendly constants to avoid parsing issues
    static const Level LOG_DEBUG = Level::DEBUG;
    static const Level LOG_INFO = Level::INFO;
    static const Level LOG_WARN = Level::WARN;
    static const Level LOG_ERROR = Level::ERROR;
    static const Level LOG_FATAL = Level::FATAL;
    
    virtual ~ILogger() = default;
    
    virtual void Log(Level level, const std::string& message) = 0;
    virtual void LogFormatted(Level level, const char* format, ...) = 0;
    virtual void LogException(const std::exception& ex, const std::string& context = "") = 0;
    virtual void SetLevel(Level level) = 0;
    virtual void SetOutput(const std::string& path) = 0;
};

// Operators for Level enum to fix MSVC parsing issues
inline bool operator==(ILogger::Level lhs, ILogger::Level rhs) {
    return static_cast<int>(lhs) == static_cast<int>(rhs);
}

inline bool operator>=(ILogger::Level lhs, ILogger::Level rhs) {
    return static_cast<int>(lhs) >= static_cast<int>(rhs);
}

// Memory Dump Analyzer Interface  
class IDumpAnalyzer {
public:
    virtual ~IDumpAnalyzer() = default;
    
    virtual Result<std::vector<std::string>> AnalyzePatterns(const MemoryDump& dump) = 0;
    virtual Result<std::vector<std::string>> FindStrings(const MemoryDump& dump) = 0;
    virtual Result<std::unordered_map<std::string, std::string>> ExtractMetadata(const MemoryDump& dump) = 0;
};

// Security Manager Interface
class ISecurityManager {
public:
    virtual ~ISecurityManager() = default;
    
    virtual Result<void> StoreCredential(const std::string& key, const std::string& value) = 0;
    virtual Result<std::string> RetrieveCredential(const std::string& key) = 0;
    virtual Result<void> EncryptData(const std::vector<uint8_t>& data, std::vector<uint8_t>& encrypted) = 0;
    virtual Result<void> DecryptData(const std::vector<uint8_t>& encrypted, std::vector<uint8_t>& data) = 0;
    virtual bool ValidateAPIKey(const std::string& key) const = 0;
};

// Core Engine Interface - Orchestrates all modules
class ICoreEngine {
public:
    virtual ~ICoreEngine() = default;
    
    virtual Result<void> Initialize() = 0;
    virtual Result<void> Shutdown() = 0;
    virtual std::shared_ptr<ILLMEngine> GetLLMEngine() = 0;
    virtual std::shared_ptr<IX64DbgBridge> GetDebugBridge() = 0;
    virtual std::shared_ptr<IExprParser> GetExprParser() = 0;
    virtual std::shared_ptr<IConfigManager> GetConfigManager() = 0;
    virtual std::shared_ptr<ILogger> GetLogger() = 0;
    virtual std::shared_ptr<IDumpAnalyzer> GetDumpAnalyzer() = 0;
    virtual std::shared_ptr<ISecurityManager> GetSecurityManager() = 0;
};

} // namespace mcp