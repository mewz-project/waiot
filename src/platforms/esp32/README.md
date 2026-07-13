# ESP32 platform

This ESP-IDF project provides the ESP32-specific platform layer for Waiot.

Core runtime logic lives in `../../core`; this directory owns the entry point,
ESP-IDF build configuration, Wi-Fi/NVS/Kubelet integration, HTTP server I/O,
WASI driver implementations, and platform log storage.

# Getting Started

Install and activate [ESP-IDF](https://github.com/espressif/esp-idf).

Set the target chip for your device.

```
idf.py set-target esp32
```

Create the NVS configuration file and update the values for your device and
network.

```
cp nvs_data.csv.example nvs_data.csv

# Edit at least these entries:
sta_ssid,data,string,example-sta-ssid
sta_pass,data,string,example-sta-password
```

Build and flash the project.

```
idf.py build
idf.py flash
```

Use the serial monitor to view logs. You can exit the monitor with `Ctrl` + `]`.

```
idf.py monitor
```