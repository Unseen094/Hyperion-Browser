#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <functional>
#include <memory>
#include <cstdlib>

namespace hyperion::debug::memory {

// Memory allocation types for profiling
enum class AllocationType : uint8_t {
    JS_OBJECT,
    STRING,
    ARRAY_BUFFER,
    TYPED_ARRAY,
    MAP,
    SET,
    FUNCTION,
    CLOSURE,
    WASM_MODULE,
    BITSET,
};

// Memory profiler entry structure
struct MemoryEntry {
    AllocationType type;
    uint64_t size;
    void* pointer;
    std::string stack_trace;
    std::chrono::system_clock::time_point allocated;
    std::chrono::system_clock::time_point freed;
    bool is_freed;
    
    MemoryEntry() : is_freed(false) {}
    MemoryEntry(AllocationType t, uint64_t s, void* p, const std::string& trace = {})
        : type(t), size(s), pointer(p), stack_trace(trace),
          allocated(std::chrono::system_clock::now()), is_freed(false) {}
};

// Memory address mapping for leaks
struct MemoryRange {
    void* start;
    void* end;
    uint64_t size;
    AllocationType type;
    std::string description;
};

// Stack trace simulation class
class StackTraceGenerator {
public:
    static std::string capture_stack_trace() {
        return "STACK: main->" + simulate_stack_frames();
    }
    
    static std::string sample_heap_allocations() {
        std::ostringstream oss;
        oss << "Heap: Array(100)";
        return oss.str();
    }

private:
    static std::string simulate_stack_frames() {
        return "render()#func()#event_handler()";
    }
};

// Memory profiler implementation
class MemoryProfiler {
public:
    MemoryProfiler() {
        install_allocation_hooks();
    }
    
    ~MemoryProfiler() {
        uninstall_allocation_hooks();
    }
    
    // Profile allocation
    void profile_allocation(AllocationType type, size_t size) {
        auto entry = MemoryEntry(type, size, nullptr, StackTraceGenerator::capture_stack_trace());
        std::lock_guard<std::mutex> lock(mutex_);
        memory_entries_.push_back(entry);
        
        // Track by type
        allocations_by_type_[type] += size;
        total_allocated_ += size;
        peak_memory_ = std::max(peak_memory_, total_allocated_);
    }
    
    // Profile deallocation
    void profile_deallocation(void* pointer, AllocationType type, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Update existing entry
        for (auto& entry : memory_entries_) {
            if (entry.pointer == pointer && !entry.is_freed) {
                entry.size = size;
                entry.freed = std::chrono::system_clock::now();
                entry.is_freed = true;
                
                allocations_by_type_[type] -= size;
                total_allocated_ -= size;
                return;
            }
        }
        
        // Unknown deallocation
        memory_entries_.push_back(MemoryEntry(type, size, pointer, "Unknown"));
        memory_entries_.back().freed = std::chrono::system_clock::now();
        memory_entries_.back().is_freed = true;
    }
    
    // Get memory summary report
    std::string generate_summary() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "Memory Profiler Report\n";
        oss << "======================\n";
        oss << "Total Allocated: " << total_allocated_ << " bytes\n";
        oss << "Peak Allocated: " << peak_memory_ << " bytes\n";
        oss << "Active Objects: " << active_objects_count() << "\n";
        oss << "\nBreakdown by Type:\n";
        
        for (const auto& [type, size] : allocations_by_type_) {
            oss << "  " << allocation_type_name(type) << ": " << size << " bytes\n";
        }
        
        return oss.str();
    }
    
    // Get active objects with above threshold size
    std::vector<MemoryEntry> get_large_objects(uint64_t threshold_bytes = 1024) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<MemoryEntry> result;
        
        for (const auto& entry : memory_entries_) {
            if (!entry.is_freed && entry.size >= threshold_bytes) {
                result.push_back(entry);
            }
        }
        
