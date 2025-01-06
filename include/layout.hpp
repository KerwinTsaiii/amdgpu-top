#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include "gpu_stats.hpp"

class Layout {
public:
    Layout();
    ftxui::Element render();  // Default render
    ftxui::Element render(const GPUStats::Stats& stats);  // Render with provided stats
    GPUStats::Stats getStats() { return gpu_stats.getStats(); }  // Expose stats getter

private:
    GPUStats gpu_stats;
    
    ftxui::Element renderGPUUsage(const GPUStats::Stats& stats);
    ftxui::Element renderMemoryUsage(const GPUStats::Stats& stats);
    ftxui::Element renderUsageBar(const std::string& title, float value, uint32_t clock = 0);
    ftxui::Element renderInformation(const GPUStats::Stats& stats);
    ftxui::Element renderProcessTable(const GPUStats::Stats& stats);
}; 