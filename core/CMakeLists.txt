cmake_minimum_required(VERSION 3.20)
project(aegis_core VERSION 1.0.0 LANGUAGES CXX CUDA)

# ==============================================================================
# 1. CONFIGURATION
# ==============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Required for shared libs/plugins

# Output binaries to root/bin for easy running
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../lib)

# ==============================================================================
# 2. DEPENDENCIES
# ==============================================================================
# Internal Shared Definitions (The Bridge Protocol)
target_include_directories(aegis_shared INTERFACE
    ${CMAKE_SOURCE_DIR}/../shared/include
)

# External Libraries (via vcpkg or system)
find_package(spdlog REQUIRED)
find_package(CUDA REQUIRED)

find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)

# Proprietary Libraries (xInfer / xTorch)
# Ensure you have FindxInfer.cmake in ../cmake/modules/
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake/modules")
find_package(xInfer REQUIRED)

# ==============================================================================
# 3. THE CORE LIBRARY (Business Logic)
# ==============================================================================
# This library contains the logic but not the main() entry point.
# It allows unit tests to link against the logic without running the app.

add_library(aegis_flight_sw SHARED
    # --- Infrastructure ---
    src/platform/memory/CudaAllocator.cpp
    src/platform/threading/Scheduler.cpp

    # --- Bridge Drivers (The Sim Connection) ---
    src/drivers/bridge_client/ShmReader.cpp
    src/drivers/bridge_client/SimRadar.cpp
    src/drivers/bridge_client/SimCamera.cpp

    # --- Core Services (Placeholder for future) ---
    # src/services/fusion/FusionEngine.cpp
    # src/services/perception/InferenceMgr.cpp
)

# Include Paths for the Core
target_include_directories(aegis_flight_sw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../shared/include # Access to shm_layout.h
)

# Link Dependencies
target_link_libraries(aegis_flight_sw PUBLIC
    spdlog::spdlog
    CUDA::cudart
    xInfer::Runtime
)

# ==============================================================================
# 4. THE EXECUTABLE (The Binary)
# ==============================================================================
add_executable(aegis_core
    src/main/Main.cpp
)

target_link_libraries(aegis_core PRIVATE
    aegis_flight_sw
)

# ==============================================================================
# 5. INSTALLATION (Deploy to Jetson)
# ==============================================================================
install(TARGETS aegis_core aegis_flight_sw
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(DIRECTORY configs DESTINATION share/aegis)