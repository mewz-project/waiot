#![no_std]
#![no_main]

use panic_halt as _;

use embedded_hal::delay::DelayNs;
use embedded_hal_wasm::delay::WasmDelay;

// Import our custom fd_write from host
#[link(wasm_import_module = "wasi:wasi_snapshot_preview1")]
unsafe extern "C" {
    fn fd_write(fd: i32, iov_ptr: i32, iov_cnt: i32, nwritten_ptr: i32) -> i32;
}

#[repr(C)]
struct Iovec {
    iov_base: i32, // pointer in linear memory
    iov_len: i32,  // length in bytes
}

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    let msg = b"hello\n";
    let mut delay = WasmDelay;

    loop {
        // Prepare iovec on stack
        let iov = Iovec {
            iov_base: msg.as_ptr() as i32,
            iov_len: msg.len() as i32,
        };

        let mut nwritten: i32 = 0;

        unsafe {
            let _ = fd_write(
                1,                             // fd: stdout
                (&iov as *const Iovec) as i32, // pointer to iovec array
                1,                             // number of iovecs
                (&mut nwritten as *mut i32) as i32,
            );
            delay.delay_ns(1_000_000_000); // 1s
        }
    }
}
