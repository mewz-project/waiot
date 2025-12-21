use embedded_hal::i2c::{Error, ErrorKind, ErrorType, I2c, NoAcknowledgeSource, Operation};

use crate::digital::WasmGpioPin;

#[link(wasm_import_module = "wasi:i2c")]
unsafe extern "C" {
    fn i2c_param_config(port: i32, sda_gpio: i32, scl_gpio: i32, freq_hz: i32) -> i32;
    fn i2c_driver_install(port: i32) -> i32;
    fn i2c_master_write(
        port: i32,
        addr: i32,
        write_buff_ptr_idx: i32,
        write_size: i32,
        ticks_to_wait: i32,
    ) -> i32;
    fn i2c_master_read(
        port: i32,
        addr: i32,
        read_buff_ptr_idx: i32,
        read_size: i32,
        ticks_to_wait: i32,
    ) -> i32;
}

pub struct Config {
    pub port: i32,
    pub freq_hz: i32,
    pub sda_gpio: WasmGpioPin<()>,
    pub scl_gpio: WasmGpioPin<()>,
}

pub struct WasmI2c {
    config: Config,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum I2cError {
    Bus,
    ArbLost,
    NackAddr,
    NackData,
    Overrun,
    Other,
}

impl Error for I2cError {
    fn kind(&self) -> ErrorKind {
        match *self {
            I2cError::Bus => ErrorKind::Bus,
            I2cError::ArbLost => ErrorKind::ArbitrationLoss,
            I2cError::NackAddr => ErrorKind::NoAcknowledge(NoAcknowledgeSource::Address),
            I2cError::NackData => ErrorKind::NoAcknowledge(NoAcknowledgeSource::Data),
            I2cError::Overrun => ErrorKind::Overrun,
            I2cError::Other => ErrorKind::Other,
        }
    }
}

impl ErrorType for WasmI2c {
    type Error = I2cError;
}

impl WasmI2c {
    pub fn new(config: Config) -> Self {
        unsafe {
            i2c_param_config(
                config.port,
                config.sda_gpio.pin,
                config.scl_gpio.pin,
                config.freq_hz,
            );
            i2c_driver_install(config.port);
        }
        WasmI2c { config }
    }
}

impl I2c for WasmI2c {
    fn transaction(&mut self, address: u8, ops: &mut [Operation<'_>]) -> Result<(), Self::Error> {
        let addr_i32 = address as i32;
        for operation in ops.iter_mut() {
            match operation {
                Operation::Read(buf) => {
                    let ptr_i32 = buf.as_mut_ptr() as usize as i32;
                    let rc = unsafe {
                        i2c_master_read(
                            self.config.port,
                            addr_i32,
                            ptr_i32,
                            buf.len() as i32,
                            100_000_000,
                        )
                    };
                    if rc != 0 {
                        return Err(I2cError::Other);
                    }
                }
                Operation::Write(buf) => {
                    let ptr_i32 = buf.as_ptr() as usize as i32;
                    let rc = unsafe {
                        i2c_master_write(
                            self.config.port,
                            addr_i32,
                            ptr_i32,
                            buf.len() as i32,
                            100_000_000,
                        )
                    };
                    if rc != 0 {
                        return Err(I2cError::Other);
                    }
                }
            }
        }
        Ok(())
    }
}
