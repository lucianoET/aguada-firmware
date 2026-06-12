# ESP-IDF stub (VS Code extension helper)

This repository primarily builds firmware using PlatformIO (`firmware/*/platformio.ini`).

However, the ESP-IDF VS Code extension expects an ESP-IDF CMake project at the workspace root.
This minimal ESP-IDF application exists only to:

- allow `idf.py menuconfig` / `idf.py build` / `idf.py flash` commands to work from the repo root
- keep ESP-IDF toolchain configuration working inside VS Code

It does **not** replace the PlatformIO firmware.

If you only work with PlatformIO, you can ignore these files.
