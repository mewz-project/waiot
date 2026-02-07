#![no_std]
#![no_main]

use panic_halt as _;
use spin::Mutex;

use embedded_hal_wasm::println;
use embedded_hal_wasm::esp::{Camera, FrameSize, PixelFormat, CameraDeviceType};
use embedded_hal_wasm::http_client::{HttpClient, Method};
use embedded_hal_wasm::digital::WasmGpioPin;
use embedded_hal_wasm::i2c::WasmI2c;

// ======================
// Network
// ======================
const POST_URL: &str = "http://192.168.10.111:8000/upload";
const TIMEOUT_MS: i32 = 10_000;

// ヘッダ(12B) + 生RGB565
const RAW_HDR_SIZE: usize = 12;

// ======================
// Camera (QQVGA = 160x120 RGB565)
// ======================
const CAM_W: usize = 320;
const CAM_H: usize = 240;
const CAM_BPP: usize = 2;
const CAM_BUF_SIZE: usize = CAM_W * CAM_H * CAM_BPP;

const POST_MAX: usize = RAW_HDR_SIZE + CAM_BUF_SIZE;

static CAM_BUF: Mutex<[u8; CAM_BUF_SIZE]> = Mutex::new([0u8; CAM_BUF_SIZE]);
static POST_BUF: Mutex<[u8; POST_MAX]> = Mutex::new([0u8; POST_MAX]);

// ======================
// I2C
// ======================
const I2C_PORT: i32 = 1;
const I2C_SDA_PIN: u32 = 4;
const I2C_SCL_PIN: u32 = 5;
const I2C_FREQ_HZ: i32 = 400_000;

// ======================
// Upload
// ======================
fn post_raw_rgb565(url: &str, body: &[u8], contents_len: i32) -> Result<i32, ()> {
    let mut client = HttpClient::init(url, TIMEOUT_MS).map_err(|_| ())?;
    client.set_header("Content-Type", "application/x-rgb565").map_err(|_| ())?;
    client.set_header("X-Device", "waiot-esp").map_err(|_| ())?;
    client.open(Method::Post, contents_len).map_err(|_| ())?;

    let mut off = 0usize;
    while off < body.len() {
        let n = client.write(&body[off..]).map_err(|_| ())?;
        if n == 0 { return Err(()); }
        off += n;
    }
    let _ = client.fetch_headers();
    let status = client.status().map_err(|_| ())?;
    let _ = client.close();
    Ok(status)
}

// 送信用パケット: [ "R565"(4) | W(2) | H(2) | endian/reserved(4) | payload... ]
fn build_rgb565_packet(
    src_rgb565: &[u8; CAM_BUF_SIZE],
    out: &mut [u8; POST_MAX],
) -> usize {
    // magic
    out[0..4].copy_from_slice(b"R565");

    // width/height (big-endian in header)
    out[4] = ((CAM_W as u16) >> 8) as u8;
    out[5] = ((CAM_W as u16) & 0xFF) as u8;
    out[6] = ((CAM_H as u16) >> 8) as u8;
    out[7] = ((CAM_H as u16) & 0xFF) as u8;

    // endian flag (例: 0=BE, 1=LE) + reserved
    // ※カメラが吐くバイト並びに合わせて必要なら変更してください
    out[8]  = 0;
    out[9]  = 0;
    out[10] = 0;
    out[11] = 0;

    // payload (そのまま転送)
    out[RAW_HDR_SIZE..RAW_HDR_SIZE + CAM_BUF_SIZE].copy_from_slice(src_rgb565);
    RAW_HDR_SIZE + CAM_BUF_SIZE
}

// ======================
// main
// ======================
#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    // ---- I2C init (CoreS3設定に合わせて)
    // let sda_gpio = WasmGpioPin::new(I2C_SDA_PIN);
    // let scl_gpio = WasmGpioPin::new(I2C_SCL_PIN);
    // let config = embedded_hal_wasm::i2c::Config {
    //     port: I2C_PORT,
    //     sda_gpio,
    //     scl_gpio,
    //     freq_hz: I2C_FREQ_HZ,
    // };
    // let _ = WasmI2c::new(config);

    // ---- Camera init (QQVGA 160x120 RGB565)
    let camera = match Camera::init(
        CameraDeviceType::OV2640,
        PixelFormat::RGB565,
        FrameSize::SizeQVGA,
        12,
    ) {
        Ok(cam) => cam,
        Err(_) => return,
    };

    let mut cam_guard = CAM_BUF.lock();
    let cam_buf: &mut [u8; CAM_BUF_SIZE] = &mut *cam_guard;

    for _ in 0..100 {
        // ---- Capture 1 frame
        let n = match camera.get(cam_buf) {
            Ok(n) => n as usize,
            Err(_) => return,
        };
        if n < CAM_BUF_SIZE { return; }

        // ---- Build packet & POST upload
        let mut post_guard = POST_BUF.lock();
        let post_buf: &mut [u8; POST_MAX] = &mut *post_guard;

        let body_len = build_rgb565_packet(cam_buf, post_buf);

        match post_raw_rgb565(POST_URL, &post_buf[..body_len], body_len as i32) {
            Ok(status) => println!("POST status={}", status),
            Err(_) => println!("POST failed"),
        }

        // ---- Delay (必要なら追加)
        // let mut delay = WasmDelay;
        // delay.delay_ns(100_000_000); // 0.1s
    }
}
