#pragma once

/**
 * @file exception_safety.hpp
 * @brief Exception safety utilities and RAII helpers for MCP Debugger
 * 
 * This file provides:
 * - RAII wrappers for system resources
 * - Exception-safe operation patterns
 * - Resource management utilities
 * - Strong exception safety guarantees
 */

#include <memory>
#include <functional>
#include <mutex>
#include <exception>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mcp {
namespace exception_safety {

// ============================================================================
// RAII RESOURCE WRAPPERS
// ============================================================================

/**
 * @brief RAII wrapper for Windows handles
 */
#ifdef _WIN32
class WindowsHandle {
private:
    HANDLE handle_;
    
public:
    explicit WindowsHandle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept 
        : handle_(handle) {}
    
    ~WindowsHandle() noexcept {
        Close();
    }
    
    // Non-copyable
    WindowsHandle(const WindowsHandle&) = delete;
    WindowsHandle& operator=(const WindowsHandle&) = delete;
    
    // Movable
    WindowsHandle(WindowsHandle&& other) noexcept 
        : handle_(other.handle_) {
        other.handle_ = INVALID_HANDLE_VALUE;
    }
    
    WindowsHandle& operator=(WindowsHandle&& other) noexcept {
        if (this != &other) {
            Close();
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    
    HANDLE Get() const noexcept { return handle_; }
    HANDLE Release() noexcept {
        HANDLE h = handle_;
        handle_ = INVALID_HANDLE_VALUE;
        return h;
    }
    
    void Reset(HANDLE new_handle = INVALID_HANDLE_VALUE) noexcept {
        Close();
        handle_ = new_handle;
    }
    
    bool IsValid() const noexcept {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
    }
    
    operator bool() const noexcept { return IsValid(); }
    
private:
    void Close() noexcept {
        if (IsValid()) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }
};
#endif

/**
 * @brief Generic RAII wrapper with custom deleter
 */
template<typename T, typename Deleter>
class ResourceWrapper {
private:
    T resource_;
    Deleter deleter_;
    bool owns_;
    
public:
    explicit ResourceWrapper(T resource, Deleter deleter) noexcept
        : resource_(resource), deleter_(std::move(deleter)), owns_(true) {}
    
    ~ResourceWrapper() noexcept {
        if (owns_) {
            try {
                deleter_(resource_);
            } catch (...) {
                // Destructors must not throw
            }
        }
    }
    
    // Non-copyable
    ResourceWrapper(const ResourceWrapper&) = delete;
    ResourceWrapper& operator=(const ResourceWrapper&) = delete;
    
    // Movable
    ResourceWrapper(ResourceWrapper&& other) noexcept
        : resource_(other.resource_), deleter_(std::move(other.deleter_)), owns_(other.owns_) {
        other.owns_ = false;
    }
    
    ResourceWrapper& operator=(ResourceWrapper&& other) noexcept {
        if (this != &other) {
            if (owns_) {
                try {
                    deleter_(resource_);
                } catch (...) {
                    // Ignore exceptions in move assignment
                }
            }
            resource_ = other.resource_;
            deleter_ = std::move(other.deleter_);
            owns_ = other.owns_;
            other.owns_ = false;
        }
        return *this;
    }
    
    T& Get() noexcept { return resource_; }
    const T& Get() const noexcept { return resource_; }
    
    T Release() noexcept {
        owns_ = false;
        return resource_;
    }
    
    void Reset(T new_resource) noexcept {
        if (owns_) {
            try {
                deleter_(resource_);
            } catch (...) {
                // Ignore exceptions in reset
            }
        }
        resource_ = new_resource;
        owns_ = true;
    }
};

/**
 * @brief Helper function to create resource wrappers
 */
template<typename T, typename Deleter>
auto MakeResourceWrapper(T resource, Deleter deleter) noexcept {
    return ResourceWrapper<T, Deleter>(resource, std::move(deleter));
}

// ============================================================================
// SCOPE GUARDS
// ============================================================================

/**
 * @brief RAII scope guard for cleanup actions
 */
class ScopeGuard {
private:
    std::function<void()> cleanup_;
    bool active_;
    
public:
    explicit ScopeGuard(std::function<void()> cleanup) noexcept
        : cleanup_(std::move(cleanup)), active_(true) {}
    
    ~ScopeGuard() noexcept {
        if (active_) {
            try {
                cleanup_();
            } catch (...) {
                // Destructors must not throw
            }
        }
    }
    
    // Non-copyable
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    
    // Movable
    ScopeGuard(ScopeGuard&& other) noexcept
        : cleanup_(std::move(other.cleanup_)), active_(other.active_) {
        other.active_ = false;
    }
    
    void Dismiss() noexcept { active_ = false; }
    
    void Execute() {
        if (active_) {
            cleanup_();
            active_ = false;
        }
    }
};

/**
 * @brief Helper macro for scope guards
 */
#define SCOPE_GUARD(cleanup) \
    auto CONCAT(_scope_guard_, __LINE__) = exception_safety::ScopeGuard([&]() { cleanup; })

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)

// ============================================================================
// EXCEPTION-SAFE OPERATIONS
// ============================================================================

/**
 * @brief Exception-safe swap for containers
 */
template<typename Container>
void SafeSwap(Container& a, Container& b) noexcept {
    static_assert(std::is_nothrow_move_constructible_v<Container>, 
                  "Container must be nothrow move constructible");
    static_assert(std::is_nothrow_move_assignable_v<Container>,
                  "Container must be nothrow move assignable");
    
    using std::swap;
    swap(a, b);
}

/**
 * @brief Exception-safe copy-and-swap for assignment operators
 */
template<typename T>
class CopyAndSwap {
public:
    T& operator=(const T& other) {
        T temp(other); // Copy
        SafeSwap(static_cast<T&>(*this), temp); // Swap
        return static_cast<T&>(*this);
    }
};

/**
 * @brief Exception-safe vector operations
 */
template<typename T>
class SafeVector {
private:
    std::vector<T> data_;
    
public:
    // Strong exception safety for push_back
    void SafePushBack(const T& value) {
        data_.reserve(data_.size() + 1); // May throw, but doesn't modify state
        data_.push_back(value); // Now guaranteed not to throw (for vectors)
    }
    
