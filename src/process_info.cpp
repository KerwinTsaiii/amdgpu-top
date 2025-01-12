#include "process_info.hpp"
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include "logger.hpp"

std::vector<ProcessCache> ProcessMonitor::last_process_cache;

bool ProcessMonitor::isDRMFd(int fd_dir_fd, const char* name) {
    struct stat stat_buf;
    int ret = fstatat(fd_dir_fd, name, &stat_buf, 0);
    return ret == 0 && (stat_buf.st_mode & S_IFMT) == S_IFCHR && major(stat_buf.st_rdev) == 226;
}

bool ProcessMonitor::parseFdinfo(FILE* fdinfo_file, ProcessInfo& proc, unsigned& client_id) {
    static const char* DRM_PDEV_OLD = "pdev";
    static const char* DRM_VRAM_OLD = "vram mem";
    static const char* DRM_VRAM_NEW = "drm-memory-vram";
    static const char* DRM_GFX_OLD = "gfx";
    static const char* DRM_GFX_NEW = "drm-engine-gfx";
    static const char* DRM_COMPUTE_OLD = "compute";
    static const char* DRM_COMPUTE_NEW = "drm-engine-compute";
    static const char* DRM_DEC_OLD = "dec";
    static const char* DRM_DEC_NEW = "drm-engine-dec";
    static const char* DRM_ENC_OLD = "enc";
    static const char* DRM_ENC_NEW = "drm-engine-enc";

    char* line = nullptr;
    size_t line_buf_size = 0;
    ssize_t count = 0;
    bool has_engine = false;
    bool client_id_set = false;
    std::string current_pasid;
    uint64_t current_memory = 0;
    static std::map<std::string, bool> processed_pasids;  // Track processed PASIDs

    // Clear processed PASIDs at the start of each process
    if (proc.memory_usage == 0) {
        processed_pasids.clear();
    }

    Logger::debug("=== Begin parsing fdinfo for PID " + std::to_string(proc.pid) + " ===");

    // First pass: get current PASID
    while ((count = getline(&line, &line_buf_size, fdinfo_file)) != -1) {
        if (strstr(line, "pasid:")) {
            char* val = strchr(line, ':');
            if (val) {
                val++;
                while (*val && isspace(*val)) val++;
                current_pasid = std::string(val);
                if (current_pasid.back() == '\n') {
                    current_pasid.pop_back();
                }
                Logger::debug("  Found PASID: " + current_pasid);
                break;
            }
        }
    }
    rewind(fdinfo_file);

    // Skip if we've already processed this PASID
    if (processed_pasids[current_pasid]) {
        Logger::debug("  Skipping already processed PASID: " + current_pasid);
        free(line);
        return has_engine;
    }

    // Second pass: parse all values
    while ((count = getline(&line, &line_buf_size, fdinfo_file)) != -1) {
        // Remove newline
        if (line[count - 1] == '\n') {
            line[--count] = '\0';
        }

        char *key = line;
        char *val = strchr(line, ':');
        if (!val) continue;
        *val++ = '\0';
        while (*val && isspace(*val)) val++;
        
        // Log raw fdinfo line
        Logger::debug("  " + std::string(key) + ": " + std::string(val));

        // Parse client ID
        if (strstr(key, "drm-client-id")) {
            char *endptr;
            client_id = strtoul(val, &endptr, 10);
            if (!*endptr) {
                client_id_set = true;
                Logger::debug("    -> Found client ID: " + std::to_string(client_id));
            }
            continue;
        }

        // Parse VRAM usage
        if (!strcmp(key, DRM_VRAM_OLD) || !strcmp(key, DRM_VRAM_NEW)) {
            char *endptr;
            unsigned long mem_kb = strtoul(val, &endptr, 10);
            if (endptr != val && (!strcmp(endptr, " kB") || !strcmp(endptr, " KiB"))) {
                current_memory = mem_kb * 1024;
                if (!processed_pasids[current_pasid]) {
                    proc.memory_usage += current_memory;  // Accumulate memory only for new PASIDs
                    processed_pasids[current_pasid] = true;
                    Logger::debug("    -> Added VRAM usage for PASID " + current_pasid + 
                                ": " + std::to_string(mem_kb) + " KiB" +
                                " (Total: " + std::to_string(proc.memory_usage / 1024) + " KiB)");
                }
                has_engine = true;
            }
            continue;
        }

        // Parse engine usage times
        if (strstr(key, "drm-engine-gfx")) {
            char *endptr;
            uint64_t time_spent = strtoull(val, &endptr, 10);
            if (endptr != val && !strcmp(endptr, " ns")) {
                proc.gfx_engine_used = time_spent;
                has_engine = true;
                Logger::debug("    -> Found GFX engine time: " + std::to_string(time_spent) + " ns");
            }
        }
        else if (strstr(key, "drm-engine-compute")) {
            char *endptr;
            uint64_t time_spent = strtoull(val, &endptr, 10);
            if (endptr != val && !strcmp(endptr, " ns")) {
                proc.compute_engine_used = time_spent;
                has_engine = true;
                Logger::debug("    -> Found Compute engine time: " + std::to_string(time_spent) + " ns");
            }
        }
        else if (strstr(key, "drm-engine-dec")) {
            char *endptr;
            uint64_t time_spent = strtoull(val, &endptr, 10);
            if (endptr != val && !strcmp(endptr, " ns")) {
                proc.dec_engine_used = time_spent;
                has_engine = true;
                Logger::debug("    -> Found Decode engine time: " + std::to_string(time_spent) + " ns");
            }
        }
        else if (strstr(key, "drm-engine-enc")) {
            char *endptr;
            uint64_t time_spent = strtoull(val, &endptr, 10);
            if (endptr != val && !strcmp(endptr, " ns")) {
                proc.enc_engine_used = time_spent;
                has_engine = true;
                Logger::debug("    -> Found Encode engine time: " + std::to_string(time_spent) + " ns");
            }
        }
    }

    Logger::debug("=== End parsing fdinfo for PID " + std::to_string(proc.pid) + 
                 " (Total memory: " + std::to_string(proc.memory_usage / 1024) + " KiB) ===\n");

    free(line);
    return has_engine;
}

