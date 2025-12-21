use crate::digital::WasmGpioPin;
use core::convert::Infallible;
use embedded_hal::pwm::{ErrorType, SetDutyCycle};

#[link(wasm_import_module = "wasi:pwm")]
unsafe extern "C" {
    fn pwm_init(pin: i32, channel: i32, frequency: i32, resolution: i32, speed_mode: i32) -> i32;
    fn pwm_set_duty(channel: i32, duty_cycle: i32, speed_mode: i32) -> i32;
    fn pwm_update_duty(channel: i32, speed_mode: i32) -> i32;
    fn pwm_set_frequency(channel: i32, frequency: i32, speed_mode: i32) -> i32;
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SpeedMode {
    High = 0,
    Low = 1,
}

pub struct WasmPwm {
    pub pin: WasmGpioPin<()>,
    pub channel: u8,
    pub resolution: u8,
    pub frequency: u32,
    pub speed_mode: SpeedMode,
}

impl WasmPwm {
    pub fn new(
        pin: WasmGpioPin<()>,
        channel: u8,
        resolution: u8,
        frequency: u32,
        speed_mode: SpeedMode,
    ) -> Self {
        unsafe {
            let _ = pwm_init(
                pin.pin,
                channel as i32,
                frequency as i32,
                resolution as i32,
                speed_mode as i32,
            );
        }
        Self {
            pin,
            channel,
            frequency,
            resolution,
            speed_mode,
        }
    }

    pub fn set_frequency(&mut self, frequency: u32) {
        unsafe {
            let _ = pwm_set_frequency(
                self.channel as i32,
                frequency as i32,
                self.speed_mode as i32,
            );
        }
        self.frequency = frequency;
    }
}

impl ErrorType for WasmPwm {
    type Error = Infallible;
}

impl SetDutyCycle for WasmPwm {
    fn max_duty_cycle(&self) -> u16 {
        if self.resolution >= 16 {
            return u16::MAX;
        }
        (1 << self.resolution) - 1
    }

    fn set_duty_cycle(&mut self, duty: u16) -> Result<(), Self::Error> {
        unsafe {
            let _ = pwm_set_duty(self.channel as i32, duty as i32, self.speed_mode as i32);
            let _ = pwm_update_duty(self.channel as i32, self.speed_mode as i32);
        }
        Ok(())
    }
}
