cmake_minimum_required(VERSION 3.12)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

project(amdgpu-top)

# Find required packages
find_package(ftxui REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(AMDGPU REQUIRED libdrm_amdgpu)

# Create executable
add_executable(amdgpu-top 
    src/main.cpp
    src/layout.cpp
    src/gpu_stats.cpp
    src/process_info.cpp
    device_info/DeviceInfo.cpp
    device_info/DeviceInfoUtils.cpp
)

# Link libraries
target_link_libraries(amdgpu-top
    PRIVATE ftxui::screen
    PRIVATE ftxui::dom
    PRIVATE ftxui::component
    PRIVATE ${DRM_LIBRARIES}
    PRIVATE ${AMDGPU_LIBRARIES}
    PRIVATE dl
)

# Include directories
target_include_directories(amdgpu-top PRIVATE
    ${DRM_INCLUDE_DIRS}
    ${AMDGPU_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Enable C++17
target_compile_features(amdgpu-top PRIVATE cxx_std_17) 

# Add install target
install(TARGETS amdgpu-top DESTINATION bin)