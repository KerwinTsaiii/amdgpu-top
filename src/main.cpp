#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <chrono>
#include <thread>
#include "layout.hpp"

using namespace ftxui;

int main() {
    auto screen = ScreenInteractive::Fullscreen();
    Layout layout;

    auto renderer = Renderer([&] {
        return layout.render();  // 不再需要傳遞 stats
    });

    auto component = Container::Vertical({
        renderer
    });

    std::atomic<bool> refresh_ui = true;
    std::thread refresh_thread([&] {
        while (refresh_ui) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1s);
            screen.Post([&] {
                // 不需要手動獲取 stats，每個 GPU 塊會自己獲取
                screen.RequestAnimationFrame();
            });
        }
    });

    screen.Loop(component);
    refresh_ui = false;
    refresh_thread.join();

    return 0;
} 