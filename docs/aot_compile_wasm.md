# AoT Compile Wasm with WAMR

Use WAMR's `wamrc` compiler to convert a Wasm module into an AoT file.

Build `wamrc` by following the
[WAMR compiler documentation](https://github.com/bytecodealliance/wasm-micro-runtime/tree/main/wamr-compiler).

- For ESP targets:

```
./build/wamrc --target=xtensa  --target-abi=eabi -o example.aot example.wasm
```