// Helper function to check if a field is valid
bool isFieldValid(uint64_t value) {
    return value != 0;
}

// Helper function to calculate rounded usage percentage
float calculateUsagePercentage(uint64_t current, uint64_t previous, uint64_t time_elapsed) {
    if (current < previous || time_elapsed == 0) return 0.0f;
    uint64_t delta = current - previous;
    if (delta > time_elapsed) return 0.0f;
    return (float)delta / time_elapsed * 100.0f;
}

void ProcessMonitor::updateEngineUsage(ProcessInfo& proc, const ProcessCache* cache, const timespec& current_time) {
    if (!cache) {
        proc.last_measurement_time = current_time;
        Logger::debug("No cache found for PID " + std::to_string(proc.pid) + ", initializing cache");
        return;
    }

    uint64_t time_elapsed = (current_time.tv_sec - cache->last_measurement_time.tv_sec) * 1000000000ULL + 
                           (current_time.tv_nsec - cache->last_measurement_time.tv_nsec);
    
    Logger::debug("Process " + std::to_string(proc.pid) + " time elapsed: " + std::to_string(time_elapsed) + " ns");

    if (time_elapsed == 0) {
        Logger::debug("Zero time elapsed, skipping usage calculation");
        return;
    }

    // Calculate GFX usage
    if (isFieldValid(proc.gfx_engine_used) && isFieldValid(cache->gfx_engine_used) &&
        proc.gfx_engine_used >= cache->gfx_engine_used) {
        proc.gfx_usage = calculateUsagePercentage(proc.gfx_engine_used, cache->gfx_engine_used, time_elapsed);
        Logger::debug("GFX usage: " + std::to_string(proc.gfx_usage) + "% " +
                     "(current: " + std::to_string(proc.gfx_engine_used) + 
                     ", prev: " + std::to_string(cache->gfx_engine_used) + ")");
    }

    // Calculate Compute usage
    if (isFieldValid(proc.compute_engine_used) && isFieldValid(cache->compute_engine_used) &&
        proc.compute_engine_used >= cache->compute_engine_used) {
        proc.compute_usage = calculateUsagePercentage(proc.compute_engine_used, 
                                                    cache->compute_engine_used, time_elapsed);
        Logger::debug("Compute usage: " + std::to_string(proc.compute_usage) + "% " +
                     "(current: " + std::to_string(proc.compute_engine_used) + 
                     ", prev: " + std::to_string(cache->compute_engine_used) + ")");
    }

    // Calculate Decode usage
    if (isFieldValid(proc.dec_engine_used) && isFieldValid(cache->dec_engine_used) &&
        proc.dec_engine_used >= cache->dec_engine_used) {
        proc.dec_usage = calculateUsagePercentage(proc.dec_engine_used, 
                                                cache->dec_engine_used, time_elapsed);
        Logger::debug("Decode usage: " + std::to_string(proc.dec_usage) + "% " +
                     "(current: " + std::to_string(proc.dec_engine_used) + 
                     ", prev: " + std::to_string(cache->dec_engine_used) + ")");
    }

    // Calculate Encode usage
    if (isFieldValid(proc.enc_engine_used) && isFieldValid(cache->enc_engine_used) &&
        proc.enc_engine_used >= cache->enc_engine_used) {
        proc.enc_usage = calculateUsagePercentage(proc.enc_engine_used, 
                                                cache->enc_engine_used, time_elapsed);
        Logger::debug("Encode usage: " + std::to_string(proc.enc_usage) + "% " +
                     "(current: " + std::to_string(proc.enc_engine_used) + 
                     ", prev: " + std::to_string(cache->enc_engine_used) + ")");
    }

    proc.last_measurement_time = current_time;
}

