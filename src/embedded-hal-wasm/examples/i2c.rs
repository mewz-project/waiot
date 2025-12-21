#![no_std]
#![no_main]

use panic_halt as _;

use embedded_hal::delay::DelayNs;
use embedded_hal::i2c::{I2c, Operation};
use embedded_hal_wasm::delay::WasmDelay;
use embedded_hal_wasm::digital::WasmGpioPin;
use embedded_hal_wasm::i2c::WasmI2c;

use heapless::String;
use ufmt::uwrite;
use ufmt_float::uFmt_f32;

const I2C_PORT: i32 = 1;
const I2C_SDA_PIN: u32 = 32;
const I2C_SCL_PIN: u32 = 33;
const I2C_FREQ_HZ: i32 = 400_000;

const SHT3X_ADDR: u8 = 0x44;
const SHT3X_CMD_SINGLE_HIGH: &[u8] = &[0x24, 0x00];

#[link(wasm_import_module = "wasi:wasi_snapshot_preview1")]
unsafe extern "C" {
    fn fd_write(fd: i32, iov_ptr: i32, iov_cnt: i32, nwritten_ptr: i32) -> i32;
}

#[repr(C)]
struct Iovec {
    iov_base: i32, // pointer in linear memory
    iov_len: i32,  // length in bytes
}

// CRC8
fn sht_crc8(data: &[u8]) -> u8 {
    let mut crc = 0xFF;
    for &b in data {
        crc ^= b;
        for _ in 0..8 {
            if crc & 0x80 != 0 {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    crc
}

fn sht3x_read_single_shot(i2c: &mut WasmI2c) -> (f32, f32) {
    let mut rx = [0u8; 6];
    let mut operations = [
        Operation::Write(SHT3X_CMD_SINGLE_HIGH),
        Operation::Read(&mut rx),
    ];

    let result = i2c.transaction(SHT3X_ADDR, &mut operations);
    match result {
        Ok(_) => {
            if sht_crc8(&rx[0..2]) != rx[2] || sht_crc8(&rx[3..5]) != rx[5] {
                return (0.0, 0.0);
            }

            let st = ((rx[0] as u16) << 8) | (rx[1] as u16);
            let srh = ((rx[3] as u16) << 8) | (rx[4] as u16);
            let t = -45.0 + 175.0 * (st as f32) / 65535.0;
            let rh = 100.0 * (srh as f32) / 65535.0;
            (t, rh)
        }
        Err(_) => {
            return (0.0, 0.0);
        }
    }
}

fn write_trh(buf: &mut [u8], t: f32, rh: f32) -> usize {
    let mut s: String<64> = String::new();

    let tw = uFmt_f32::Two(t);
    let rhw = uFmt_f32::Two(rh);

    uwrite!(&mut s, "\"Temp\":{}, \"Humidity\":{}\n", tw, rhw).ok();

    let bytes = s.as_bytes();
    let n = core::cmp::min(buf.len(), bytes.len());
    buf[..n].copy_from_slice(&bytes[..n]);
    n
}

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    let sda_gpio = WasmGpioPin::new(I2C_SDA_PIN);
    let scl_gpio = WasmGpioPin::new(I2C_SCL_PIN);
    let config = embedded_hal_wasm::i2c::Config {
        port: I2C_PORT,
        sda_gpio,
        scl_gpio,
        freq_hz: I2C_FREQ_HZ,
    };
    let mut i2c = WasmI2c::new(config);

    let mut delay = WasmDelay;

    loop {
        let (t, rh) = sht3x_read_single_shot(&mut i2c);

        // Prepare iovec on stack
        let mut buf = [0u8; 64];
        let n = write_trh(&mut buf, t, rh);
        let iov = Iovec {
            iov_base: buf.as_ptr() as i32,
            iov_len: n as i32,
        };
        let mut nwritten: i32 = 0;

        unsafe {
            let _ = fd_write(
                1,
                (&iov as *const Iovec) as i32,
                1,
                (&mut nwritten) as *mut _ as i32,
            );
            delay.delay_ns(1_000_000_000); // 1s
        }
        delay.delay_ns(1_000_000_000); // 1s
    }
}
