#pragma once

#include <cstdint>

// Simplified version of the device info structures
struct GDT_GfxCardInfo {
    uint32_t asic_id;
    uint32_t pci_rev_id;
    const char* name;
};

// Define the card database
static const GDT_GfxCardInfo amdgpu_device_info[] = {
    // NAVI 3x
    {0x744C, 0xC8, "AMD Radeon RX 7900 XTX"},
    {0x744C, 0xC9, "AMD Radeon RX 7900 XT"},
    {0x7470, 0xC9, "AMD Radeon RX 7800 XT"},
    {0x7470, 0xCF, "AMD Radeon RX 7700 XT"},
    
    // NAVI 2x
    {0x73A2, 0x00, "AMD Radeon RX 6950 XT"},
    {0x73A3, 0x00, "AMD Radeon RX 6900 XT"},
    {0x73BF, 0x00, "AMD Radeon RX 6800 XT"},
    {0x73BF, 0xC0, "AMD Radeon RX 6800"},
    {0x73DF, 0x00, "AMD Radeon RX 6750 XT"},
    {0x73DF, 0xC0, "AMD Radeon RX 6700 XT"},
    {0x73FF, 0x00, "AMD Radeon RX 6650 XT"},
    {0x73FF, 0xC1, "AMD Radeon RX 6600 XT"},
    {0x73FF, 0xC3, "AMD Radeon RX 6600"},
    
    // NAVI 1x
    {0x731F, 0x00, "AMD Radeon RX 5700 XT"},
    {0x731F, 0xC0, "AMD Radeon RX 5700"},
    {0x7341, 0x00, "AMD Radeon RX 5600 XT"},
    {0x7340, 0xC1, "AMD Radeon RX 5500 XT"},
    
    // VEGA
    {0x687F, 0xC0, "AMD Radeon VII"},
    {0x6863, 0x00, "AMD Radeon RX Vega 56"},
    {0x6867, 0x00, "AMD Radeon RX Vega 64"},
    
    // Add more GPUs as needed...
};

static const size_t amdgpu_device_info_size = sizeof(amdgpu_device_info) / sizeof(amdgpu_device_info[0]); 