        return result;
    }
    
    // Get memory leaks (still allocated at exit)
    std::vector<MemoryEntry> find_leaks(size_t limit = 100) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<MemoryEntry> leaks;
        
        for (const auto& entry : memory_entries_) {
            if (!entry.is_freed && leaks.size() < limit) {
                leaks.push_back(entry);
            }
        }
        
        return leaks;
    }
    
    // Track specific allocations for debugging
    class AllocationTracker {
    public:
        void track_allocation(AllocationType type, const std::string& description) {
            std::lock_guard<std::mutex> lock(mutex_);
            tracked_[description] = type;
        }
        
        std::string get_description(AllocationType type) {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [desc, t] : tracked_) {
                if (t == type) return desc;
            }
            return allocation_type_name(type);
        }

    private:
        std::unordered_map<std::string, AllocationType> tracked_;
        mutable std::mutex mutex_;
    };
    
    AllocationTracker& allocation_tracker() {
        static AllocationTracker tracker;
        return tracker;
    }
    
    // Mark - similar to console.time
    void mark(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        marks_[name] = std::chrono::steady_clock::now();
    }
    
    // Measure - similar to console.timeEnd
    void measure(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = marks_.find(name);
        if (it != marks_.end()) {
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - it->second).count();
            memory_entries_.push_back({
                AllocationType::FUNCTION, 
                static_cast<uint64_t>(duration),
                nullptr,
                "Memory measurement: " + name
            });
            marks_.erase(it);
        }
    }
    
    // Get allocation history for pointer
    std::vector<MemoryEntry> get_pointer_history(void* pointer) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<MemoryEntry> history;
        
        for (const auto& entry : memory_entries_) {
            // @TODO: Real pointer comparison
            if (entry.pointer == pointer) {
                history.push_back(entry);
            }
        }
        
        return history;
    }

private:
    uint64_t active_objects_count() const {
        uint64_t count = 0;
        for (const auto& entry : memory_entries_) {
            if (!entry.is_freed) count++;
        }
        return count;
    }
    
    const char* allocation_type_name(AllocationType type) const {
        switch (type) {
            case AllocationType::JS_OBJECT: return "JavaScript Object";
            case AllocationType::STRING: return "String";
            case AllocationType::ARRAY_BUFFER: return "ArrayBuffer";
            case AllocationType::TYPED_ARRAY: return "TypedArray";
            case AllocationType::MAP: return "Map";
            case AllocationType::SET: return "Set";
            case AllocationType::FUNCTION: return "Function";
            case AllocationType::CLOSURE: return "Closure";
            case AllocationType::WASM_MODULE: return "WASM Module";
            case AllocationType::BITSET: return "Bitset";
            default: return "Unknown";
        }
    }
    
    void install_allocation_hooks() {
        // In real implementation: replace system malloc/free with profiling wrappers
        // For now: use placeholder allocation_size function
    }
    
    void uninstall_allocation_hooks() {
        // Restore systemalloctors
    }
    
    std::vector<MemoryEntry> memory_entries_;
    std::unordered_map<AllocationType, uint64_t> allocations_by_type_;
    uint64_t total_allocated_ = 0;
    uint64_t peak_memory_ = 0;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> marks_;
};

// C compatible wrapper for global profiler instance
class CMemoryProfiler {
public:
    static MemoryProfiler& instance() {
        static MemoryProfiler inst;
        return inst;
    }
    
    static void log_allocation(void* type, size_t size) {
        instance().profile_allocation(static_cast<AllocationType>(reinterpret_cast<uint64_t>(type)), size);
    }
    
    static void log_deallocation(void* ptr, void* type, size_t size) {
        instance().profile_deallocation(ptr, static_cast<AllocationType>(reinterpret_cast<uint64_t>(type)), size);
    }
};

// Custom allocator wrapper for testing
class InstrumentedAllocator {
public:
    static void* allocate(size_t size) {
        void* ptr = malloc(size);
        CMemoryProfiler::log_allocation(nullptr, size);
        return ptr;
    }
    
    static void deallocate(void* ptr) {
        CMemoryProfiler::log_deallocation(ptr, nullptr, 0);
        free(ptr);
    }
    
    static void* reallocate(void* ptr, size_t size) {
        CMemoryProfiler::log_deallocation(ptr, nullptr, 0);
        void* new_ptr = realloc(ptr, size);
        CMemoryProfiler::log_allocation(nullptr, size);
        return new_ptr;
    }
};

} // namespace hyperion::debug::memory