std::vector<ProcessInfo> ProcessMonitor::getProcesses(amdgpu_device_handle device, float gpu_usage) {
    std::vector<ProcessInfo> processes;
    std::vector<ProcessCache> current_cache;
    timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    Logger::debug("Starting process scan");

    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return processes;

    struct dirent* proc_entry;
    while ((proc_entry = readdir(proc_dir))) {
        if (proc_entry->d_type != DT_DIR || !isdigit(proc_entry->d_name[0])) continue;

        pid_t pid = std::stoi(proc_entry->d_name);
        Logger::debug("Checking process: " + std::to_string(pid));

        ProcessInfo proc;
        proc.pid = pid;
        bool uses_gpu = false;
        unsigned client_id = 0;

        // Check fdinfo
        std::string fdinfo_path = "/proc/" + std::string(proc_entry->d_name) + "/fdinfo";
        int fdinfo_dir_fd = open(fdinfo_path.c_str(), O_DIRECTORY);
        if (fdinfo_dir_fd >= 0) {
            std::string fd_path = "/proc/" + std::string(proc_entry->d_name) + "/fd";
            DIR* fd_dir = opendir(fd_path.c_str());
            if (fd_dir) {
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
                closedir(fd_dir);
            }
            close(fdinfo_dir_fd);
        }

        if (uses_gpu) {
            // Get process name
            std::string comm_path = "/proc/" + std::string(proc_entry->d_name) + "/comm";
            std::ifstream comm_file(comm_path);
            if (comm_file) {
                std::getline(comm_file, proc.name);
                Logger::debug("Found GPU process: " + proc.name + " (PID: " + 
                            std::to_string(pid) + ")");
            }

            // Find matching cache entry
            const ProcessCache* cache_entry = nullptr;
            for (const auto& cache : last_process_cache) {
                if (cache.pid == pid && cache.client_id == client_id) {
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
            new_cache.gfx_engine_used = proc.gfx_engine_used;
            new_cache.compute_engine_used = proc.compute_engine_used;
            new_cache.enc_engine_used = proc.enc_engine_used;
            new_cache.dec_engine_used = proc.dec_engine_used;
            new_cache.last_measurement_time = current_time;
            current_cache.push_back(new_cache);

            processes.push_back(proc);
        }
    }
    closedir(proc_dir);

    Logger::debug("Found " + std::to_string(processes.size()) + " GPU processes");

    // Update cache for next iteration
    last_process_cache = std::move(current_cache);

    return processes;
}

uint64_t ProcessMonitor::getTimeDiffNs(const timespec& start, const timespec& end) {
    return (end.tv_sec - start.tv_sec) * 1000000000ULL + (end.tv_nsec - start.tv_nsec);
} 