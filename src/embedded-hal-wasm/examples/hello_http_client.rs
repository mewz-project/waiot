#![no_std]
#![no_main]

use panic_halt as _;
use embedded_hal::delay::DelayNs;
use embedded_hal::digital::OutputPin as _;
use embedded_hal_wasm::delay::WasmDelay;
use embedded_hal_wasm::digital;

#[link(wasm_import_module = "wasi_waiot:http_client")]
unsafe extern "C" {
    fn http_open(method: i32, url_ptr: i32, url_len: i32, timeout_ms: i32, content_len: i32) -> i32;
    // fn http_set_header(handle: i32, k_ptr: i32, k_len: i32, v_ptr: i32, v_len: i32) -> i32;
    fn http_fetch_headers(handle: i32) -> i32;
    // fn http_write(handle: i32, buf_ptr: i32, buf_len: i32) -> i32;
    fn http_read(handle: i32, buf_ptr: i32, buf_len: i32) -> i32;
    // fn http_status(handle: i32) -> i32;
    fn http_close(handle: i32) -> i32;
}

const BUF_SIZE: usize = 1024;
static mut BUFFER: [u8; BUF_SIZE] = [0; BUF_SIZE];

#[inline]
fn ptr_len(s: &str) -> (i32, i32) {
    (s.as_ptr() as i32, s.len() as i32)
}

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    let mut pin = digital::WasmGpioPin::new(27).into_output();
    let mut delay = WasmDelay;

    let duration_ns = 300_000_000;

    for _ in 0..2 {
        let _ = pin.set_high();
        delay.delay_ns(duration_ns);
        let _ = pin.set_low();
        delay.delay_ns(duration_ns);
    }
    let _ = pin.set_low();

    // HTTP GET
    let url = "http://example.com/";
    let (uptr, ulen) = ptr_len(url);
    let h = unsafe { http_open(0, uptr, ulen, 5_000, 0) };
    unsafe { let _ = http_fetch_headers(h); };

    // Read response
    for _ in 0..4 {
        unsafe { 
            let p = (&raw mut BUFFER) as *mut [u8; BUF_SIZE] as *mut u8;
            let n = http_read(h, p as i32, BUF_SIZE as i32);
            if n <= 0 {
                break;
            }
        };
    }
    
    // Close handle
    let _res = unsafe {http_close(h)};        
}