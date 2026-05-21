// unified_solution.cpp — TelemetryBuffer + main in one translation unit

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#define MAX_VOLTAGE 5.0f
#define SYS_PORT 0x0A20

enum class DeviceType : std::uint8_t {
    UNKNOWN = 0,
    FPGA_TRACKER = 1,
    OSCILLOSCOPE = 2
};

struct alignas(64) TelemetryFrame {
    std::uint32_t frame_id;
    std::uint64_t sys_time_res;
    float sensor_measurements;
    bool is_validated;
    DeviceType source_device;
};

class BaseProcessor {
protected:
    std::string serial_n;
    mutable std::mutex m_0;

public:
    explicit BaseProcessor(std::string serial_number)
        : serial_n(std::move(serial_number)) {}

    virtual ~BaseProcessor() = default;

    virtual void DataContract(TelemetryFrame&& rvalue) = 0;

    bool GetConnectionStatus() const {
        std::lock_guard<std::mutex> lock(m_0);
        return !serial_n.empty();
    }
};

class CoreProcessor : public BaseProcessor {
private:
    std::vector<std::unique_ptr<TelemetryFrame>> memory_pool;
    std::atomic<std::size_t> circular_buffer_index{0};
    std::size_t m_capacity;

public:
    CoreProcessor(std::string serial, std::size_t capacity)
        : BaseProcessor(std::move(serial)), m_capacity(capacity) {
        memory_pool.resize(m_capacity);
        circular_buffer_index.store(0, std::memory_order_relaxed);
    }

    ~CoreProcessor() override {
        std::cout << "Core FPGA tracker pipeline disconnected cleanly.\n";
    }

    void DataContract(TelemetryFrame&& frame) override final {
        std::lock_guard<std::mutex> lock(m_0);

        std::size_t targetIndex =
            circular_buffer_index.fetch_add(1, std::memory_order_relaxed) % m_capacity;

        memory_pool[targetIndex] = std::make_unique<TelemetryFrame>(std::move(frame));
        memory_pool[targetIndex]->source_device = DeviceType::FPGA_TRACKER;

        std::cout << "[FPGA CHIP READY] Frame ID " << memory_pool[targetIndex]->frame_id
                  << " stamped with Device-type enum ("
                  << static_cast<int>(memory_pool[targetIndex]->source_device)
                  << ") and logged to heap memory pool.\n";
    }
};

class OscilloscopeProcessor : public BaseProcessor {
private:
    std::vector<std::unique_ptr<TelemetryFrame>> memory_pool;
    std::atomic<std::size_t> circular_buffer_index{0};
    std::size_t m_capacity;

public:
    OscilloscopeProcessor(std::string serial, std::size_t capacity)
        : BaseProcessor(std::move(serial)), m_capacity(capacity) {
        memory_pool.resize(m_capacity);
        circular_buffer_index.store(0, std::memory_order_relaxed);
    }

    ~OscilloscopeProcessor() override {
        std::cout << "Oscilloscope instrument cluster detached.\n";
    }

    void DataContract(TelemetryFrame&& frame) override final {
        std::lock_guard<std::mutex> lock(m_0);

        std::size_t targetIndex =
            circular_buffer_index.fetch_add(1, std::memory_order_relaxed) % m_capacity;

        memory_pool[targetIndex] = std::make_unique<TelemetryFrame>(std::move(frame));
        memory_pool[targetIndex]->source_device = DeviceType::OSCILLOSCOPE;

        std::cout << "[OSCILLOSCOPE READY] Frame ID " << memory_pool[targetIndex]->frame_id
                  << " stamped with Device-type enum ("
                  << static_cast<int>(memory_pool[targetIndex]->source_device)
                  << ") and logged to heap memory pool.\n";
    }
};

#define EXPORT_API __attribute__((visibility("default")))

extern "C" {

EXPORT_API void* CreateCoreProcessor(const char* serial, std::size_t capacity) {
    return static_cast<void*>(new CoreProcessor(std::string(serial), capacity));
}

EXPORT_API void IngestTelemetryFrame(void* processorHandle, std::uint32_t id,
                                     std::uint64_t time, float measurement, bool valid) {
    if (!processorHandle) return;

    TelemetryFrame frame{id, time, measurement, valid, DeviceType::UNKNOWN};
    auto* processor = static_cast<BaseProcessor*>(processorHandle);
    processor->DataContract(std::move(frame));
}

EXPORT_API void DestroyProcessor(void* processorHandle) {
    if (!processorHandle) return;
    delete static_cast<BaseProcessor*>(processorHandle);
}

}  // extern "C"

int main() {
    std::cout << "=== Activating Polymorphic Lab Network Bus ===\n";

    // 1. THE DYNAMIC BUS ARRAY (The Multiplex Bus for N devices)
    std::vector<std::unique_ptr<BaseProcessor>> labBenchNetwork;
    
    // OPTIMIZATION: Pre-reserve space to eliminate vector reallocation jitter
    // --> 10 is an example this would be determined at runtime by a network bus discovery scan that would determine number of devices
    labBenchNetwork.reserve(10); 

    // 2. YOUR EXACT DECOUPLED ALIGNMENT SYNTAX (Show this on the whiteboard!)
    // The left side is the universal parent socket; the right side is the child.
    std::unique_ptr<BaseProcessor> fpgaPipeline = 
        std::make_unique<CoreProcessor>("FPGA-CORE-01", 5);
        
    std::unique_ptr<BaseProcessor> scopePipeline = 
        std::make_unique<OscilloscopeProcessor>("SCOPE-HW-02", 5);

    // 3. TRANSFER EXCLUSIVE OWNERSHIP TO THE AUTOMATED N-ARRAY BUS
    // We use std::move to push your decoupled pipelines directly into the vector array slots.
    labBenchNetwork.push_back(std::move(fpgaPipeline));  // fpgaPipeline is now null
    labBenchNetwork.push_back(std::move(scopePipeline)); // scopePipeline is now null

    // DYNAMIC CONDITIONS: You can now append more devices to N as needed!
    // labBenchNetwork.push_back(std::make_unique<CoreProcessor>("FPGA-CORE-03", 5));

    // 4. DYNAMIC MULTIPLEXED INGESTION LOOP (Scales for N devices blindly)
    uint32_t activeFrameCounter = 501;
    
    // The loop iterates over N items using your parent interface rules
    for (const auto& device : labBenchNetwork) {
        TelemetryFrame freshPacket{ activeFrameCounter++, 111111, 1.25f, true, DeviceType::UNKNOWN };
        
        // Executes data contracts smoothly via vtable resolution
        device->DataContract(std::move(freshPacket));
    }

    std::cout << "\n=== Closing Down Systems Lab Engine Diagnostics ===\n";
    return 0;
}