    void SafePushBack(T&& value) {
        data_.reserve(data_.size() + 1); // May throw, but doesn't modify state
        data_.push_back(std::move(value)); // Now guaranteed not to throw
    }
    
    // Strong exception safety for insertion
    template<typename... Args>
    void SafeEmplaceBack(Args&&... args) {
        data_.reserve(data_.size() + 1); // May throw, but doesn't modify state
        data_.emplace_back(std::forward<Args>(args)...); // Now guaranteed not to throw
    }
    
    // Delegating interface
    auto begin() noexcept { return data_.begin(); }
    auto end() noexcept { return data_.end(); }
    auto begin() const noexcept { return data_.begin(); }
    auto end() const noexcept { return data_.end(); }
    
    size_t size() const noexcept { return data_.size(); }
    bool empty() const noexcept { return data_.empty(); }
    
    T& operator[](size_t index) noexcept { return data_[index]; }
    const T& operator[](size_t index) const noexcept { return data_[index]; }
    
    T& at(size_t index) { return data_.at(index); }
    const T& at(size_t index) const { return data_.at(index); }
    
    void clear() noexcept { data_.clear(); }
    void reserve(size_t capacity) { data_.reserve(capacity); }
};

// ============================================================================
// EXCEPTION INFORMATION
// ============================================================================

/**
 * @brief Capture current exception information
 */
struct ExceptionInfo {
    std::string type_name;
    std::string message;
    std::string stack_trace; // Platform-specific implementation needed
    
    static ExceptionInfo Current() noexcept {
        ExceptionInfo info;
        
        try {
            try {
                throw; // Re-throw current exception
            } catch (const std::exception& e) {
                info.type_name = typeid(e).name();
                info.message = e.what();
            } catch (...) {
                info.type_name = "unknown";
                info.message = "Unknown exception type";
            }
        } catch (...) {
            // If we can't even handle the exception, provide minimal info
            info.type_name = "exception_handling_error";
            info.message = "Failed to capture exception information";
        }
        
        return info;
    }
};

/**
 * @brief Exception-safe logging for catch blocks
 */
template<typename Logger>
void SafeLogException(Logger& logger, const std::string& context) noexcept {
    try {
        auto info = ExceptionInfo::Current();
        logger.LogFormatted(
            Logger::Level::ERROR,
            "Exception in %s: %s (%s)",
            context.c_str(),
            info.message.c_str(),
            info.type_name.c_str()
        );
    } catch (...) {
        // If logging fails, there's not much we can do
        // Could write to stderr as last resort
        try {
            fprintf(stderr, "CRITICAL: Exception logging failed in context: %s\n", context.c_str());
        } catch (...) {
            // Even stderr failed - system is likely in bad state
        }
    }
}

// ============================================================================
// EXCEPTION SAFETY TESTING
// ============================================================================

/**
 * @brief Exception injection for testing exception safety
 */
class ExceptionInjector {
private:
    static thread_local int injection_countdown_;
    static thread_local bool enabled_;
    
public:
    static void Enable(int countdown = 1) noexcept {
        enabled_ = true;
        injection_countdown_ = countdown;
    }
    
    static void Disable() noexcept {
        enabled_ = false;
    }
    
    static void CheckAndThrow() {
        if (enabled_ && --injection_countdown_ <= 0) {
            enabled_ = false; // Only throw once per enable
            throw std::runtime_error("Injected exception for testing");
        }
    }
};

/**
 * @brief RAII wrapper for exception injection testing
 */
class ExceptionTestScope {
public:
    explicit ExceptionTestScope(int countdown) {
        ExceptionInjector::Enable(countdown);
    }
    
    ~ExceptionTestScope() {
        ExceptionInjector::Disable();
    }
};

#define EXCEPTION_TEST_POINT() exception_safety::ExceptionInjector::CheckAndThrow()

} // namespace exception_safety
} // namespace mcp