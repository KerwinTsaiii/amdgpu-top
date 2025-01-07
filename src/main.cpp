#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <chrono>
#include <thread>
#include "layout.hpp"
#include <atomic>
#include <iostream>

using namespace ftxui;

int main() {
    try {
        auto screen = ScreenInteractive::Fullscreen();
        Layout layout;

        auto renderer = Renderer([&] {
            return layout.render() | flex;
        });

        auto component = Container::Vertical({
            renderer
        });

        // 創建更新線程
        std::atomic<bool> refresh_ui = true;
        std::thread refresh_thread([&] {
            while (refresh_ui) {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(1s);
                screen.Post([&] { screen.RequestAnimationFrame(); });
            }
        });

        // 主循環
        screen.Loop(component);
        
        // 清理
        refresh_ui = false;
        refresh_thread.join();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 
