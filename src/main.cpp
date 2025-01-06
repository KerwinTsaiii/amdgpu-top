#include <ftxui/component/screen_interactive.hpp>
#include <chrono>
#include <thread>
#include <iostream>
#include <mutex>
#include "layout.hpp"

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  -i, --interval <ms>   Update interval in milliseconds (default: 1000)\n"
              << "  -h, --help            Show this help message\n";
}

int main(int argc, char* argv[]) {
    // Default update interval (1 second)
    int update_interval = 1000;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-i" || arg == "--interval") {
            if (i + 1 < argc) {
                try {
                    update_interval = std::stoi(argv[++i]);
                    if (update_interval < 100) {
                        std::cerr << "Warning: Update interval too low, setting to minimum (100ms)\n";
                        update_interval = 100;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid interval value\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: -i/--interval requires a value\n";
                return 1;
            }
        }
    }

    try {
        auto screen = ftxui::ScreenInteractive::Fullscreen();
        Layout layout;

        // Shared state between threads
        std::mutex stats_mutex;
        GPUStats::Stats current_stats;
        bool stats_ready = false;

        auto renderer = ftxui::Renderer([&] {
            std::lock_guard<std::mutex> lock(stats_mutex);
            if (stats_ready) {
                return layout.render(current_stats);
            }
            return layout.render();  // Initial render or fallback
        });

        std::atomic<bool> refresh_ui = true;
        std::thread refresh_thread([&] {
            while (refresh_ui) {
                // Get new stats
                auto new_stats = layout.getStats();
                
                // Update shared state
                {
                    std::lock_guard<std::mutex> lock(stats_mutex);
                    current_stats = new_stats;
                    stats_ready = true;
                }

                // Trigger UI update
                screen.Post(ftxui::Event::Custom);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(update_interval));
            }
        });

        screen.Loop(renderer);
        refresh_ui = false;
        refresh_thread.join();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 