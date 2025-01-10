#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <ctime>
#include <sstream>

class Logger {
public:
    enum Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    static void init(const std::string& log_file = "", Level level = INFO) {
        instance().log_level = level;
        if (!log_file.empty()) {
            instance().log_file.open(log_file, std::ios::app);
        }
    }

    static void debug(const std::string& message) { log(DEBUG, message); }
    static void info(const std::string& message) { log(INFO, message); }
    static void warning(const std::string& message) { log(WARNING, message); }
    static void error(const std::string& message) { log(ERROR, message); }

private:
    Logger() : log_level(INFO) {}
    
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    static void log(Level level, const std::string& message) {
        if (level < instance().log_level) return;

        std::string level_str;
        switch (level) {
            case DEBUG: level_str = "DEBUG"; break;
            case INFO: level_str = "INFO"; break;
            case WARNING: level_str = "WARNING"; break;
            case ERROR: level_str = "ERROR"; break;
        }

        std::time_t now = std::time(nullptr);
        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

        std::stringstream log_message;
        log_message << "[" << timestamp << "] [" << level_str << "] " << message << std::endl;

        if (instance().log_file.is_open()) {
            instance().log_file << log_message.str();
            instance().log_file.flush();
        }
        
        #ifdef DEBUG_BUILD
        std::cout << log_message.str();
        #endif
    }

    Level log_level;
    std::ofstream log_file;
};

#define LOG_DEBUG(msg) Logger::getInstance().debug(msg) 