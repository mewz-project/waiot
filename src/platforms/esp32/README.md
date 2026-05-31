# ESP32 platform

This ESP-IDF project provides the ESP32-specific platform layer for Waiot.

Core runtime logic lives in `../../core`; this directory owns the entry point,
ESP-IDF build configuration, Wi-Fi/NVS/Kubelet integration, HTTP server I/O,
WASI driver implementations, and platform log storage.
