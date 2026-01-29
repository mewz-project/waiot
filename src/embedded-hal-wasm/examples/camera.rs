#![no_std]
#![no_main]

use panic_halt as _;
use spin::Mutex;

use embedded_hal_wasm::esp::Camera;
use embedded_hal_wasm::http_client::{HttpClient, Method};

use embedded_hal::delay::DelayNs;
use embedded_hal_wasm::delay::WasmDelay;

const BUF_SIZE: usize = 10 * 1024;
static BUFFER: Mutex<[u8; BUF_SIZE]> = Mutex::new([0; BUF_SIZE]);


#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    let camera = match Camera::init() {
        Ok(cam) => cam,
        Err(_) => {
            return;
        }
    }; 

    let mut delay = WasmDelay;
    let duration_ns = 1_000_000_000;

    for _ in 0..5 {
        let mut guard = BUFFER.lock();
        let buffer: &mut [u8] = &mut *guard;
        match camera.get(buffer) {
            Ok(_n) =>  {
                // Here you can process the image data in `buffer`
                // n is the number of bytes written into buffer
            }
            Err(_) => {
                return;
            }
        };
        drop(guard);
        delay.delay_ns(duration_ns);
    }
}