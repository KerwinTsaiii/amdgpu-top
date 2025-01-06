#include "layout.hpp"
#include <string>

using namespace ftxui;

Layout::Layout() {
    if (!gpu_stats.initialize()) {
        throw std::runtime_error("Failed to initialize AMD GPU monitoring");
    }
}

Element Layout::renderGPUUsage(const GPUStats::Stats& stats) {
    return renderUsageBar("GPU Usage: ", stats.gpu_usage, stats.gpu_clock);
}

Element Layout::renderMemoryUsage(const GPUStats::Stats& stats) {
    float memory_percent = (stats.memory_used / stats.memory_total) * 100.0f;
    return renderUsageBar("Memory Usage: ", memory_percent);
}

Element Layout::renderUsageBar(const std::string& title, float value, uint32_t clock) {
    auto size = Terminal::Size();
    int bar_width = size.dimx - 4;
    float usage_fraction = value / 100.0f;
    int filled_width = static_cast<int>(usage_fraction * bar_width);
    
    std::string header;
    if (clock > 0) {
        header = title + std::to_string((int)value) + "% @ " + std::to_string(clock) + " MHz";
    } else {
        header = title + std::to_string((int)value) + "%";
    }

    return vbox({
        text(header),
        hbox({
            filled_width > 0 ?
                text(std::string(filled_width, ' ')) | bgcolor(Color::Yellow) :
                text(""),
            (bar_width - filled_width) > 0 ?
                text(std::string(bar_width - filled_width, ' ')) :
                text(""),
            text("  " + std::to_string((int)value) + "%")
        }) | border
    });
}

Element Layout::renderInformation(const GPUStats::Stats& stats) {
    std::vector<Element> info_elements;
    
    info_elements.push_back(text("GPU: " + std::string(gpu_stats.getMarketName())));
    
    info_elements.push_back(
        hbox({
            text("Temperature: " + std::to_string(stats.temperature) + "°C"),
            text(" | "),
            text("Power: " + std::to_string(stats.power_usage) + "W")
        })
    );

    info_elements.push_back(
        hbox({
            text("GPU Clock: " + std::to_string(stats.gpu_clock) + " MHz"),
            text(" | "),
            text("Memory Clock: " + std::to_string(stats.memory_clock) + " MHz")
        })
    );

    info_elements.push_back(
        text("VRAM: " + std::to_string((int)stats.memory_used) + " MB / " + 
             std::to_string((int)stats.memory_total) + " MB")
    );

    return vbox(info_elements) | border;
}

Element Layout::renderProcessTable(const GPUStats::Stats& stats) {
    std::vector<Element> rows;
    
    rows.push_back(
        hbox(std::vector<Element>{
            text("PID") | size(WIDTH, EQUAL, 8),
            text("Process") | size(WIDTH, EQUAL, 20),
            text("GPU%") | size(WIDTH, EQUAL, 8),
            text("Memory") | size(WIDTH, EQUAL, 10),
            text("Encode") | size(WIDTH, EQUAL, 8),
            text("Decode") | size(WIDTH, EQUAL, 8)
        }) | bold
    );

    for (const auto& proc : stats.processes) {
        rows.push_back(
            hbox(std::vector<Element>{
                text(std::to_string(proc.pid)) | size(WIDTH, EQUAL, 8),
                text(proc.name) | size(WIDTH, EQUAL, 20),
                text(std::to_string((int)proc.gpu_usage) + "%") | size(WIDTH, EQUAL, 8),
                text(std::to_string(proc.memory_usage / 1024 / 1024) + " MB") | size(WIDTH, EQUAL, 10),
                text(std::to_string(proc.enc_usage) + "%") | size(WIDTH, EQUAL, 8),
                text(std::to_string(proc.dec_usage) + "%") | size(WIDTH, EQUAL, 8)
            })
        );
    }

    return vbox({
        text("Processes") | bold | center,
        separator(),
        vbox(rows)
    }) | border;
}

Element Layout::render() {
    return render(gpu_stats.getStats());
}

Element Layout::render(const GPUStats::Stats& stats) {
    return vbox({
        text("AMD GPU Monitor") | bold | center,
        separator(),
        renderGPUUsage(stats),
        renderMemoryUsage(stats),
        separator(),
        text("Information") | bold | center,
        renderInformation(stats),
        separator(),
        renderProcessTable(stats)
    }) | border;
} 