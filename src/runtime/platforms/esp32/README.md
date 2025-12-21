# Getting started with ESP32

## Install ESP-IDF
In order to build the device agent including WAMR, you need to install ESP-IDF first.
Please follow the instructions in the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html) to set up the ESP-IDF development environment.

## Build and Flash
Copy the example NVS data file and edit it to set your WiFi SSID and password.
```
cd src/runtime/platforms/esp32
cp nvs_data.csv.example nvs_data.csv
# Edit nvs_data.csv to set your WiFi SSID and password
```

Configure the options using `menuconfig`.
```
idf.py menuconfig
-> Waiot Features
  -> Enable WASI-NN with TensorFlow Lite Micro  # Enable this option to use WASI-NN
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
