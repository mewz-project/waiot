#![no_std]
#![no_main]

use panic_halt as _;
use spin::Mutex;

use embedded_hal_wasm::esp::{Camera, FrameSize, PixelFormat, CameraDeviceType};
use embedded_hal_wasm::http_client::{HttpClient, Method};
use embedded_hal_wasm::digital::WasmGpioPin;
use embedded_hal_wasm::i2c::WasmI2c;
use embedded_hal::delay::DelayNs;
use embedded_hal_wasm::delay::WasmDelay;
// ======================
// Network
// ======================
const POST_URL: &str = "http://192.168.50.88:8000/upload";
const TIMEOUT_MS: i32 = 10_000;

// ヘッダ(12B) + 生RGB565
const RAW_HDR_SIZE: usize = 12;

// ======================
// Camera (QQVGA = 160x120 RGB565)
// ======================
const CAM_W: usize = 128;
const CAM_H: usize = 128;
const CAM_BPP: usize = 2;
const CAM_BUF_SIZE: usize = CAM_W * CAM_H * CAM_BPP;

static CAM_BUF: Mutex<[u8; CAM_BUF_SIZE]> = Mutex::new([0u8; CAM_BUF_SIZE]);

const HDR: [u8; RAW_HDR_SIZE] = {
    let w = CAM_W as u16;
    let h = CAM_H as u16;
    [
        b'R', b'5', b'6', b'5',
        (w >> 8) as u8, (w & 0xFF) as u8,
        (h >> 8) as u8, (h & 0xFF) as u8,
        0, 0, 0, 0
    ]
};


// ======================
// I2C
// ======================
const I2C_PORT: i32 = 1;
const I2C_SDA_PIN: u32 = 12;
const I2C_SCL_PIN: u32 = 11;
const I2C_FREQ_HZ: i32 = 400_000;

// ======================
// Upload
// ======================

fn write_all(client: &mut HttpClient, mut buf: &[u8]) -> Result<(), ()> {
    while !buf.is_empty() {
        let n = client.write(buf).map_err(|_| ())?;
        if n == 0 { return Err(()); }
        buf = &buf[n..];
    }
    Ok(())
}

fn post_rgb565_stream(
    client: &mut HttpClient,
    _url: &str,
    cam_buf: &[u8],
) -> Result<i32, ()> {
    let contents_len = (RAW_HDR_SIZE + cam_buf.len()) as i32;

    client.set_header("Connection", "keep-alive").map_err(|_| ())?;
    client.set_header("Content-Type", "application/x-rgb565").map_err(|_| ())?;
    client.set_header("X-Device", "waiot-esp").map_err(|_| ())?;
    client.open(Method::Post, contents_len).map_err(|_| ())?;

    // 12B header をスタック上に作る（小さいのでOK）
    // let mut hdr = [0u8; RAW_HDR_SIZE];
    // hdr[0..4].copy_from_slice(b"R565");
    // hdr[4] = ((CAM_W as u16) >> 8) as u8;
    // hdr[5] = ((CAM_W as u16) & 0xFF) as u8;
    // hdr[6] = ((CAM_H as u16) >> 8) as u8;
    // hdr[7] = ((CAM_H as u16) & 0xFF) as u8;
    // hdr[8] = 0; hdr[9] = 0; hdr[10] = 0; hdr[11] = 0;

    // ヘッダ → 画像 の順に送る（コピー不要）
    write_all(client, &HDR)?;
    write_all(client, cam_buf)?;

    // let _ = client.fetch_headers();
    Ok(client.status().map_err(|_| ())?)
}

// ======================
// main
// ======================
#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    // ---- I2C init (CoreS3設定に合わせて)
    let sda_gpio = WasmGpioPin::new(I2C_SDA_PIN);
    let scl_gpio = WasmGpioPin::new(I2C_SCL_PIN);
    let config = embedded_hal_wasm::i2c::Config {
        port: I2C_PORT,
        sda_gpio,
        scl_gpio,
        freq_hz: I2C_FREQ_HZ,
    };
    let _ = WasmI2c::new(config);

    // ---- Camera init (RGB565 240x240)
    // let camera = match Camera::init(CameraDeviceType::GC0308, PixelFormat::RGB565, FrameSize::SizeQQVGA, 12) {
    let camera = match Camera::init(CameraDeviceType::GC0308, PixelFormat::RGB565, FrameSize::Size128x128, 12) {
        Ok(cam) => cam,
        Err(_) => return,
    };

    let mut cam_guard = CAM_BUF.lock();
    let cam_buf: &mut [u8; CAM_BUF_SIZE] = &mut *cam_guard;

    // Initialize buffer before loop
    for v in cam_buf.iter_mut() {
        *v = 0;
    }

    let mut client = match HttpClient::init(POST_URL, TIMEOUT_MS) {
        Ok(client) => client,
        Err(_) => return,
    };
    let mut delay = WasmDelay;
    let duration_ns = 300_000_000;
    // let mut client = HttpClient::init(POST_URL, TIMEOUT_MS).unwrap();
    for _ in 0..10000 {
        // ---- Capture 1 frame
        let n = match camera.get(cam_buf) {
            Ok(n) => n as usize,
            Err(_) => return,
        };
        if n < CAM_BUF_SIZE { return; }

        match post_rgb565_stream(&mut client, POST_URL, cam_buf) {
            Ok(_) => {}
            Err(_) => {
                return;
            }
        }
        delay.delay_ns(duration_ns);
    }
    let _ = client.close();
}
