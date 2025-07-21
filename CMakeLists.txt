#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(MOCNordicDongle)


add_subdirectory(MOCNordic)

target_sources(app PRIVATE src/main.cpp)

target_include_directories(app PRIVATE 
    MOCNordic/include
)

target_link_libraries(app PRIVATE MOCNordic)
