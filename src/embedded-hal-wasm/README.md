# embedded-hal-wasm

`embedded-hal-wasm` implements [embedded-hal](https://github.com/rust-embedded/embedded-hal.git) traits for WebAssembly (WASM).
`embedded-hal` defines the Hardware Abstraction Layer (HAL) traits for embedded systems, and it is the de facto standard in the Rust embedded ecosystem.
Since standard Wasm runtimes do not provide access to hardware peripherals, `embedded-hal-wasm` converts `embedded-hal` API calls into corresponding Wasm function calls.
`waiot` provides the implementation of these Wasm functions as an extension of [WAMR](https://github.com/bytecodealliance/wasm-micro-runtime). You can find the implementation in the [runtime](../runtime) directory.
Using `embedded-hal-wasm` and `waiot`, you can write device applications with `embedded-hal` and run them in a Wasm environment.

```
cargo build --release --target wasm32-unknown-unknown --example digital
```