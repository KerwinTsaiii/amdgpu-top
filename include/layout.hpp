#pragma once

#include <ftxui/dom/elements.hpp>
#include "gpu_stats.hpp"

class Layout {
public:
    Layout();
    ftxui::Element render();
    std::string getMetricsText() const;

private:
    GPUStats gpu_stats;
    
    // GPU Grid rendering
    ftxui::Element renderGPUGrid();
    ftxui::Element renderGPUBlock(const GPUDevice* device);
    
    // Individual components
    ftxui::Element renderGPUUsage(const GPUDevice::Metrics& metrics);
    ftxui::Element renderMemoryUsage(const GPUDevice::Metrics& metrics);
    ftxui::Element renderUsageBar(const std::string& title, float value, uint32_t clock = 0);
    ftxui::Element renderUsageBar(const std::string& title, float value, const std::string& details);
    ftxui::Element renderProcessTable();
    
    static constexpr size_t GRID_COLUMNS = 4;  // 4 columns for up to 8 GPUs
    
    // Text mode helpers
    std::string formatGPUMetrics(const GPUDevice* device) const;
    std::string formatProcessInfo(const std::vector<ProcessInfo>& processes) const;
}; 