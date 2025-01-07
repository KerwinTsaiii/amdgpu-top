#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <libdrm/amdgpu.h>
#include <libdrm/amdgpu_drm.h>
#include <xf86drm.h>
#include "process_info.hpp"

class GPUDevice {
public:
    // GPU Metrics (sensor data, memory usage, etc.)
    struct Metrics {
        float gpu_usage = 0;
        float memory_used = 0;
        float memory_total = 0;
        float memory_cpu_accessible_total = 0;
        float memory_cpu_accessible_used = 0;
        uint32_t temperature = 0;
        uint32_t power_usage = 0;
        uint32_t fan_speed = 0;
        uint32_t gpu_clock = 0;
        uint32_t memory_clock = 0;
    };

    GPUDevice(int fd, amdgpu_device_handle device, drmVersionPtr version, const std::string& pci_path);
    ~GPUDevice();

    // Device identification
    const char* getGPUName() const;
    const char* getMarketName() const;
    const std::string& getPCIPath() const { return pci_path; }

    // Metrics and process info
    Metrics getMetrics() const;
    std::vector<ProcessInfo> getProcesses() const;

private:
    int fd;
    amdgpu_device_handle device;
    drmVersionPtr version;
    std::string pci_path;
    mutable std::string market_name_cache;

    // Helper functions
    void updateMetrics(Metrics& metrics) const;
    void updateProcessInfo(std::vector<ProcessInfo>& processes) const;
};

class GPUStats {
public:
    GPUStats();
    ~GPUStats();

    bool initialize();
    size_t getGPUCount() const { return gpus.size(); }
    GPUDevice* getGPU(size_t index);
    const GPUDevice* getGPU(size_t index) const;

private:
    std::vector<std::unique_ptr<GPUDevice>> gpus;
}; 