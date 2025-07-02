#pragma once

/**
 * @file performance_optimization.hpp
 * @brief Performance optimization utilities and techniques for MCP Debugger
 * 
 * This file contains performance-critical optimizations:
 * - Memory pool allocators
 * - Object recycling
 * - String interning
 * - Cache-friendly data structures
 * - SIMD optimizations where applicable
 */

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>

namespace mcp {
namespace performance {

// ============================================================================
// MEMORY POOL ALLOCATOR
// ============================================================================

/**
 * @brief High-performance memory pool for frequent allocations
 * Thread-safe memory pool that reduces malloc/free overhead
 */
template<typename T, size_t PoolSize = 1024>
class MemoryPool {
private:
    struct Block {
        alignas(T) char data[sizeof(T)];
        Block* next;
    };
    
    std::vector<std::unique_ptr<Block[]>> chunks_;
    Block* free_list_;
    std::mutex mutex_;
    std::atomic<size_t> allocated_count_{0};
    std::atomic<size_t> peak_allocated_{0};
    
public:
    MemoryPool() : free_list_(nullptr) {
        AllocateNewChunk();
    }
    
    ~MemoryPool() = default;
    
    T* Allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!free_list_) {
            AllocateNewChunk();
        }
        
        Block* block = free_list_;
        free_list_ = free_list_->next;
        
        size_t current = allocated_count_.fetch_add(1) + 1;
        size_t peak = peak_allocated_.load();
        while (current > peak && !peak_allocated_.compare_exchange_weak(peak, current)) {
            peak = peak_allocated_.load();
        }
        
        return reinterpret_cast<T*>(block);
    }
    
    void Deallocate(T* ptr) {
        if (!ptr) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        Block* block = reinterpret_cast<Block*>(ptr);
        block->next = free_list_;
        free_list_ = block;
        
        allocated_count_.fetch_sub(1);
    }
    
    size_t GetAllocatedCount() const { return allocated_count_.load(); }
    size_t GetPeakAllocated() const { return peak_allocated_.load(); }
    
private:
    void AllocateNewChunk() {
        auto chunk = std::make_unique<Block[]>(PoolSize);
        
        // Link all blocks in the chunk to free list
        for (size_t i = 0; i < PoolSize - 1; ++i) {
            chunk[i].next = &chunk[i + 1];
        }
        chunk[PoolSize - 1].next = free_list_;
        free_list_ = &chunk[0];
        
        chunks_.push_back(std::move(chunk));
    }
};

// ============================================================================
// STRING INTERNING
// ============================================================================

/**
 * @brief String interning for memory optimization
 * Reduces memory usage by storing only one copy of each unique string
 */
class StringInterner {
private:
    std::unordered_map<std::string, std::weak_ptr<std::string>> intern_map_;
    std::mutex mutex_;
    std::atomic<size_t> total_strings_{0};
    std::atomic<size_t> unique_strings_{0};
    
public:
    std::shared_ptr<std::string> Intern(const std::string& str) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        total_strings_.fetch_add(1);
        
        auto it = intern_map_.find(str);
        if (it != intern_map_.end()) {
            if (auto existing = it->second.lock()) {
                return existing;
            } else {
                // Weak pointer expired, remove from map
                intern_map_.erase(it);
            }
        }
        
        // Create new interned string
        auto interned = std::make_shared<std::string>(str);
        intern_map_[str] = interned;
        unique_strings_.fetch_add(1);
        
        return interned;
    }
    
    void Cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto it = intern_map_.begin(); it != intern_map_.end();) {
            if (it->second.expired()) {
                it = intern_map_.erase(it);
                unique_strings_.fetch_sub(1);
            } else {
                ++it;
            }
        }
    }
    
    size_t GetTotalStrings() const { return total_strings_.load(); }
    size_t GetUniqueStrings() const { return unique_strings_.load(); }
    double GetInternRatio() const {
        size_t total = total_strings_.load();
        size_t unique = unique_strings_.load();
        return total > 0 ? static_cast<double>(unique) / total : 0.0;
    }
};

// ============================================================================
// OBJECT RECYCLING
// ============================================================================

/**
 * @brief Object recycling pool for expensive-to-construct objects
 * Reduces construction/destruction overhead for heavy objects
 */
template<typename T>
class ObjectPool {
private:
    std::vector<std::unique_ptr<T>> available_;
    std::mutex mutex_;
    std::atomic<size_t> created_count_{0};
    std::atomic<size_t> recycled_count_{0};
    
public:
    template<typename... Args>
    std::unique_ptr<T> Acquire(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!available_.empty()) {
            auto obj = std::move(available_.back());
            available_.pop_back();
            recycled_count_.fetch_add(1);
            return obj;
        }
        
        created_count_.fetch_add(1);
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
    
    void Release(std::unique_ptr<T> obj) {
        if (!obj) return;
        
        // Reset object to clean state
        obj->Reset(); // Assumes T has Reset() method
        
        std::lock_guard<std::mutex> lock(mutex_);
        available_.push_back(std::move(obj));
    }
    
