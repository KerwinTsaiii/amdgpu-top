cmake_minimum_required(VERSION 3.12)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

project(amdgpu-top)

# Add debug build type
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel")

# Debug build settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG_BUILD)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    message(STATUS "Debug build enabled")
endif()

# Find required packages
find_package(ftxui REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DRM REQUIRED libdrm)
pkg_check_modules(AMDGPU REQUIRED libdrm_amdgpu)

# Find uthash
find_path(UTHASH_INCLUDE_DIR uthash.h
    PATHS
    /usr/include
    /usr/local/include
    ${CMAKE_SOURCE_DIR}/external
)

if(NOT UTHASH_INCLUDE_DIR)
    # Download uthash if not found
    message(STATUS "uthash.h not found in system, downloading...")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/troydhanson/uthash/master/src/uthash.h"
        "${CMAKE_SOURCE_DIR}/external/uthash.h"
        SHOW_PROGRESS
    )
    set(UTHASH_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external")
endif()

# Create external directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/external)

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
    ${UTHASH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Enable C++17
target_compile_features(amdgpu-top PRIVATE cxx_std_17) 

# Add install target
install(TARGETS amdgpu-top DESTINATION bin)