#![no_std]
#![no_main]

use panic_halt as _;

use embedded_hal_wasm::esp::Camera;

const BUF_SIZE: usize = 48 * 1024;

#[link(wasm_import_module = "wasi:clock/mono")]
unsafe extern "C" {
    fn sleep(ns: i64) -> i32;
}

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    let camera = Camera::init().unwrap();   
    let mut buffer = [0u8; BUF_SIZE];

    loop {
        match camera.get(&mut buffer) {
            Ok(_n) =>  {
                // Here you can process the image data in `buffer`
                // n is the number of bytes written into buffer
            }
            Err(_) => {
                return;
            }
        };

        unsafe {
            sleep(1_000_000_000);
        }
    }
}