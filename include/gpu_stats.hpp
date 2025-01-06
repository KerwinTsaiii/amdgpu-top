#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <libdrm/amdgpu.h>
#include <libdrm/amdgpu_drm.h>
#include <xf86drm.h>
#include "process_info.hpp"

class GPUStats {
public:
    struct Stats {
        float gpu_usage = 0;
        float memory_used = 0;
        float memory_total = 0;
        uint32_t temperature = 0;
        uint32_t power_usage = 0;
        uint32_t fan_speed = 0;
        uint32_t gpu_clock = 0;
        uint32_t memory_clock = 0;
        std::vector<ProcessInfo> processes;
    };

    GPUStats();
    ~GPUStats();

    bool initialize();
    Stats getStats();
    const char* getGPUName() const;
    const char* getMarketName() const;

private:
    int fd;
    amdgpu_device_handle device;
    drmVersionPtr version;
}; 