    size_t GetCreatedCount() const { return created_count_.load(); }
    size_t GetRecycledCount() const { return recycled_count_.load(); }
    double GetRecycleRatio() const {
        size_t total = created_count_.load() + recycled_count_.load();
        size_t recycled = recycled_count_.load();
        return total > 0 ? static_cast<double>(recycled) / total : 0.0;
    }
    
    void Shrink(size_t max_size = 10) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (available_.size() > max_size) {
            available_.resize(max_size);
        }
    }
};

// ============================================================================
// PERFORMANCE MONITORING
// ============================================================================

/**
 * @brief Lightweight performance timer for profiling
 */
class PerformanceTimer {
private:
    std::chrono::high_resolution_clock::time_point start_;
    std::string name_;
    
public:
    explicit PerformanceTimer(const std::string& name) 
        : start_(std::chrono::high_resolution_clock::now()), name_(name) {}
    
    ~PerformanceTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        
        // Could integrate with logging system here
        // For now, just track the measurement
        static std::atomic<long long> total_time{0};
        total_time.fetch_add(duration.count());
    }
    
    long long ElapsedMicroseconds() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - start_).count();
    }
};

/**
 * @brief RAII performance scope timer
 */
#define PERF_TIMER(name) performance::PerformanceTimer _timer(name)

// ============================================================================
// CACHE-FRIENDLY STRUCTURES
// ============================================================================

/**
 * @brief Cache-aligned allocator for better performance
 */
template<typename T>
class CacheAlignedAllocator {
public:
    using value_type = T;
    static constexpr size_t cache_line_size = 64; // Common cache line size
    
    template<typename U>
    struct rebind {
        using other = CacheAlignedAllocator<U>;
    };
    
    T* allocate(size_t n) {
        size_t size = n * sizeof(T);
        size_t aligned_size = (size + cache_line_size - 1) & ~(cache_line_size - 1);
        
#ifdef _WIN32
        return static_cast<T*>(_aligned_malloc(aligned_size, cache_line_size));
#else
        void* ptr = nullptr;
        if (posix_memalign(&ptr, cache_line_size, aligned_size) != 0) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
#endif
    }
    
    void deallocate(T* ptr, size_t) {
#ifdef _WIN32
        _aligned_free(ptr);
#else
        free(ptr);
#endif
    }
    
    template<typename U>
    bool operator==(const CacheAlignedAllocator<U>&) const { return true; }
    
    template<typename U>
    bool operator!=(const CacheAlignedAllocator<U>&) const { return false; }
};

// ============================================================================
// VECTORIZED OPERATIONS
// ============================================================================

/**
 * @brief SIMD-optimized memory operations where available
 */
namespace simd {

/**
 * @brief Fast memory comparison using SIMD when available
 */
inline bool FastMemoryCompare(const void* a, const void* b, size_t size) {
    const uint8_t* pa = static_cast<const uint8_t*>(a);
    const uint8_t* pb = static_cast<const uint8_t*>(b);
    
    // For small sizes, use regular comparison
    if (size < 16) {
        return std::memcmp(a, b, size) == 0;
    }
    
    // For larger sizes, could use SIMD instructions
    // This is a simplified version - real implementation would use
    // SSE2/AVX2 instructions for better performance
    
    size_t aligned_size = size & ~15; // Align to 16 bytes
    
    for (size_t i = 0; i < aligned_size; i += 16) {
        // Compare 16 bytes at a time
        if (std::memcmp(pa + i, pb + i, 16) != 0) {
            return false;
        }
    }
    
    // Handle remaining bytes
    if (size > aligned_size) {
        return std::memcmp(pa + aligned_size, pb + aligned_size, size - aligned_size) == 0;
    }
    
    return true;
}

/**
 * @brief Fast memory search using SIMD when available
 */
inline const uint8_t* FastMemorySearch(const uint8_t* haystack, size_t haystack_size,
                                     const uint8_t* needle, size_t needle_size) {
    if (needle_size == 0 || needle_size > haystack_size) {
        return nullptr;
    }
    
    // For small needles, use standard search
    if (needle_size == 1) {
        return static_cast<const uint8_t*>(std::memchr(haystack, needle[0], haystack_size));
    }
    
    // For larger patterns, could use Boyer-Moore or SIMD-accelerated search
    // This is a simplified version
    for (size_t i = 0; i <= haystack_size - needle_size; ++i) {
        if (FastMemoryCompare(haystack + i, needle, needle_size)) {
            return haystack + i;
        }
    }
    
    return nullptr;
}

} // namespace simd

// ============================================================================
// GLOBAL PERFORMANCE MANAGERS
// ============================================================================

/// Global string interner instance
extern StringInterner& GetStringInterner();

/// Global memory pools for common types
extern MemoryPool<std::string>& GetStringPool();

} // namespace performance
} // namespace mcp