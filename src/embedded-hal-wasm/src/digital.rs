use core::marker::PhantomData;
use embedded_hal::digital::{ErrorKind, InputPin, OutputPin, StatefulOutputPin};

#[link(wasm_import_module = "wasi:digital")]
unsafe extern "C" {
    // Set pin mode
    fn gpio_set_pin_mode(pin: i32, mode: i32) -> i32;

    // Output pin
    fn gpio_write(pin: i32, value: i32) -> i32;

    /// Input pin
    fn gpio_read(pin: i32) -> i32;
}

pub struct WasmGpioPin<M> {
    pub pin: i32,
    _marker: PhantomData<M>,
}

impl WasmGpioPin<()> {
    pub fn new(pin: u32) -> Self {
        if pin > i32::MAX as u32 {
            panic!("Pin number out of range");
        }
        Self {
            pin: pin as i32,
            _marker: PhantomData,
        }
    }

    pub fn into_input(self) -> WasmGpioPin<Input> {
        unsafe {
            let _ = gpio_set_pin_mode(self.pin, 0);
        }
        WasmGpioPin {
            pin: self.pin,
            _marker: PhantomData,
        }
    }

    pub fn into_output(self) -> WasmGpioPin<Output> {
        unsafe {
            let _ = gpio_set_pin_mode(self.pin, 1);
        }
        WasmGpioPin {
            pin: self.pin,
            _marker: PhantomData,
        }
    }
}

impl<M> embedded_hal::digital::ErrorType for WasmGpioPin<M> {
    type Error = ErrorKind;
}

#[derive(Debug, Clone, Copy)]
pub struct Input;
#[derive(Debug, Clone, Copy)]
pub struct Output;

#[inline]
fn return_err_code(code: i32) -> Result<(), ErrorKind> {
    match code {
        0 => Ok(()),
        _ => Err(ErrorKind::Other),
    }
}

impl OutputPin for WasmGpioPin<Output> {
    fn set_high(&mut self) -> Result<(), Self::Error> {
        let rc = unsafe { gpio_write(self.pin, 1) };
        return_err_code(rc)
    }

    fn set_low(&mut self) -> Result<(), Self::Error> {
        let rc = unsafe { gpio_write(self.pin, 0) };
        return_err_code(rc)
    }
}

impl StatefulOutputPin for WasmGpioPin<Output> {
    fn is_set_high(&mut self) -> Result<bool, Self::Error> {
        let rc = unsafe { gpio_read(self.pin) };
        match rc {
            0 => Ok(false),
            1 => Ok(true),
            _ => Err(ErrorKind::Other),
        }
    }

    fn is_set_low(&mut self) -> Result<bool, Self::Error> {
        let rc = unsafe { gpio_read(self.pin) };
        match rc {
            0 => Ok(true),
            1 => Ok(false),
            _ => Err(ErrorKind::Other),
        }
    }
}

impl InputPin for WasmGpioPin<Input> {
    fn is_high(&mut self) -> Result<bool, Self::Error> {
        let rc = unsafe { gpio_read(self.pin) };
        match rc {
            0 => Ok(false),
            1 => Ok(true),
            _ => Err(ErrorKind::Other),
        }
    }

    fn is_low(&mut self) -> Result<bool, Self::Error> {
        let rc = unsafe { gpio_read(self.pin) };
        match rc {
            0 => Ok(true),
            1 => Ok(false),
            _ => Err(ErrorKind::Other),
        }
    }
}
