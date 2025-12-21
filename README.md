# waiot: Bring WebAssembly to the IoT devices (under development)

waiot enables running WebAssembly (Wasm) workloads on IoT devices.

# Repository Structure
- `src/embedded-hal-wasm`: A crate that provides [embedded-hal](https://github.com/rust-embedded/embedded-hal.git) implementation for Wasm
- `src/runtime`: The runtime that runs Wasm workloads on IoT devices using [WAMR](https://github.com/bytecodealliance/wasm-micro-runtime.git)

# Getting Started
## Writing Application
```
cd src/embedded-hal-wasm
cargo build --example digital --target wasm32-wasi --release
```

## Run Wasm on IoT Devices
- [ESP32](src/runtime/platforms/esp32/README.md)

# License
waiot is licensed under the MIT License. For the full license text, please refer to the LICENSE file.