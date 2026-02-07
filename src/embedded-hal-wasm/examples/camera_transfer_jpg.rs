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
const POST_URL: &str = "http://192.168.50.88:8000/upload";
const TIMEOUT_MS: i32 = 10_000;

// ======================
// Camera
// ======================
// JPEGは可変長なので「最大」バッファを確保して、get() が返した n を送る
// QQVGA(160x120)なら 64KB もあれば大抵収まる想定（画質/内容次第で増えるので余裕を見て）
const JPEG_MAX: usize = 64 * 1024;
static CAM_BUF: Mutex<[u8; JPEG_MAX]> = Mutex::new([0u8; JPEG_MAX]);

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
fn post_jpeg(client: &mut HttpClient, body: &[u8], contents_len: i32) -> Result<i32, ()> {
    // keep-alive周りで詰まりやすい場合は "close" 推奨（安定優先）
    client.set_header("Connection", "close").map_err(|_| ())?;
    client.set_header("Content-Type", "image/jpeg").map_err(|_| ())?;
    client.set_header("X-Device", "waiot-esp").map_err(|_| ())?;

    client.open(Method::Post, contents_len).map_err(|_| ())?;

    // contents_len 分だけ送る（Content-Lengthと送信量の不一致を防ぐ）
    let total = contents_len as usize;
    let mut off = 0usize;
    while off < total {
        let n = client.write(&body[off..total]).map_err(|_| ())?;
        if n == 0 {
            return Err(());
        }
        off += n;
    }

    // ヘッダ取得
    let _ = client.fetch_headers();

    // 可能ならレスポンスボディも読み捨てしたいが、APIが無い前提でここでは省略
    // （Connection: close にしてるので、次のリクエストへの悪影響は起きにくい）

    let status = client.status().map_err(|_| ())?;
    Ok(status)
}

// ======================
// main
// ======================
#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    // ---- I2C init
    let sda_gpio = WasmGpioPin::new(I2C_SDA_PIN);
    let scl_gpio = WasmGpioPin::new(I2C_SCL_PIN);
    let config = embedded_hal_wasm::i2c::Config {
        port: I2C_PORT,
        sda_gpio,
        scl_gpio,
        freq_hz: I2C_FREQ_HZ,
    };
    // 生成して保持（Dropで無効化される実装に備える）
    let _ = WasmI2c::new(config);

    // ---- Camera init (JPEG + QQVGA)
    // 最後の引数(12)が画質/クロック/品質などのパラメータなら、JPEGのサイズや画質に影響します
    let camera = match Camera::init(
        CameraDeviceType::GC0308,
        PixelFormat::JPEG,
        FrameSize::SizeQQVGA,
        12,
    ) {
        Ok(cam) => cam,
        Err(_) => return,
    };

    let mut client = match HttpClient::init(POST_URL, TIMEOUT_MS) {
        Ok(client) => client,
        Err(_) => return,
    };

    for _ in 0..10000 {
        // ---- Capture 1 frame (JPEG)
        let mut cam_guard = CAM_BUF.lock();
        let cam_buf: &mut [u8; JPEG_MAX] = &mut *cam_guard;

        // 0クリアは必須ではない（n分だけ送るので）が、デバッグ時の混乱防止に残してもOK
        // for v in cam_buf.iter_mut() { *v = 0; }

        let n = match camera.get(cam_buf) {
            Ok(n) => n as usize,
            Err(_) => {
                println!("camera.get failed");
                continue;
            }
        };

        if n == 0 || n > JPEG_MAX {
            println!("bad jpeg size: {}", n);
            continue;
        }

        // 先頭がJPEGのSOI(FF D8)か軽く検査（任意）
        if n >= 2 && !(cam_buf[0] == 0xFF && cam_buf[1] == 0xD8) {
            println!("not jpeg? head={:02X} {:02X}", cam_buf[0], cam_buf[1]);
            // continue; // 厳密にやるなら
        }

        // ---- POST upload (JPEG bytes)
        match post_jpeg(&mut client, &cam_buf[..n], n as i32) {
            Ok(status) => println!("POST status={} jpeg_bytes={}", status, n),
            Err(_) => println!("POST failed"),
        }
    }

    let _ = client.close();
}
