#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <chrono>
#include <thread>
#include "layout.hpp"
#include <atomic>
#include <iostream>
#include <cstring>
#include "logger.hpp"

using namespace ftxui;

void printTextMode(Layout& layout) {
    auto metrics = layout.getMetricsText();
    std::cout << metrics << std::endl;
}

void printUsage() {
    std::cout << "Usage: amdgpu-top [OPTIONS]\n"
              << "Options:\n"
              << "  -t, --text     Text-only mode\n"
              << "  -h, --help     Show this help message\n";
}

int main(int argc, char* argv[]) {
    #ifdef DEBUG_BUILD
    Logger::init("/tmp/amdgpu-top.log", Logger::DEBUG);
    Logger::info("Starting amdgpu-top in debug mode");
    #endif

    bool text_mode = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--text") == 0) {
            text_mode = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage();
            return 0;
        }
    }

    try {
        Layout layout;

        if (text_mode) {
            // Text mode: continuously print stats
            while (true) {
                printTextMode(layout);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } else {
            // Interactive TUI mode
            auto screen = ScreenInteractive::Fullscreen();
            
            auto renderer = Renderer([&] {
                return layout.render() | flex;
            });

            auto component = Container::Vertical({
                renderer
            });

            std::atomic<bool> refresh_ui = true;
            std::thread refresh_thread([&] {
                while (refresh_ui) {
                    using namespace std::chrono_literals;
                    std::this_thread::sleep_for(1s);
                    screen.Post([&] { screen.RequestAnimationFrame(); });
                }
            });

            screen.Loop(component);
            
            refresh_ui = false;
            refresh_thread.join();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 
