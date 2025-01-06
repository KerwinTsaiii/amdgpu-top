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

GPUStats::GPUStats() : fd(-1), device(nullptr), version(nullptr) {}

GPUStats::~GPUStats() {
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

bool GPUStats::initialize() {
    // Open the first available AMD GPU
    drmDevicePtr devices[64];
    int num_devices = drmGetDevices2(0, devices, 64);
    
    for (int i = 0; i < num_devices; i++) {
        if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER) {
            fd = open(devices[i]->nodes[DRM_NODE_RENDER], O_RDWR);
            if (fd >= 0) {
                version = drmGetVersion(fd);
                if (version && !strcmp(version->name, "amdgpu")) {
                    uint32_t major, minor;
                    if (amdgpu_device_initialize(fd, &major, &minor, &device) == 0) {
                        drmFreeDevices(devices, num_devices);
                        return true;
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
    return false;
}

GPUStats::Stats GPUStats::getStats() {
    Stats stats;
    uint32_t value;

    // Get GPU usage
    if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GPU_LOAD, 
                               sizeof(value), &value) == 0) {
        stats.gpu_usage = value;
    }

    // Get GPU temperature
    if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GPU_TEMP, 
                               sizeof(value), &value) == 0) {
        stats.temperature = value / 1000; // Convert to Celsius
    }

    // Get power usage
    if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GPU_AVG_POWER, 
                               sizeof(value), &value) == 0) {
        stats.power_usage = value;
    }

    // Get memory info
    struct drm_amdgpu_memory_info memory_info;
    if (amdgpu_query_info(device, AMDGPU_INFO_MEMORY, 
                        sizeof(memory_info), &memory_info) == 0) {
        stats.memory_total = memory_info.vram.total_heap_size / (1024.0 * 1024.0);
        stats.memory_cpu_accessible_total = memory_info.cpu_accessible_vram.total_heap_size / (1024.0 * 1024.0);
        stats.memory_cpu_accessible_used = memory_info.cpu_accessible_vram.heap_usage / (1024.0 * 1024.0);
        stats.memory_used = memory_info.vram.heap_usage / (1024.0 * 1024.0);
    }

    // Get clock speeds
    if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GFX_SCLK, 
                               sizeof(value), &value) == 0) {
        stats.gpu_clock = value;
    }
    if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GFX_MCLK, 
                               sizeof(value), &value) == 0) {
        stats.memory_clock = value;
    }

    // Update process information
    stats.processes = ProcessMonitor::getProcesses(device, stats.gpu_usage);

    return stats;
}

const char* GPUStats::getGPUName() const {
    return version ? version->name : "Unknown";
}

const char* GPUStats::getMarketName() const {
    struct amdgpu_gpu_info gpu_info;
    static char buf[128];  // Static buffer for the formatted string
    
    if (amdgpu_query_gpu_info(device, &gpu_info) == 0) {
        // First try to find exact match in our database
        for (size_t i = 0; i < gs_cardInfoSize; i++) {
            const auto& id = gs_cardInfo[i];
            if (gpu_info.asic_id == id.m_deviceID && gpu_info.pci_rev_id == id.m_revID) {
                snprintf(buf, sizeof(buf), "%s [0x%04lX:0x%02lX]", 
                        id.m_szMarketingName, (unsigned long)id.m_deviceID, (unsigned long)id.m_revID);
                return buf;
            }
        }

        // If no exact match, try to find by asic_id only
        for (size_t i = 0; i < gs_cardInfoSize; i++) {
            const auto& id = gs_cardInfo[i];
            if (gpu_info.asic_id == id.m_deviceID) {
                snprintf(buf, sizeof(buf), "%s [0x%04lX:0x%02lX]", 
                        id.m_szMarketingName, (unsigned long)id.m_deviceID, (unsigned long)gpu_info.pci_rev_id);
                return buf;
            }
        }

        // If still no match, return generic name with IDs
        snprintf(buf, sizeof(buf), "AMD GPU [0x%04X:0x%02X]", 
                gpu_info.asic_id, gpu_info.pci_rev_id);
        return buf;
    }
    return "Unknown AMD GPU";
} 