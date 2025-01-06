#include "gpu_stats.hpp"
#include "device_info/DeviceInfo.h"
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include "amdgpu_ids.hpp"

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

    // Get process information
    drmDevicePtr devices[64];
    int num_devices = drmGetDevices2(0, devices, 64);
    
    for (int i = 0; i < num_devices; i++) {
        if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER) {
            // Read process information from /proc
            DIR *proc_dir = opendir("/proc");
            if (proc_dir) {
                struct dirent *entry;
                while ((entry = readdir(proc_dir))) {
                    if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
                        ProcessInfo proc_info = {0};
                        proc_info.pid = std::stoi(entry->d_name);
                        
                        // Read process name
                        std::string comm_path = "/proc/" + std::string(entry->d_name) + "/comm";
                        std::ifstream comm_file(comm_path);
                        if (comm_file) {
                            std::getline(comm_file, proc_info.name);
                        }

                        // Read GPU usage from fdinfo
                        std::string fdinfo_path = "/proc/" + std::string(entry->d_name) + "/fdinfo";
                        DIR *fd_dir = opendir(fdinfo_path.c_str());
                        if (fd_dir) {
                            struct dirent *fd_entry;
                            while ((fd_entry = readdir(fd_dir))) {
                                // Check DRM usage
                                std::string fd_path = fdinfo_path + "/" + fd_entry->d_name;
                                std::ifstream fd_file(fd_path);
                                std::string line;
                                while (std::getline(fd_file, line)) {
                                    if (line.find("drm-engine-gfx") != std::string::npos) {
                                        proc_info.gpu_usage = 1.0f; // Process is using GPU
                                    }
                                }
                            }
                            closedir(fd_dir);
                        }
                        
                        if (proc_info.gpu_usage > 0) {
                            stats.processes.push_back(proc_info);
                        }
                    }
                }
                closedir(proc_dir);
            }
            break;
        }
    }
    drmFreeDevices(devices, num_devices);

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
                snprintf(buf, sizeof(buf), "%s [0x%04X:0x%02X]", 
                        id.m_szMarketingName, id.m_deviceID, id.m_revID);
                return buf;
            }
        }

        // If no exact match, try to find by asic_id only
        for (size_t i = 0; i < gs_cardInfoSize; i++) {
            const auto& id = gs_cardInfo[i];
            if (gpu_info.asic_id == id.m_deviceID) {
                snprintf(buf, sizeof(buf), "%s [0x%04X:0x%02X]", 
                        id.m_szMarketingName, id.m_deviceID, gpu_info.pci_rev_id);
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