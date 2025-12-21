#![no_std]
#![no_main]

use panic_halt as _;

use embedded_hal_wasm::wasi_nn::*;

use bytemuck::{cast_slice, cast_slice_mut};

fn as_bytes(data: &[f32]) -> &[u8] {
    cast_slice(data)
}

fn as_bytes_mut(data: &mut [f32]) -> &mut [u8] {
    cast_slice_mut(data)
}

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    let wasi_nn = WasiNN::new();

    static MODEL: &[u8] = include_bytes!("sum.tflite");
    let _ = wasi_nn.load_simple(MODEL, MODEL.len() as u32);

    let _ = wasi_nn.init_execution_context_simple();

    let input_storage: [f32; 25] = [1.0; 25];
    wasi_nn.set_input_simple(as_bytes(&input_storage), 4 * input_storage.len() as u32);
    wasi_nn.compute_simple();

    let mut output_storage: [f32; 1] = [0.0; 1];
    let _ = wasi_nn.get_output_simple(as_bytes_mut(&mut output_storage), 4);
}
