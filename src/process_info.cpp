#include "process_info.hpp"
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <libdrm/amdgpu_drm.h>
#include <libdrm/amdgpu.h>

std::vector<ProcessCache> ProcessMonitor::last_process_cache;

bool ProcessMonitor::isDRMFd(int fd_dir_fd, const char* name) {
    struct stat stat_buf;
    int ret = fstatat(fd_dir_fd, name, &stat_buf, 0);
    return ret == 0 && (stat_buf.st_mode & S_IFMT) == S_IFCHR && major(stat_buf.st_rdev) == 226;
}

bool ProcessMonitor::isROCmProcess(pid_t pid) {
    char path[256];
    char buf[1024];
    bool has_rocm_lib = false;
    bool has_pasid = false;
    
    // Check if the process has ROCm libraries
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE *fp = fopen(path, "r");
    if (fp) {
        while (fgets(buf, sizeof(buf), fp)) {
            if (strstr(buf, "libhsa-runtime64.so") ||
                strstr(buf, "librocm_smi64.so") ||
                strstr(buf, "libamdgpu.so")) {
                has_rocm_lib = true;
                break;
            }
        }
        fclose(fp);
    }
    
    // If no ROCm libraries found, return false
    if (!has_rocm_lib) {
        return false;
    }
    
    // 2. Check PASID in fdinfo
    snprintf(path, sizeof(path), "/proc/%d/fdinfo", pid);
    DIR *dr = opendir(path);
    if (dr) {
        struct dirent *de;
        while ((de = readdir(dr)) != NULL) {
            if (isdigit(de->d_name[0])) {
                snprintf(path, sizeof(path), "/proc/%d/fdinfo/%s", pid, de->d_name);
                fp = fopen(path, "r");
                if (fp) {
                    while (fgets(buf, sizeof(buf), fp)) {
                        if (strstr(buf, "pasid:")) {
                            has_pasid = true;
                            break;
                        }
                    }
                    fclose(fp);
                    if (has_pasid) break;
                }
            }
        }
        closedir(dr);
    }
    
    // Must satisfy both conditions: has ROCm library and has PASID
    return has_rocm_lib && has_pasid;
}

bool ProcessMonitor::parseFdinfo(FILE* fdinfo_file, ProcessInfo& proc, unsigned& client_id) {
    char line[256];
    bool has_engine = false;
    client_id = 0;

    // Check if it's a ROCm process first
    proc.is_rocm = isROCmProcess(proc.pid);

    // Read basic information
    while (fgets(line, sizeof(line), fdinfo_file)) {
        char *key, *val;
        char *saveptr;
        
        key = strtok_r(line, ":", &saveptr);
        if (!key) continue;
        
        val = saveptr;
        while (val && *val && isspace(*val)) val++;
        if (!val) continue;

        // Parse basic information
        if (strstr(key, "pasid")) {
            char *endptr;
            uint32_t pasid = strtoul(val, &endptr, 10);
            if (proc.is_rocm && pasid > 0) {
                proc.rock_info.pasid = pasid;
            }
        }
        else if (strstr(key, "drm-client-id")) {
            char *endptr;
            client_id = strtoul(val, &endptr, 10);
        }
        else if (strstr(key, "drm-memory-vram")) {
            char *endptr;
            unsigned long mem_kb = strtoul(val, &endptr, 10);
            if (endptr != val && !strcmp(endptr, " KiB\n")) {
                proc.memory_usage = mem_kb * 1024;
                has_engine = true;
            }
        }
    }

    // If it's a ROCm process, return true
    if (proc.is_rocm) {
        return true;
    }

    // Non-ROCm processes need to parse engine usage time
    rewind(fdinfo_file);
    
    while (fgets(line, sizeof(line), fdinfo_file)) {
        char *key, *val;
        char *saveptr;
        
        key = strtok_r(line, ":", &saveptr);
        if (!key) continue;
        
        val = saveptr;
        while (val && *val && isspace(*val)) val++;
        if (!val) continue;

        // Parse engine usage time
        char *endptr;
        uint64_t time_spent;
        
        if (strstr(key, "drm-engine-gfx")) {
            time_spent = strtoull(val, &endptr, 10);
            if (endptr != val && !strcmp(endptr, " ns\n")) {
                proc.gfx_engine_used = time_spent;
                has_engine = true;
            }
        }
        else if (strstr(key, "drm-engine-compute")) {
            time_spent = strtoull(val, &endptr, 10);
            if (endptr != val && !strcmp(endptr, " ns\n")) {
                proc.compute_engine_used = time_spent;
                has_engine = true;
            }
        }
        else if (strstr(key, "drm-engine-dec")) {
            time_spent = strtoull(val, &endptr, 10);
            if (endptr != val && !strcmp(endptr, " ns\n")) {
                proc.dec_engine_used = time_spent;
                has_engine = true;
            }
        }
        else if (strstr(key, "drm-engine-enc")) {
            time_spent = strtoull(val, &endptr, 10);
            if (endptr != val && !strcmp(endptr, " ns\n")) {
                proc.enc_engine_used = time_spent;
                has_engine = true;
            }
        }
    }

    return has_engine;
}

