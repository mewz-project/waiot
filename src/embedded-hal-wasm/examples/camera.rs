#![no_std]
#![no_main]

use panic_halt as _;
use spin::Mutex;

use embedded_hal_wasm::esp::{Camera, CameraDeviceType, FrameSize, PixelFormat};
use embedded_hal_wasm::http_client::{HttpClient, Method};
use embedded_hal_wasm::digital::WasmGpioPin;
use embedded_hal_wasm::i2c::WasmI2c;

use embedded_hal::delay::DelayNs;
use embedded_hal_wasm::delay::WasmDelay;

const WIDTH: usize = 240;
const HEIGHT: usize = 240;
const STRIDE_BYTES: usize = WIDTH * 2; // RGB565: 2 bytes/pixel

// RGB565 bytes = W*H*2 = 115,200
const BUF_SIZE: usize = WIDTH * HEIGHT * 2;
static BUFFER: Mutex<[u8; BUF_SIZE]> = Mutex::new([0; BUF_SIZE]);

const POST_URL: &str = "http://192.168.10.111:8000/upload";
const TIMEOUT_MS: i32 = 10_000;

fn post_rgb565(url: &str, rgb565: &[u8], content_len: i32) -> Result<i32, ()> {
    let mut client =
        HttpClient::init(url, TIMEOUT_MS).map_err(|_| ())?;
    client.open(Method::Post, content_len).map_err(|_| ())?;

    // Raw RGB565
    client
        .set_header("Content-Type", "application/x-rgb565")
        .map_err(|_| ())?;

    // Metadata for server reconstruction
    client.set_header("X-Device", "waiot-esp-eye").map_err(|_| ())?;
    client.set_header("X-Width", "240").map_err(|_| ())?;
    client.set_header("X-Height", "240").map_err(|_| ())?;
    client.set_header("X-Stride", "480").map_err(|_| ())?; // WIDTH*2
    client.set_header("X-Endian", "little").map_err(|_| ())?; // usually little-endian on ESP

    let mut off = 0usize;
    while off < rgb565.len() {
        let n = client.write(&rgb565[off..]).map_err(|_| ())?;
        if n == 0 {
            return Err(());
        }
        off += n;
    }

    let _ = client.fetch_headers();
    let status = client.status().map_err(|_| ())?;
    let _ = client.close();

    Ok(status)
}

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    // I2C init (legacy/new depends on your build; this just sets up the bus pins/port)
    const I2C_PORT: i32 = 1;
    const I2C_SDA_PIN: u32 = 12;
    const I2C_SCL_PIN: u32 = 11;
    const I2C_FREQ_HZ: i32 = 400_000;
    let sda_gpio = WasmGpioPin::new(I2C_SDA_PIN);
    let scl_gpio = WasmGpioPin::new(I2C_SCL_PIN);
    let config = embedded_hal_wasm::i2c::Config {
        port: I2C_PORT,
        sda_gpio,
        scl_gpio,
        freq_hz: I2C_FREQ_HZ,
    };
    let _ = WasmI2c::new(config);

    // Camera init: RGB565 240x240
    let camera = match Camera::init(CameraDeviceType::GC0308, PixelFormat::RGB565, FrameSize::Size240x240, 12) {
        Ok(cam) => cam,
        Err(_) => return,
    };

    let mut delay = WasmDelay;
    let duration_ns = 1_000_000_000;

    for _ in 0..1 {
        let mut guard = BUFFER.lock();
        let buffer: &mut [u8] = &mut *guard;

        match camera.get(buffer) {
            Ok(n) => {
                let n = n as usize;

                // We expect full frame for this mode. If your camera returns smaller chunks,
                // you can send only n bytes, but server expects stride*height.
                if n < BUF_SIZE {
                    // short frame: treat as failure for now
                    return;
                }

                let payload = &buffer[..BUF_SIZE];
                let _ = post_rgb565(POST_URL, payload, payload.len() as i32);
            }
            Err(_) => return,
        }

        drop(guard);
        delay.delay_ns(duration_ns);
    }
}
