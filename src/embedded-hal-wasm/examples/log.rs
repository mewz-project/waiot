#![no_std]
#![no_main]

use panic_halt as _;
use embedded_hal::delay::DelayNs;
use embedded_hal_wasm::delay::WasmDelay;

use embedded_hal_wasm::println;

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    println!("Logging from embedded-hal-wasm!");
    println!("We can log to the console using println! macro.");
    
    let mut delay = WasmDelay;
    for i in 0..2 {
        println!("Count: {}", i);
        delay.delay_ns(1_000_000_000); // 1 second
    }
}