bool ProcessMonitor::updateROCkProcessInfo(ProcessInfo& proc, amdgpu_device_handle device) {
    if (!proc.is_rocm) return false;

    bool success = false;
    success |= getROCkComputeUsage(proc, device);
    success |= getROCkMemoryUsage(proc, device);
    return success;
}

bool ProcessMonitor::getROCkComputeUsage(ProcessInfo& proc, amdgpu_device_handle device) {
    // Use ROCk API to get compute usage
    // TODO: Implement this function
    return true;
}

bool ProcessMonitor::getROCkMemoryUsage(ProcessInfo& proc, amdgpu_device_handle device) {
    // Use ROCk API to get memory usage
    // TODO: Implement this function
    return true;
}

uint64_t ProcessMonitor::getTimeDiffNs(const timespec& start, const timespec& end) {
    return (end.tv_sec - start.tv_sec) * 1000000000ULL + (end.tv_nsec - start.tv_nsec);
}

void ProcessMonitor::updateEngineUsage(ProcessInfo& proc, const ProcessCache* cache, const timespec& current_time) {
    if (!cache) {
        proc.last_measurement_time = current_time;
        return;
    }

    uint64_t time_elapsed = getTimeDiffNs(cache->last_measurement_time, current_time);
    if (time_elapsed == 0) return;

    // Calculate deltas and ensure they're valid
    if (proc.gfx_engine_used >= cache->gfx_engine_used) {
        uint64_t delta = proc.gfx_engine_used - cache->gfx_engine_used;
        if (delta <= time_elapsed) {
            proc.gfx_usage = (float)delta / time_elapsed * 100.0f;
        }
    }

    if (proc.compute_engine_used >= cache->compute_engine_used) {
        uint64_t delta = proc.compute_engine_used - cache->compute_engine_used;
        if (delta <= time_elapsed) {
            proc.compute_usage = (float)delta / time_elapsed * 100.0f;
        }
    }

    if (proc.enc_engine_used >= cache->enc_engine_used) {
        uint64_t delta = proc.enc_engine_used - cache->enc_engine_used;
        if (delta <= time_elapsed) {
            proc.enc_usage = (float)delta / time_elapsed * 100.0f;
        }
    }

    if (proc.dec_engine_used >= cache->dec_engine_used) {
        uint64_t delta = proc.dec_engine_used - cache->dec_engine_used;
        if (delta <= time_elapsed) {
            proc.dec_usage = (float)delta / time_elapsed * 100.0f;
        }
    }

    proc.last_measurement_time = current_time;
}

