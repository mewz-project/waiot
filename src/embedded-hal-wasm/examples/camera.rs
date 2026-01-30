#![no_std]
#![no_main]

use panic_halt as _;
use spin::Mutex;

use embedded_hal_wasm::esp::{Camera, FrameSize, PixelFormat};
use embedded_hal_wasm::http_client::{HttpClient, Method};

use embedded_hal::delay::DelayNs;
use embedded_hal_wasm::delay::WasmDelay;

const BUF_SIZE: usize = 16 * 1024;
static BUFFER: Mutex<[u8; BUF_SIZE]> = Mutex::new([0; BUF_SIZE]);

const POST_URL: &str = "http://192.168.10.111:8000/upload";
const TIMEOUT_MS: i32 = 10_000;

fn post_jpeg(url: &str, jpeg: &[u8], contents_len: i32) -> Result<i32, ()> {
    let mut client = HttpClient::open(Method::Post, url, TIMEOUT_MS, contents_len).map_err(|_| ())?;

    client.set_header("Content-Type", "image/jpeg").map_err(|_| ())?;
    client.set_header("X-Device", "waiot-esp-eye").map_err(|_| ())?;

    let mut off = 0usize;
    while off < jpeg.len() {
        let n = client.write(&jpeg[off..]).map_err(|_| ())?;
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
    let camera = match Camera::init(PixelFormat::Grayscale, FrameSize::Size96x96, 30) {
        Ok(cam) => cam,
        Err(_) => {
            return;
        }
    }; 

    let mut delay = WasmDelay;
    let duration_ns = 1_000_000_000;

    for _ in 0..1 {
        let mut guard = BUFFER.lock();
        let buffer: &mut [u8] = &mut *guard;
        match camera.get(buffer) {
            Ok(n) =>  {
                // Here you can process the image data in `buffer`
                // n is the number of bytes written into buffer
                match post_jpeg(POST_URL, buffer, n) {
                    Ok(status) => {
                        // Successfully posted the image
                        // You can log the status if needed
                        if status == 200 {
                            // Posted successfully
                        } else {
                            // Handle non-200 status if needed
                        }
                    }
                    Err(_) => {
                        // Handle post error if needed
                    }
                }
            }
            Err(_) => {
                return;
            }
        };

        drop(guard);
        delay.delay_ns(duration_ns);
    }
}