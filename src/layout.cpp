#include "layout.hpp"
#include <string>
#include <algorithm>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <ftxui/component/screen_interactive.hpp>

using namespace ftxui;

Layout::Layout() {
    if (!gpu_stats.initialize()) {
        throw std::runtime_error("Failed to initialize AMD GPU monitoring");
    }
}

Element Layout::renderGPUUsage(const GPUDevice::Metrics& metrics) {
    return renderUsageBar("GPU Usage: ", metrics.gpu_usage, metrics.gpu_clock);
}

Element Layout::renderMemoryUsage(const GPUDevice::Metrics& metrics) {
    float memory_percent = (metrics.memory_used / metrics.memory_total) * 100.0f;
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

Element Layout::renderGPUBlock(const GPUDevice* device) {
    if (!device) return text("") | border;  // Empty block for invalid device

    auto metrics = device->getMetrics();
    
    return vbox({
        text(device->getMarketName()) | bold,
        renderGPUUsage(metrics),
        renderMemoryUsage(metrics),
        hbox({
            text(std::to_string(metrics.temperature) + "°C"),
            text(" | "),
            text(std::to_string(metrics.power_usage) + "W"),
            text(" | "),
            text(std::to_string(metrics.gpu_clock) + "/" + 
                 std::to_string(metrics.memory_clock) + " MHz")
        }) | center
    }) | border;
}

Element Layout::renderGPUGrid() {
    std::vector<Element> rows;
    size_t gpu_count = gpu_stats.getGPUCount();
    size_t row_count = (gpu_count + GRID_COLUMNS - 1) / GRID_COLUMNS;  // Ceiling division
    
    for (size_t row = 0; row < row_count; ++row) {
        std::vector<Element> gpu_blocks;
        
        for (size_t col = 0; col < GRID_COLUMNS; ++col) {
            size_t gpu_index = row * GRID_COLUMNS + col;
            if (gpu_index < gpu_count) {
                gpu_blocks.push_back(renderGPUBlock(gpu_stats.getGPU(gpu_index)));
            } else {
                gpu_blocks.push_back(text("") | border);  // Empty block for alignment
            }
        }
        
        rows.push_back(hbox(gpu_blocks));
    }
    
    return vbox(rows);
}

Element Layout::renderProcessTable() {
    std::vector<Element> rows;
    
    // Table header
    rows.push_back(
        hbox({
            text("GPU") | size(WIDTH, EQUAL, 8),
            text("PID") | size(WIDTH, EQUAL, 8),
            text("Process") | size(WIDTH, EQUAL, 20),
            text("GFX%") | size(WIDTH, EQUAL, 8),
            text("CMP%") | size(WIDTH, EQUAL, 8),
            text("ENC%") | size(WIDTH, EQUAL, 8),
            text("DEC%") | size(WIDTH, EQUAL, 8),
            text("VRAM") | size(WIDTH, EQUAL, 10)
        }) | bold
    );

    // Add separator after header
    rows.push_back(separator());

    // Collect all processes with their GPU index
    struct ProcessEntry {
        size_t gpu_index;
        ProcessInfo proc;
    };
    std::vector<ProcessEntry> all_processes;

    // Get processes for each GPU
    for (size_t i = 0; i < gpu_stats.getGPUCount(); ++i) {
        const GPUDevice* device = gpu_stats.getGPU(i);
        if (!device) continue;

        auto processes = device->getProcesses();
        for (const auto& proc : processes) {
            all_processes.push_back({i, proc});
        }
    }

    // Sort processes by GFX usage in descending order
    std::sort(all_processes.begin(), all_processes.end(),
        [](const ProcessEntry& a, const ProcessEntry& b) {
            return a.proc.gfx_usage > b.proc.gfx_usage;
        });

    // Add sorted processes to rows
    for (const auto& entry : all_processes) {
        const auto& proc = entry.proc;
        rows.push_back(
            hbox({
                text(std::to_string(entry.gpu_index)) | size(WIDTH, EQUAL, 8),
                text(std::to_string(proc.pid)) | size(WIDTH, EQUAL, 8),
                text(proc.name) | size(WIDTH, EQUAL, 20),
                text(proc.gfx_usage > 0 ? std::to_string((int)proc.gfx_usage) + "%" : "-") | size(WIDTH, EQUAL, 8),
                text(proc.compute_usage > 0 ? std::to_string((int)proc.compute_usage) + "%" : "-") | size(WIDTH, EQUAL, 8),
                text(proc.enc_usage > 0 ? std::to_string((int)proc.enc_usage) + "%" : "-") | size(WIDTH, EQUAL, 8),
                text(proc.dec_usage > 0 ? std::to_string((int)proc.dec_usage) + "%" : "-") | size(WIDTH, EQUAL, 8),
                text(std::to_string((int)(proc.memory_usage / (1024*1024))) + " MB") | size(WIDTH, EQUAL, 10)
            })
        );
    }

    return vbox({
        text("GPU Processes") | bold | center,
        separator(),
        vbox(rows)
    }) | border;
}

Element Layout::render() {
    return vbox({
        text("AMD GPU Monitor") | bold | center,
        separator(),
        renderGPUGrid(),
        separator(),
        renderProcessTable()
    }) | border;
} 