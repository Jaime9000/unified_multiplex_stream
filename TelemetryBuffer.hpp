// Copyright (c) 2026 football.io LLLP. All rights reserved.
#pragma once
#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#define MAX_VOLTAGE 5.0f 
#define SYS_PORT 0x0A20 

// --- NEW SYSTEM COMPONENT: SCOPED ENUM FOR HARDWARE IDENTIFICATION ---
// We use a modern C++ 'enum class' backed by an explicit uint8_t to ensure 
// minimal memory storage footprint (1 byte) within our tracking packets.
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
    
    // --- INTEGRATE NEW ENUM VARIABLE HERE ---
    // Declare a member variable named 'source_device' of type 'DeviceType' 
    // to make this structure completely self-describing down the pipeline stream.
    DeviceType source_device;
};

class BaseProcessor {
protected:
    std::string serial_n;
    mutable std::mutex m_0;

public:
    explicit BaseProcessor(std::string serial_number);
    virtual ~BaseProcessor();

    virtual void DataContract(TelemetryFrame&& rvalue) = 0;
    bool GetConnectionStatus() const;
};

// ============================================================================
// CHILD CLASS 1: FPGA CORE PROCESSOR (UPDATED)
// ============================================================================
class CoreProcessor : public BaseProcessor {
private:
    std::vector<std::unique_ptr<TelemetryFrame>> memory_pool; 
    std::atomic<size_t> circular_buffer_index; 
    size_t m_capacity;

public:
    CoreProcessor(std::string serial, size_t capacity);
    ~CoreProcessor() override; 

    // Enforce devirtualization safety using 'override final'
    void DataContract(TelemetryFrame&& frame) override final;
};


// ============================================================================
//  CHILD CLASS 2: OSCILLOSCOPE PROCESSOR
// ============================================================================
// 1. Declare a new class named 'OscilloscopeProcessor' that inherits publicly from 'BaseProcessor'.
// 2. Private section requirements:
//    - Create a standard vector tracking an internal memory pool of raw TelemetryFrame values or pointers.
//    - Create an atomic size_t index for tracking circular buffer writes.
//    - Create a size_t tracking local capacity size thresholds.
// 3. Public section requirements:
//    - Create a constructor that matches the exact parameter signature of CoreProcessor (serial and capacity).
//    - Explicitly write out the child destructor signature using the override keyword.
//    - Override the 'DataContract' function precisely, tagging it as 'override final'.

class OscilloscopeProcessor : public BaseProcessor {
private:
    std::vector<std::unique_ptr<TelemetryFrame>> memory_pool;
    std::atomic<std::size_t> circular_buffer_index;
    std::size_t m_capacity;

public:
    OscilloscopeProcessor(std::string serial, std::size_t capacity);
    ~OscilloscopeProcessor() override;

    void DataContract(TelemetryFrame&& frame) override final;
};

