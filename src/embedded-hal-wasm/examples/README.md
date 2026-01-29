
For HTTP client example, use the specific config file to increase the stack size:
```
cargo build --example hello_http_client --target wasm32-unknown-unknown --release --config .cargo/config-big-stack.toml
```