#![no_std]
#![no_main]

use panic_halt as _;

use embedded_hal::delay::DelayNs;
use embedded_hal::digital::OutputPin as _;
use embedded_hal_wasm::delay::WasmDelay;
use embedded_hal_wasm::digital;

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    let mut pin = digital::WasmGpioPin::new(27).into_output();
    let mut delay = WasmDelay;

    let duration_ns = 300_000_000;

    for _ in 0..5 {
        let _ = pin.set_high();
        delay.delay_ns(duration_ns);
        let _ = pin.set_low();
        delay.delay_ns(duration_ns);
    }
    let _ = pin.set_low();
}
