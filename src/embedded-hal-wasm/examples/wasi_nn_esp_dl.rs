#![no_std]
#![no_main]

use panic_halt as _;
use spin::Mutex;
// use core::ptr::{addr_of_mut, write};
// mod wasi_nn_simple;
// use wasi_nn_simple::*;

#[link(wasm_import_module = "wasi_nn:esp_dl")]
unsafe extern "C" {
    pub fn load_simple(model_ptr_idx: u32, model_size: u32) -> i32;
    pub fn init_context_simple() -> i32;
    pub fn set_input_simple(input_ptr_idx: u32, input_size: u32) -> i32;
    pub fn compute_simple() -> i32;
    pub fn get_output_simple(index: u32, output_ptr_idx: u32, output_buff_max_size: u32) -> i32;
}

const INPUT_W: usize = 224;
const INPUT_H: usize = 224;
const INPUT_C: usize = 3;
const INPUT_SIZE: usize = INPUT_W * INPUT_H * INPUT_C;
const OUTPUT_MAX: usize = 25088;

static MODEL: &[u8] = include_bytes!("./pedestrian_detect_pico_s8_v1.espdl");
static IMAGE: &[u8] = include_bytes!("./pedestrian.jpg");

#[repr(align(16))]
struct Aligned<const N: usize>([u8; N]);

static INPUT: Mutex<Aligned<INPUT_SIZE>> = Mutex::new(Aligned([0; INPUT_SIZE]));
static OUTPUT: Mutex<Aligned<OUTPUT_MAX>> = Mutex::new(Aligned([0; OUTPUT_MAX]));
// static mut INPUT: Aligned<INPUT_SIZE> = Aligned([0; INPUT_SIZE]);
// static mut OUTPUT: [u8; OUTPUT_MAX] = [0; OUTPUT_MAX];

fn preprocess_into_rgb224(jpeg: &[u8], out_rgb: &mut [u8]) {
    let mut j = 0usize;
    for i in 0..out_rgb.len() {
        out_rgb[i] = jpeg[j];
        j += 1;
        if j >= jpeg.len() {
            j = 0;
        }
    }
}

// unsafe fn preprocess_into_rgb224_ptr(jpeg: &[u8], out_ptr: *mut u8, out_len: usize) {
//     let mut j = 0usize;
//     for i in 0..out_len {
//         // out_ptr[i] = jpeg[j] を生ポインタで
//         write(out_ptr.add(i), jpeg[j]);
//         j += 1;
//         if j >= jpeg.len() {
//             j = 0;
//         }
//     }
// }

fn postprocess(_out: &[u8]) {
}


#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    // load model
    // let rc = unsafe { load_simple(MODEL.as_ptr() as u32, MODEL.len() as u32) };
    // if rc != 0 {
        // return;
    // }

    // Init execution context
    let rc = unsafe { init_context_simple() };
    if rc != 0 {
        return;
    }


    // Set input
    let mut guard = INPUT.lock();
    let input_buff: &mut Aligned<INPUT_SIZE> = &mut *guard;
    preprocess_into_rgb224(IMAGE, input_buff.0.as_mut());

    let rc = unsafe { set_input_simple(input_buff.0.as_ptr() as u32, INPUT_SIZE as u32) };
    if rc != 0 {
        return;
    }

    // Compute
    let rc = unsafe { compute_simple() };
    if rc != 0 {
        return;
    }

    // Get output
    let mut guard_out = OUTPUT.lock();
    let output_buff: &mut Aligned<OUTPUT_MAX> = &mut *guard_out;
    for i in 0..6 {
        let rc = unsafe { get_output_simple(i, output_buff.0.as_mut_ptr() as u32, OUTPUT_MAX as u32) };
        if rc != 0 {
        return;
        }
    }

   
    // Set input
    // let input_ptr_u32: u32 = unsafe {
    //     let p: *mut u8 = (*addr_of_mut!(INPUT)).0.as_mut_ptr();
    //     preprocess_into_rgb224_ptr(IMAGE, p, INPUT_SIZE);
    //     p as u32
    // };
    // let rc = unsafe { set_input_simple(input_ptr_u32, INPUT_SIZE as u32) };
    // if rc != 0 {
    //     return;
    // }

}