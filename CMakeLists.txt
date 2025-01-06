cmake_minimum_required(VERSION 3.11)
project(amd_monitor)

# Find required packages
find_package(ftxui REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(AMDGPU REQUIRED libdrm_amdgpu)

# Create executable
add_executable(amd_monitor 
    src/main.cpp
    src/layout.cpp
    src/gpu_stats.cpp
    device_info/DeviceInfo.cpp
    device_info/DeviceInfoUtils.cpp
)

# Link libraries
target_link_libraries(amd_monitor
    PRIVATE ftxui::screen
    PRIVATE ftxui::dom
    PRIVATE ftxui::component
    PRIVATE ${DRM_LIBRARIES}
    PRIVATE ${AMDGPU_LIBRARIES}
    PRIVATE dl
)

# Include directories
target_include_directories(amd_monitor PRIVATE
    ${DRM_INCLUDE_DIRS}
    ${AMDGPU_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Enable C++17
target_compile_features(amd_monitor PRIVATE cxx_std_17) 