std::vector<ProcessInfo> ProcessMonitor::getProcesses(amdgpu_device_handle device, float gpu_usage) {
    std::vector<ProcessInfo> processes;
    std::vector<ProcessCache> current_cache;
    timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    // Get device PCI path
    char pdev[256];
    int fd = amdgpu_device_get_fd(device);
    struct stat st;
    if (fstat(fd, &st) == 0) {
        snprintf(pdev, sizeof(pdev), "%d:%d", major(st.st_rdev), minor(st.st_rdev));
    } else {
        // Fallback to just using the fd as identifier
        snprintf(pdev, sizeof(pdev), "fd%d", fd);
    }

    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return processes;

    struct dirent* proc_entry;
    while ((proc_entry = readdir(proc_dir))) {
        if (proc_entry->d_type != DT_DIR || !isdigit(proc_entry->d_name[0])) {
            continue;
        }

        pid_t pid = std::stoi(proc_entry->d_name);
        
        // Open fdinfo directory
        std::string fdinfo_path = "/proc/" + std::string(proc_entry->d_name) + "/fdinfo";
        int fdinfo_dir_fd = open(fdinfo_path.c_str(), O_DIRECTORY);
        if (fdinfo_dir_fd < 0) continue;

        // Open fd directory
        std::string fd_path = "/proc/" + std::string(proc_entry->d_name) + "/fd";
        DIR* fd_dir = opendir(fd_path.c_str());
        if (!fd_dir) {
            close(fdinfo_dir_fd);
            continue;
        }

        ProcessInfo proc;
        proc.pid = pid;
        bool uses_gpu = false;
        unsigned client_id = 0;

        struct dirent* fd_entry;
        while ((fd_entry = readdir(fd_dir))) {
            if (!isdigit(fd_entry->d_name[0])) continue;

            if (isDRMFd(dirfd(fd_dir), fd_entry->d_name)) {
                int fdinfo_fd = openat(fdinfo_dir_fd, fd_entry->d_name, O_RDONLY);
                if (fdinfo_fd >= 0) {
                    FILE* fdinfo_file = fdopen(fdinfo_fd, "r");
                    if (fdinfo_file) {
                        if (parseFdinfo(fdinfo_file, proc, client_id)) {
                            uses_gpu = true;
                        }
                        fclose(fdinfo_file);
                    }
                    close(fdinfo_fd);
                }
            }
        }

        if (uses_gpu) {
            // Get process name
            std::string comm_path = "/proc/" + std::string(proc_entry->d_name) + "/comm";
            std::ifstream comm_file(comm_path);
            if (comm_file) {
                std::getline(comm_file, proc.name);
            }

            if (proc.is_rocm) {
                // Use ROCk API to update process info
                if (updateROCkProcessInfo(proc, device)) {
                    processes.push_back(proc);
                }
            } else {
                // Find matching cache entry
                const ProcessCache* cache_entry = nullptr;
                for (const auto& cache : last_process_cache) {
                    if (cache.pid == pid && cache.client_id == client_id && cache.pdev == pdev) {
                        cache_entry = &cache;
                        break;
                    }
                }

                // Update usage based on engine times
                updateEngineUsage(proc, cache_entry, current_time);

                // Store current state in cache
                ProcessCache new_cache;
                new_cache.pid = pid;
                new_cache.client_id = client_id;
                new_cache.pdev = pdev;
                new_cache.gfx_engine_used = proc.gfx_engine_used;
                new_cache.compute_engine_used = proc.compute_engine_used;
                new_cache.enc_engine_used = proc.enc_engine_used;
                new_cache.dec_engine_used = proc.dec_engine_used;
                new_cache.last_measurement_time = current_time;
                current_cache.push_back(new_cache);

                processes.push_back(proc);
            }
        }

        closedir(fd_dir);
        close(fdinfo_dir_fd);
    }
    closedir(proc_dir);

    // Update cache for next iteration
    last_process_cache = std::move(current_cache);

    return processes;
} 