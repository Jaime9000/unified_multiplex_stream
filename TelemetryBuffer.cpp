#include "TelemetryBuffer.hpp"
#include <utility>

// ============================================================================
// BASE CLASS METHODS IMPLEMENTATION
// ============================================================================
BaseProcessor::BaseProcessor(std::string serial_number) : serial_n(std::move(serial_number)) {}
BaseProcessor::~BaseProcessor() {}

bool BaseProcessor::GetConnectionStatus() const {
    std::lock_guard<std::mutex> lock(m_0);
    return !serial_n.empty(); 
}

// ============================================================================
// CHILD CLASS 1: FPGA CORE PROCESSOR IMPLEMENTATION
// ============================================================================
CoreProcessor::CoreProcessor(std::string serial, size_t capacity) 
    : BaseProcessor(std::move(serial)), m_capacity(capacity) {
    memory_pool.resize(m_capacity);
    circular_buffer_index.store(0, std::memory_order_relaxed);
}

CoreProcessor::~CoreProcessor() {
    std::cout << "Core FPGA tracker pipeline disconnected cleanly.\n";
}

void CoreProcessor::DataContract(TelemetryFrame&& frame) {
    std::lock_guard<std::mutex> lock(m_0);

    size_t targetIndex = circular_buffer_index.fetch_add(1, std::memory_order_relaxed) % m_capacity;
    
    // Move the structural payload into our managed heap vector allocation
    memory_pool[targetIndex] = std::make_unique<TelemetryFrame>(std::move(frame));
    
    // 1. THE STAMPING OPERATION: Assign the modern scoped enum directly into the memory block!
    memory_pool[targetIndex]->source_device = DeviceType::FPGA_TRACKER;
    
    // 2. THE LOGGING OPERATION: Fixed the broken semicolon split to maintain stream continuity
    //--> Specific use of static_cast<int> here because compile strictly isolates type saftey for enum class.
    std::cout << "[FPGA CHIP READY] Frame ID " << memory_pool[targetIndex]->frame_id 
              << " stamped with Device-type enum (" << static_cast<int>(memory_pool[targetIndex]->source_device)
              << ") and logged to heap memory pool.\n";
}



// ============================================================================
//  CHILD CLASS 2: OSCILLOSCOPE PROCESSOR IMPLEMENTATION
// ============================================================================
// 1. full scope resolution constructor for OscilloscopeProcessor.
//    - Chain the parameter values to the BaseProcessor parent constructor via std::move().
//    - Pre-allocate your memory pool capacity boundaries inside the bracket scope.
//    - Stably initialize your unique atomic offsets.
OscilloscopeProcessor::OscilloscopeProcessor(std::string serial, std::size_t capacity)
    : BaseProcessor(std::move(serial)), m_capacity(capacity) {
    memory_pool.resize(m_capacity);
    circular_buffer_index.store(0, std::memory_order_relaxed);
}
// 2.  child Destructor printing out: "Oscilloscope instrument cluster detached.\n"
OscilloscopeProcessor::~OscilloscopeProcessor() {
    std::cout << "Osiliscope tracker pipeline disconnected cleanly.\n";
}
// 3.  'DataContract' method implementation override block.
//    - Acquire an RAII lock_guard on 'm_0' to guard execution state transitions.
//    - Run your lock-free fetch-add modulo operations using relaxed CPU atomic instructions.
//    - Dynamically instantiate the object via std::make_unique, moving the rvalue payload into place.
//    - Crucial STEP: Explicitly stamp the incoming element's internal 'source_device' variable
//      to match 'DeviceType::OSCILLOSCOPE' before printing execution confirmation logs.
void OscilloscopeProcessor::DataContract(TelemetryFrame&& frame) {
    std::lock_guard<std::mutex> lock(m_0);

    size_t targetIndex = circular_buffer_index.fetch_add(1, std::memory_order_relaxed) % m_capacity;
    memory_pool[targetIndex] = std::make_unique<TelemetryFrame>(std::move(frame));
    
    // CHOP WORK: Stamp this one with the matching oscilloscope enum tag!
    memory_pool[targetIndex]->source_device = DeviceType::OSCILLOSCOPE;
    
    std::cout << "[OSCILLOSCOPE READY] Frame ID " << memory_pool[targetIndex]->frame_id
              << " stamped with Device-type enum (" << static_cast<int>(memory_pool[targetIndex]->source_device)
              << ") and logged to heap memory pool.\n";
} 

// 1. Re-declare your original visibility macro right here if not in a shared header
#define EXPORT_API __attribute__((visibility("default")))

extern "C" {

    // THE FACTORY CONSTRUCTOR: Creates the class object on the heap and returns its memory address
    EXPORT_API void* CreateCoreProcessor(const char* serial, size_t capacity) {
        // Enforce safety: Return a raw pointer to our modern allocated heap space
        // Unity captures this as an unmanaged 'IntPtr' handle
        return static_cast<void*>(new CoreProcessor(std::string(serial), capacity));
    }

    // THE PIPELINE ENTRY POINT: Takes the raw handle, casts it back to the base contract, and processes data
    EXPORT_API void IngestTelemetryFrame(void* processorHandle, uint32_t id, uint64_t time, float measurement, bool valid) {
        if (!processorHandle) return;

        // Reconstruct a temporary frame container local to this stack frame execution boundary
        TelemetryFrame frame{ id, time, measurement, valid, DeviceType::UNKNOWN };

        // Polymorphic Cast: Convert the raw void pointer back to our abstract Base class pointer
        BaseProcessor* processor = static_cast<BaseProcessor*>(processorHandle);

        // Execute the data contract. C++ resolves the internal vtable and invokes the correct child rules!
        processor->DataContract(std::move(frame));
    }

    // THE FACTORY DESTRUCTOR: Explicitly deletes the allocated memory block to trigger RAII teardown
    EXPORT_API void DestroyProcessor(void* processorHandle) {
        if (!processorHandle) return;

        BaseProcessor* processor = static_cast<BaseProcessor*>(processorHandle);
        
        // Because the BaseProcessor destructor is marked 'virtual', deleting this pointer 
        // cleanly triggers the derived child destructor first, preventing heap memory leaks!
        delete processor; 
    }
}


