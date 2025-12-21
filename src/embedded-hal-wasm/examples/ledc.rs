#![no_std]
#![no_main]

use panic_halt as _;

use embedded_hal::delay::DelayNs;
use embedded_hal::pwm::SetDutyCycle;
use embedded_hal_wasm::delay::WasmDelay;
use embedded_hal_wasm::digital::WasmGpioPin;
use embedded_hal_wasm::pwm::{SpeedMode, WasmPwm};

const BUZZER_PIN: u32 = 2;
const LEDC_CHANNEL: u32 = 0;
const RES_BITS: u32 = 10;
const DEFAULT_VOL: u32 = 8;
const INIT_FREQ_HZ: u32 = 2000;

fn beep(pwm: &mut WasmPwm, freq: u32, duration_ns: u32, mut volume: u32, delay: &mut WasmDelay) {
    let max_duty = (1u32 << RES_BITS) - 1;

    if volume > max_duty {
        volume = max_duty;
    }

    if freq == 0 || volume == 0 {
        let _ = pwm.set_duty_cycle(0);
    } else {
        pwm.set_frequency(freq);
        let _ = pwm.set_duty_cycle(volume as u16);
    }

    delay.delay_ns(duration_ns);
}

fn buzzer_off(pwm: &mut WasmPwm) {
    let _ = pwm.set_duty_cycle(0);
}

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    let pin = WasmGpioPin::new(BUZZER_PIN);
    let mut pwm = WasmPwm::new(
        pin,
        LEDC_CHANNEL as u8,
        RES_BITS as u8,
        INIT_FREQ_HZ,
        SpeedMode::Low,
    );
    let mut delay = WasmDelay;

    const SCALE: [u32; 8] = [262, 294, 330, 349, 392, 440, 494, 523];

    loop {
        for &f in &SCALE {
            beep(&mut pwm, f, 200_000_000, DEFAULT_VOL, &mut delay);
            beep(&mut pwm, 0, 40_000_000, DEFAULT_VOL, &mut delay);
        }
        buzzer_off(&mut pwm);
        delay.delay_ns(300_000_000);
    }
}
