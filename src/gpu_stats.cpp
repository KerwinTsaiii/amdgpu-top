#include "gpu_stats.hpp"
#include "device_info/DeviceInfo.h"
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include "amdgpu_ids.hpp"
#include <libdrm/amdgpu_drm.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>  // for major()
#include <fcntl.h>
#include "process_info.hpp"
#include <map>

GPUDevice::GPUDevice(int fd, amdgpu_device_handle device, drmVersionPtr version, const std::string& pci_path)
    : fd(fd), device(device), version(version), pci_path(pci_path) {}

GPUDevice::~GPUDevice() {
    if (device) {
        amdgpu_device_deinitialize(device);
    }
    if (version) {
        drmFreeVersion(version);
    }
    if (fd >= 0) {
        close(fd);
    }
}

void GPUDevice::updateMetrics(Metrics& metrics) const {
    uint32_t value;

    // Get GPU usage
    if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GPU_LOAD, 
                               sizeof(value), &value) == 0) {
        metrics.gpu_usage = value;
    }

    // Get GPU temperature
    if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GPU_TEMP, 
                               sizeof(value), &value) == 0) {
        metrics.temperature = value / 1000;
    }

    // Get power usage
    if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GPU_AVG_POWER, 
                               sizeof(value), &value) == 0) {
        metrics.power_usage = value;
    }

    // Get memory info
    struct drm_amdgpu_memory_info memory_info;
    if (amdgpu_query_info(device, AMDGPU_INFO_MEMORY, 
                        sizeof(memory_info), &memory_info) == 0) {
        metrics.memory_total = memory_info.vram.total_heap_size / (1024.0 * 1024.0);
        metrics.memory_cpu_accessible_total = memory_info.cpu_accessible_vram.total_heap_size / (1024.0 * 1024.0);
        metrics.memory_cpu_accessible_used = memory_info.cpu_accessible_vram.heap_usage / (1024.0 * 1024.0);
        metrics.memory_used = memory_info.vram.heap_usage / (1024.0 * 1024.0);
    }

    // Get clock speeds
    if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GFX_SCLK, 
                               sizeof(value), &value) == 0) {
        metrics.gpu_clock = value;
    }
    if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GFX_MCLK, 
                               sizeof(value), &value) == 0) {
        metrics.memory_clock = value;
    }
}

GPUDevice::Metrics GPUDevice::getMetrics() const {
    Metrics metrics;
    updateMetrics(metrics);
    return metrics;
}

std::vector<ProcessInfo> GPUDevice::getProcesses() const {
    Metrics metrics = getMetrics();  // Get current metrics for GPU usage
    return ProcessMonitor::getProcesses(device, metrics.gpu_usage);
}

const char* GPUDevice::getGPUName() const {
    return version ? version->name : "Unknown";
}

/**
 * Get the marketing name of the GPU device
 * 
 * This function attempts to identify the GPU and return a human-readable name in the following order:
 * 1. First checks the cache to avoid repeated lookups
 * 2. Queries the GPU info using AMDGPU driver
 * 3. Tries to match the GPU in this order:
 *    a. Exact match (both device ID and revision ID)
 *    b. Partial match (only device ID)
 *    c. Generic format if no match found
 * 
 * The returned name format will be one of:
 * - Full match:    "Radeon RX 6800 [0x73BF:0x0A]"
 * - Partial match: "Radeon RX 6800 [0x73BF:0x0C]"
 * - No match:      "AMD GPU [0x73BF:0x0A] @ 0000:0B:00.0"
 * - Error:         "Unknown AMD GPU @ 0000:0B:00.0"
 * 
 * @return const char* The marketing name of the GPU
 */
const char* GPUDevice::getMarketName() const {
    struct amdgpu_gpu_info gpu_info;
    static char buf[256];  // Static buffer for formatting the name
    
    // Return cached name if available
    if (!market_name_cache.empty()) {
        return market_name_cache.c_str();
    }

    if (amdgpu_query_gpu_info(device, &gpu_info) == 0) {
        // First try: Look for exact match (device ID and revision ID)
        for(size_t i = 0; i < gs_cardInfoSize; i++) {
            const auto& id = gs_cardInfo[i];
            if(gpu_info.asic_id == id.m_deviceID && gpu_info.pci_rev_id == id.m_revID) {
                snprintf(buf, sizeof(buf), "%s [0x%04x:0x%02x]", 
                        id.m_szMarketingName, 
                        (unsigned int)id.m_deviceID, 
                        (unsigned int)id.m_revID);
                market_name_cache = buf;
                return market_name_cache.c_str();
            }
        }

        // Second try: Look for partial match (only device ID)
        for(size_t i = 0; i < gs_cardInfoSize; i++) {
            const auto& id = gs_cardInfo[i];
            if(gpu_info.asic_id == id.m_deviceID) {
                snprintf(buf, sizeof(buf), "%s [0x%04x:0x%02x]", 
                        id.m_szMarketingName, 
                        (unsigned int)id.m_deviceID,     
                        (unsigned int)gpu_info.pci_rev_id);
                market_name_cache = buf;
                return market_name_cache.c_str();
            }
        }

        // Fallback: Use generic name with device info if no match found
        snprintf(buf, sizeof(buf), "AMD GPU [0x%04x:0x%02x] @ %s", 
                (unsigned int)gpu_info.asic_id,    
                (unsigned int)gpu_info.pci_rev_id, 
                pci_path.c_str());
        market_name_cache = buf;
    } else {
        // Error case: Could not query GPU info
        snprintf(buf, sizeof(buf), "Unknown AMD GPU @ %s", pci_path.c_str());
        market_name_cache = buf;
    }
    
    return market_name_cache.c_str();
}

GPUStats::GPUStats() {}

GPUStats::~GPUStats() = default;

bool GPUStats::initialize() {
    drmDevicePtr devices[64];
    int num_devices = drmGetDevices2(0, devices, 64);
    
    for (int i = 0; i < num_devices; i++) {
        if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER) {
            int fd = open(devices[i]->nodes[DRM_NODE_RENDER], O_RDWR);
            if (fd >= 0) {
                drmVersionPtr version = drmGetVersion(fd);
                if (version && !strcmp(version->name, "amdgpu")) {
                    uint32_t major, minor;
                    amdgpu_device_handle device;
                    if (amdgpu_device_initialize(fd, &major, &minor, &device) == 0) {
                        char pci_path[256];
                        snprintf(pci_path, sizeof(pci_path), "%04x:%02x:%02x.%d",
                                devices[i]->businfo.pci->domain,
                                devices[i]->businfo.pci->bus,
                                devices[i]->businfo.pci->dev,
                                devices[i]->businfo.pci->func);
                        
                        gpus.push_back(std::make_unique<GPUDevice>(fd, device, version, pci_path));
                        continue;
                    }
                }
                if (version) {
                    drmFreeVersion(version);
                }
                close(fd);
            }
        }
    }
    
    drmFreeDevices(devices, num_devices);
    return !gpus.empty();
}

GPUDevice* GPUStats::getGPU(size_t index) {
    if (index >= gpus.size()) return nullptr;
    return gpus[index].get();
}

const GPUDevice* GPUStats::getGPU(size_t index) const {
    if (index >= gpus.size()) return nullptr;
    return gpus[index].get();
} 