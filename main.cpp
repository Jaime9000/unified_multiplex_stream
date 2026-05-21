// Copyright (c) 2026 football.io LLLP. All rights reserved.
#include "TelemetryBuffer.hpp"
#include <iostream>
#include <vector>
#include <utility>
#include <memory>

int main() {
    std::cout << "=== Activating Polymorphic Lab Network Bus ===\n";

    // 1. THE SHARED ABSTRACT ARRAY CONTAINER (The Multiplex Bus)
    // We instantiate a standard vector configured to hold unique_ptrs to ANY 'BaseProcessor' data tracks.
    // This allows us to cleanly aggregate different hardware children inside a single memory layout block.
    std::vector<std::unique_ptr<BaseProcessor>> labBenchNetwork;

    // 2. INSTANTIATE VEHICLES VIA "STYLE 2" INTERFACE ALIGNMENT
    // The left side of the equals sign acts as a universal parent hardware socket container.
    // The right side dynamically allocates the concrete, specific child processing engines.
    std::unique_ptr<BaseProcessor> fpgaPipeline = std::make_unique<CoreProcessor>("FPGA-CORE-01", 5);
    std::unique_ptr<BaseProcessor> scopePipeline = std::make_unique<OscilloscopeProcessor>("SCOPE-HW-02", 5);

    // 3. TRANSFER EXCLUSIVE POINTER OWNERSHIP TO THE BUS ARRAY
    // Because std::unique_ptr explicitly deletes its copy constructor to enforce zero-leak memory integrity,
    // we utilize std::move() to cleanly shift the underlying heap addresses right into the vector elements.
    // After these lines execute, the 'fpgaPipeline' and 'scopePipeline' variables become safely null.
    labBenchNetwork.push_back(std::move(fpgaPipeline));  
    labBenchNetwork.push_back(std::move(scopePipeline)); 

    // 4. GENERATE STREAM READOUT DATA PACKETS
    // We instantiate two cache-aligned structural samples local to the stack frame.
    // Note: The 'source_device' enum parameter is left as 'DeviceType::UNKNOWN' at setup;
    // our unmanaged child devices will automatically handle self-describing their identity down the pipeline.
    TelemetryFrame sampleA{ 501, 111111, 1.25f, true, DeviceType::UNKNOWN };
    TelemetryFrame sampleB{ 502, 222222, 4.36f, true, DeviceType::UNKNOWN };

    std::cout << "\n--- Activating Hardware Serialization Data Contracts ---\n";

    // 5. STREAM MULTIPLEX OVER THE UNIFIED SENSOR FUSION DATA BUS
    // The orchestrator iterates through the index blocks blindly. It doesn't care which child chip
    // is attached—it simply pushes the data contracts down via standard rvalue references.
    if (labBenchNetwork.size() >= 2) {
        // Element 0 instantly resolves its internal vtable lookup to execute FPGA processing rules
        labBenchNetwork[0]->DataContract(std::move(sampleA));
        
        // Element 1 instantly resolves its internal vtable lookup to execute Oscilloscope processing rules
        labBenchNetwork[1]->DataContract(std::move(sampleB));
    }

    // 6. VALIDATE READ-ONLY GETTER Snapshots
    std::cout << "\n--- Auditing Connected Channel Status Nodes ---\n";
    for (size_t i = 0; i < labBenchNetwork.size(); ++i) {
        // Safe, const-correct multi-threaded lookup executing via scope-based lock_guards
        if (labBenchNetwork[i]->GetConnectionStatus()) {
            std::cout << "Bus Node Index [" << i << "] Structural Linkage Verified and Active.\n";
        }
    }

    std::cout << "\n=== Closing Down Systems Lab Engine Diagnostics ===\n";
    return 0;
    // SYSTEM RAII AUTOMATION VERIFICATION AT EXIT PHASE:
    // As 'main' exits, the stack frame unwinds. 'labBenchNetwork' vector drops out of scope, destroying its elements.
    // The 'virtual' parent destructor intercepts the teardown sequence, ensuring that child memory loops 
    // run completely, freeing the pre-allocated ring-buffer vectors with absolute zero memory leaks.
}
