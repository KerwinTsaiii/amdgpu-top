#pragma once

#include <fstream>
#include <string>
#include <mutex>
#include <iostream>

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void init(bool debug_mode) {
        std::lock_guard<std::mutex> lock(mutex_);
        debug_enabled_ = debug_mode;
        if (debug_mode) {
            log_file_.open("/tmp/amdgpu-top-debug.log", std::ios::app);
        }
    }

    void debug(const std::string& message) {
        if (!debug_enabled_) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (log_file_.is_open()) {
            log_file_ << "[DEBUG] " << message << std::endl;
        }
        #ifdef DEBUG_BUILD
        std::cerr << "[DEBUG] " << message << std::endl;
        #endif
    }

    ~Logger() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex mutex_;
    std::ofstream log_file_;
    bool debug_enabled_ = false;
};

#define LOG_DEBUG(msg) Logger::getInstance().debug(msg) 