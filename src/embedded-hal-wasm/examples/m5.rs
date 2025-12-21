#![no_std]
#![no_main]

use panic_halt as _;

use embedded_hal::delay::DelayNs;
use embedded_hal_wasm::delay::WasmDelay;
use embedded_hal_wasm::platforms::m5::*;

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    let m5 = M5::setup();
    m5.display.set_rotation(1);
    m5.display.fill_screen(TFTColor::TftBlue);
    m5.display.set_cursor(0, 0);
    m5.display
        .set_text_color(TFTColor::TftWhite, TFTColor::TftWhite);
    m5.display.set_text_size(2);
    m5.display.print_str("Hello, M5stick !!!\n");
    let mut ax = 0.0;
    let mut ay = 0.0;
    let mut az = 0.0;

    let mut delay = WasmDelay;

    loop {
        m5.imu.get_accel(&mut ax, &mut ay, &mut az);

        m5.display.fill_screen(TFTColor::TftBlue);
        m5.display.set_cursor(0, 20);
        m5.display.print_str("Accel\n");
        m5.display.print_float(ax);
        m5.display.print_str("\n");
        m5.display.print_float(ay);
        m5.display.print_str("\n");
        m5.display.print_float(az);
        m5.display.print_str("\n");
        delay.delay_ns(1_000_000_000);
    }
}
