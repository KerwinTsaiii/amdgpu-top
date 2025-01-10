#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <sys/types.h>
#include <libdrm/amdgpu.h>
#include <time.h>
#include <libdrm/amdgpu_drm.h>
#include <xf86drm.h>

struct ROCkProcessInfo {
    uint32_t pasid;
    uint64_t vram_usage;
    uint64_t compute_time;
    timespec last_timestamp;
};

struct ProcessInfo {
    pid_t pid;
    std::string name;
    float gfx_usage = 0;
    float compute_usage = 0;
    float enc_usage = 0;
    float dec_usage = 0;
    uint64_t memory_usage = 0;
    
    // Track engine usage time
    uint64_t gfx_engine_used = 0;
    uint64_t compute_engine_used = 0;
    uint64_t enc_engine_used = 0;
    uint64_t dec_engine_used = 0;
    timespec last_measurement_time = {0, 0};
    bool is_rocm = false;
    unsigned client_id = 0;
    ROCkProcessInfo rock_info;
};

struct ProcessCache {
    pid_t pid;
    std::string pdev;
    unsigned client_id;
    
    uint64_t gfx_engine_used;
    uint64_t compute_engine_used;
    uint64_t enc_engine_used;
    uint64_t dec_engine_used;
    timespec last_measurement_time;
    
    bool operator==(const ProcessCache& other) const {
        return pid == other.pid && 
               pdev == other.pdev && 
               client_id == other.client_id;
    }
};

class ProcessMonitor {
public:
    static std::vector<ProcessInfo> getProcesses(amdgpu_device_handle device, float gpu_usage);
private:
    static bool parseFdinfo(FILE* fdinfo_file, ProcessInfo& proc, unsigned& client_id);
    static void updateEngineUsage(ProcessInfo& proc, const ProcessCache* cache, const timespec& current_time);
    static bool isDRMFd(int fd_dir_fd, const char* name);
    static uint64_t getTimeDiffNs(const timespec& start, const timespec& end);
    
    static std::vector<ProcessCache> last_process_cache;
    
    static bool updateROCkProcessInfo(ProcessInfo& proc, amdgpu_device_handle device);
    static bool getROCkComputeUsage(ProcessInfo& proc, amdgpu_device_handle device);
    static bool getROCkMemoryUsage(ProcessInfo& proc, amdgpu_device_handle device);
    static bool isROCmProcess(pid_t pid);
}; 