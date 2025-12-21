# Getting started with ESP32

## Install ESP-IDF
In order to build the device agent including WAMR, you need to install ESP-IDF first.
Please follow the instructions in the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html) to set up the ESP-IDF development environment.

## Build and Flash
Modify the WiFi SSID and password in `main/config.h`:
```
#define WIFI_SSID "hogehoge"
#define WIFI_PASS "hogepassword"
```

Then, connect your ESP32 device to your PC and run the following command.
This script will build the device agent and flash it to the device using ESP-IDF.
```
./build_and_run.sh ESP32
```

## Monitor Serial Output
You can get the IP address of the device from the serial output.
```
idf.py monitor
```

## HTTP request
```
# Hello
curl http://<IP_ADDRESS>/hello

# Run Wasm
curl -X POST http://<IP_ADDRESS>/create -H "Content-Type: application/octet-stream" --data-binary @<abs path>/src/embedded-hal-wasm/target/wasm32-unknown-unknown/release/examples/digital.wasm

# Stop
curl -X POST http://<IP_ADDRESS>/stop
```