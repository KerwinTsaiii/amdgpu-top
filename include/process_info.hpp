#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <sys/types.h>
#include <libdrm/amdgpu.h>
#include <time.h>
#include <libdrm/amdgpu_drm.h>
#include <xf86drm.h>
#include "uthash.h"
#include <map>

struct ProcessMemoryInfo {
    uint64_t memory_usage;
    std::string pasid;
};

struct ROCkProcessInfo {
    uint32_t pasid;
    uint64_t vram_usage;
};

struct ProcessInfo {
    pid_t pid;
    std::string name;
    bool is_rocm;
    
    // Usage percentages
    float gfx_usage;
    float compute_usage;
    float enc_usage;
    float dec_usage;
    
    // Engine time usage in nanoseconds
    uint64_t gfx_engine_used;
    uint64_t compute_engine_used;
    uint64_t enc_engine_used;
    uint64_t dec_engine_used;
    
    // Memory usage in bytes
    uint64_t memory_usage;
    
    // Timestamp of last measurement
    timespec last_measurement_time;
    
    // ROCm specific info
    ROCkProcessInfo rock_info;

    // Constructor to initialize values
    ProcessInfo() : 
        pid(0), 
        is_rocm(false),
        gfx_usage(0),
        compute_usage(0),
        enc_usage(0),
        dec_usage(0),
        gfx_engine_used(0),
        compute_engine_used(0),
        enc_engine_used(0),
        dec_engine_used(0),
        memory_usage(0) {
        last_measurement_time = {0, 0};
        rock_info = {0, 0};
    }
};

struct ProcessCache {
    pid_t pid;
    unsigned client_id;
    std::string pdev;
    uint64_t gfx_engine_used;
    uint64_t compute_engine_used;
    uint64_t enc_engine_used;
    uint64_t dec_engine_used;
    timespec last_measurement_time;
};

// Process cache structure for AMDGPU
struct amdgpu_process_info_cache {
    struct {
        unsigned client_id;
        pid_t pid;
        char* pdev;
    } client_id;
    uint64_t gfx_engine_used;
    uint64_t compute_engine_used;
    uint64_t enc_engine_used;
    uint64_t dec_engine_used;
    timespec last_measurement_tstamp;
    unsigned char valid[1];  // Bitmap for valid fields
    UT_hash_handle hh;
};

typedef bool (*fdinfo_callback)(ProcessInfo& proc, FILE* fdinfo, void* data);

struct FdinfoCallback {
    fdinfo_callback callback;
    void* data;
};

class ProcessMonitor {
public:
    static std::vector<ProcessInfo> getProcesses(amdgpu_device_handle device, float gpu_usage);

private:
    static bool parseFdinfo(FILE* fdinfo_file, ProcessInfo& proc, unsigned& client_id);
    static bool isDRMFd(int fd_dir_fd, const char* name);
    static bool isROCmProcess(pid_t pid);
    static bool updateROCkProcessInfo(ProcessInfo& proc, amdgpu_device_handle device);
    static bool getROCkComputeUsage(ProcessInfo& proc, amdgpu_device_handle device);
    static bool getROCkMemoryUsage(ProcessInfo& proc, amdgpu_device_handle device);
    static uint64_t getTimeDiffNs(const timespec& start, const timespec& end);
    static void updateEngineUsage(ProcessInfo& proc, const ProcessCache* cache, const timespec& current_time);
    
    static std::vector<ProcessCache> last_process_cache;
    static struct amdgpu_process_info_cache* last_update_process_cache;
    static struct amdgpu_process_info_cache* current_update_process_cache;
    static std::vector<FdinfoCallback> fdinfo_callbacks;

    static void swap_process_cache_for_next_update();

    static std::map<pid_t, std::map<std::string, ProcessMemoryInfo>> process_memory_map;
}; 