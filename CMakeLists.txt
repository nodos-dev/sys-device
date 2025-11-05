# Copyright MediaZ Teknoloji A.S. All Rights Reserved.
cmake_minimum_required(VERSION 3.24.2)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)

if (NOT WITH_NODOS_WORKSPACE)
    message(FATAL_ERROR "This repo currently does not support builds without Nodos workspace. "
    "Place this repo under nodos-workspace/Module folder and run cmake -S ./Toolchain/CMake -B Build from workspace root.")
endif()

add_compile_definitions(NOS_DISABLE_DEPRECATED)
nos_find_plugin_sdk("40.0.0" NOS_PLUGIN_SDK_TARGET NOS_SDK_DIR)

nos_generate_flatbuffers("${CMAKE_CURRENT_SOURCE_DIR}/Types" "${CMAKE_CURRENT_SOURCE_DIR}/Include/nosDeviceSubsystem" "cpp" "" nosDeviceSubsystem_generated)
set(DEPENDENCIES nosDeviceSubsystem_generated ${NOS_PLUGIN_SDK_TARGET})

nos_add_plugin("nosDeviceSubsystem" "${DEPENDENCIES}" "")

# Project generation
nos_group_targets("nosDeviceSubsystem" "NOS Subsystems")