cmake_minimum_required(VERSION 3.16)

# Minimal ESP-IDF app scaffold so the ESP-IDF VS Code extension can run
# menuconfig/build/flash in this repo without requiring a separate workspace.
# (Does not interfere with PlatformIO projects under firmware/.)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(aguada_idf_stub)
