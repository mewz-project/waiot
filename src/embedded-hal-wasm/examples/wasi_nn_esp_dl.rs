#![no_std]
#![no_main]

use panic_halt as _;

// mod wasi_nn_simple;
// use wasi_nn_simple::*;

#[link(wasm_import_module = "wasi_nn:esp_dl")]
unsafe extern "C" {
    pub fn esp_dl_load_simple(model_ptr_idx: u32, model_size: u32) -> i32;
    pub fn esp_dl_init_execution_context_simple() -> i32;
    pub fn esp_dl_set_input_simple(input_ptr_idx: u32, input_size: u32) -> i32;
    pub fn esp_dl_compute_simple() -> i32;
    pub fn esp_dl_get_output_simple(output_ptr_idx: u32, output_buff_max_size: u32) -> i32;
}


const INPUT_SIZE: usize = 64;
const OUTPUT_MAX: usize = 64;

static MODEL: &[u8] = include_bytes!("./pedestrian_detect_pico_s8_v1.espdl");

static mut INPUT: [u8; INPUT_SIZE] = [0; INPUT_SIZE];
static mut OUTPUT: [u8; OUTPUT_MAX] = [0; OUTPUT_MAX];

fn preprocess(_input: &mut [u8]) {
}

fn postprocess(_out: &[u8]) {
}


#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    // load model
    let rc = unsafe { esp_dl_load_simple(MODEL.as_ptr() as u32, MODEL.len() as u32) };
    if rc != 0 {
        return;
    }

    // init execution context
    // let rc = unsafe { esp_dl_init_execution_context_simple() };
    // if rc != 0 {
    //     return;
    // }

    // Preprocess input
    // unsafe {
    //     if INPUT_SIZE >= 4 {
    //         INPUT[0] = 0x11;
    //         INPUT[1] = 0x22;
    //         INPUT[2] = 0x33;
    //         INPUT[3] = 0x44;
    //     }
    // }

    // Set input
    // let rc = unsafe { set_input_simple(unsafe { INPUT.as_ptr() as u32 }, INPUT_SIZE as u32) };
    // if rc != 0 {
    //     return;
    // }

    // Run
    // let rc = unsafe { compute_simple() };
    // if rc != 0 {
    //     return;
    // }

    // Get output
    // let rc = unsafe { get_output_simple(unsafe { OUTPUT.as_mut_ptr() as u32 }, OUTPUT_MAX as u32) };
    // if rc != 0 {
    //     return;
    // }

    // Postprocess output
    // unsafe {
    //     postprocess(&OUTPUT);
    // }
}