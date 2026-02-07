#![no_std]
#![no_main]

use panic_halt as _;
use spin::Mutex;
use libm::{expf, sqrtf};

use embedded_hal_wasm::println;
use embedded_hal_wasm::esp::{Camera, FrameSize, PixelFormat, CameraDeviceType};
use embedded_hal_wasm::http_client::{HttpClient, Method};
use embedded_hal_wasm::digital::WasmGpioPin;
use embedded_hal_wasm::i2c::WasmI2c;
// use embedded_hal_wasm::delay::WasmDelay;
// use embedded_hal::delay::DelayNs;

#[link(wasm_import_module = "wasi_nn:esp_dl")]
unsafe extern "C" {
    pub fn load_simple(model_ptr_idx: u32, model_size: u32) -> i32;
    pub fn init_context_simple() -> i32;
    pub fn set_input_simple(input_ptr_idx: u32, input_size: u32) -> i32;
    pub fn compute_simple() -> i32;
    pub fn get_output_simple(index: u32, output_ptr_idx: u32, output_buff_max_size: u32) -> i32;
}

// ======================
// Network
// ======================
const POST_URL: &str = "http://192.168.10.111:8000/upload";
const TIMEOUT_MS: i32 = 10_000;

// ======================
// Camera (240x240 RGB565)
// ======================
// const CAM_W: usize = 128;
// const CAM_H: usize = 128;
const CAM_W: usize = 240;
const CAM_H: usize = 240;
const CAM_BPP: usize = 2;
const CAM_BUF_SIZE: usize = CAM_W * CAM_H * CAM_BPP;
static CAM_BUF: Mutex<[u8; CAM_BUF_SIZE]> = Mutex::new([0u8; CAM_BUF_SIZE]);

const I2C_PORT: i32 = 1;
const I2C_SDA_PIN: u32 = 12;
const I2C_SCL_PIN: u32 = 11;
const I2C_FREQ_HZ: i32 = 400_000;

// ======================
// Model / Input
// ======================
static MODEL: &[u8] = include_bytes!("./pedestrian_detect_pico_s8_v1.espdl");

const INPUT_W: usize = 224;
const INPUT_H: usize = 224;
const INPUT_C: usize = 3;
const INPUT_SIZE: usize = INPUT_W * INPUT_H * INPUT_C;

const SMALL_W: usize = CAM_W;
const SMALL_H: usize = CAM_H;
// const PAD: usize = (INPUT_W - SMALL_W) / 2;
const PAD: usize = 0;

#[repr(align(16))]
struct Aligned<const N: usize>([u8; N]);

// 推論入力 (qint8をu8で保持)
static INPUT: Mutex<Aligned<INPUT_SIZE>> = Mutex::new(Aligned([0u8; INPUT_SIZE]));

// 224x224 RGB888（描画＆PPM送信用）
const IMG_SIZE: usize = INPUT_W * INPUT_H * 3;
static RGB888_224: Mutex<Aligned<IMG_SIZE>> = Mutex::new(Aligned([0u8; IMG_SIZE]));

// ======================
// PicoDet outputs
// ======================
const S0_HW: usize = 28 * 28;
const S1_HW: usize = 14 * 14;
const S2_HW: usize = 7 * 7;

const SCORE0_SIZE: usize = S0_HW;         // int8
const BBOX0_SIZE: usize = S0_HW * 32;     // int8
const SCORE1_SIZE: usize = S1_HW;
const BBOX1_SIZE: usize = S1_HW * 32;
const SCORE2_SIZE: usize = S2_HW;
const BBOX2_SIZE: usize = S2_HW * 32;

static OUT_SCORE0: Mutex<Aligned<SCORE0_SIZE>> = Mutex::new(Aligned([0; SCORE0_SIZE]));
static OUT_BBOX0: Mutex<Aligned<BBOX0_SIZE>> = Mutex::new(Aligned([0; BBOX0_SIZE]));
static OUT_SCORE1: Mutex<Aligned<SCORE1_SIZE>> = Mutex::new(Aligned([0; SCORE1_SIZE]));
static OUT_BBOX1: Mutex<Aligned<BBOX1_SIZE>> = Mutex::new(Aligned([0; BBOX1_SIZE]));
static OUT_SCORE2: Mutex<Aligned<SCORE2_SIZE>> = Mutex::new(Aligned([0; SCORE2_SIZE]));
static OUT_BBOX2: Mutex<Aligned<BBOX2_SIZE>> = Mutex::new(Aligned([0; BBOX2_SIZE]));

// ======================
// Postprocess params
// ======================
const SCORE_EXP: i32 = -8;
const BOX_EXP: i32 = -3;

const SCORE_THR: f32 = 0.5;
const NMS_THR: f32 = 0.3;
const TOPK: usize = 10;

// ======================
// Upload PPM
// ======================
const PPM_MAX: usize = IMG_SIZE + 64;
static POST_BUF: Mutex<[u8; PPM_MAX]> = Mutex::new([0u8; PPM_MAX]);

fn post_ppm(url: &str, ppm: &[u8], contents_len: i32) -> Result<i32, ()> {
    let mut client = HttpClient::init(url, TIMEOUT_MS).map_err(|_| ())?;
    client.set_header("Content-Type", "image/x-portable-pixmap").map_err(|_| ())?;
    client.set_header("X-Device", "waiot-esp").map_err(|_| ())?;
    client.open(Method::Post, contents_len).map_err(|_| ())?;

    let mut off = 0usize;
    while off < ppm.len() {
        let n = client.write(&ppm[off..]).map_err(|_| ())?;
        if n == 0 { return Err(()); }
        off += n;
    }
    let _ = client.fetch_headers();
    let status = client.status().map_err(|_| ())?;
    let _ = client.close();
    Ok(status)
}

fn build_ppm_p6_224(src_rgb888: &[u8; IMG_SIZE], out: &mut [u8; PPM_MAX]) -> usize {
    let header = b"P6\n224 224\n255\n";
    let hlen = header.len();
    out[..hlen].copy_from_slice(header);
    out[hlen..hlen + IMG_SIZE].copy_from_slice(src_rgb888);
    hlen + IMG_SIZE
}

// ======================
// Image conversion: 240x240 RGB565(LE) -> 224x224 RGB888 center crop
// ======================
// #[inline]
// fn expand_5_to_8(v: u16) -> u8 {
//     // 0..31 -> 0..255
//     ((v * 255 + 15) / 31) as u8
// }
// #[inline]
// fn expand_6_to_8(v: u16) -> u8 {
//     // 0..63 -> 0..255
//     ((v * 255 + 31) / 63) as u8
// }
#[inline(always)]
fn expand_5_to_8(v: u16) -> u8 {
    let x = v as u8;          // 0..31
    (x << 3) | (x >> 2)       // 5bit -> 8bit
}

#[inline(always)]
fn expand_6_to_8(v: u16) -> u8 {
    let x = v as u8;          // 0..63
    (x << 2) | (x >> 4)       // 6bit -> 8bit
}

fn preprocess_rgb565be_240_to_rgb888_224_and_qint8(
    src: &[u8; CAM_BUF_SIZE],
    dst_rgb888: &mut [u8; IMG_SIZE],
    dst_qint8: &mut [u8; INPUT_SIZE],
) {
    // crop offset: (240-224)/2 = 8
    const OFF: usize = 8;

    for y in 0..INPUT_H {
        let mut si = ((y + OFF) * CAM_W + OFF) * 2;
        let mut di = (y * INPUT_W) * 3;
        for _ in 0..INPUT_W {
            let px = ((src[si] as u16) << 8) | (src[si + 1] as u16);
            si += 2;
            let r5 = (px >> 11) & 0x1F;
            let g6 = (px >> 5) & 0x3F;
            let b5 = px & 0x1F;

            let r = expand_5_to_8(r5);
            let g = expand_6_to_8(g6);
            let b = expand_5_to_8(b5);
            
            dst_rgb888[di + 0] = r;
            dst_rgb888[di + 1] = g;
            dst_rgb888[di + 2] = b;

            dst_qint8[di + 0] = r >> 1;
            dst_qint8[di + 1] = g >> 1;
            dst_qint8[di + 2] = b >> 1;
            di += 3;
        }
    }
}

fn preprocess_rgb565be_128_to_rgb888_244_center_and_qint8(
    src: &[u8; CAM_BUF_SIZE],
    dst_rgb888: &mut [u8; IMG_SIZE],
    dst_qint8: &mut [u8; INPUT_SIZE],
) {
    // 240→128 の center crop
    let src_off = (CAM_W - SMALL_W) / 2; // = 56
    
    for y in 0..SMALL_H {
        let sy = y + src_off;
        let mut si = (sy * CAM_W + src_off) * 2;

        let dy = y + PAD;
        let mut di = (dy * INPUT_W + PAD) * 3;

        for _ in 0..SMALL_W {
            let px = ((src[si] as u16) << 8) | (src[si + 1] as u16);
            si += 2;
            // let sx = x + src_off;

            // let si = (sy * CAM_W + sx) * 2;
            // let hi = src[si] as u16;
            // let lo = src[si + 1] as u16;
            // let px = (hi << 8) | lo; // ※既存コードに合わせてBE

            let r5 = (px >> 11) & 0x1F;
            let g6 = (px >> 5) & 0x3F;
            let b5 = px & 0x1F;

            let r = expand_5_to_8(r5);
            let g = expand_6_to_8(g6);
            let b = expand_5_to_8(b5);

            // ★224x224 中央に配置
            // let dx = x + PAD;
            // let dy = y + PAD;
            // let di = (dy * INPUT_W + dx) * 3;

            // dst_rgb888[di + 0] = r;
            // dst_rgb888[di + 1] = g;
            // dst_rgb888[di + 2] = b;

            // dst_qint8[di + 0] = (r >> 1) as u8;
            // dst_qint8[di + 1] = (g >> 1) as u8;
            // dst_qint8[di + 2] = (b >> 1) as u8;
            di += 3;
        }
    }
}


// ======================
// Drawing helpers
// ======================
#[inline]
fn clamp_i32(v: i32, lo: i32, hi: i32) -> i32 {
    if v < lo { lo } else if v > hi { hi } else { v }
}

fn put_px(img: &mut [u8; IMG_SIZE], x: i32, y: i32, r: u8, g: u8, b: u8) {
    if x < 0 || y < 0 || x >= INPUT_W as i32 || y >= INPUT_H as i32 { return; }
    let idx = ((y as usize) * INPUT_W + (x as usize)) * 3;
    img[idx + 0] = r;
    img[idx + 1] = g;
    img[idx + 2] = b;
}

fn draw_rect_rgb888(img: &mut [u8; IMG_SIZE], mut x1: i32, mut y1: i32, mut x2: i32, mut y2: i32) {
    x1 = clamp_i32(x1, 0, (INPUT_W as i32) - 1);
    y1 = clamp_i32(y1, 0, (INPUT_H as i32) - 1);
    x2 = clamp_i32(x2, 0, (INPUT_W as i32) - 1);
    y2 = clamp_i32(y2, 0, (INPUT_H as i32) - 1);
    if x2 < x1 { core::mem::swap(&mut x1, &mut x2); }
    if y2 < y1 { core::mem::swap(&mut y1, &mut y2); }

    let t = 2;
    let (r, g, b) = (255u8, 0u8, 0u8);

    for k in 0..t {
        let yy1 = y1 + k;
        let yy2 = y2 - k;
        let xx1 = x1 + k;
        let xx2 = x2 - k;

        for x in xx1..=xx2 {
            put_px(img, x, yy1, r, g, b);
            put_px(img, x, yy2, r, g, b);
        }
        for y in yy1..=yy2 {
            put_px(img, xx1, y, r, g, b);
            put_px(img, xx2, y, r, g, b);
        }
    }
}

// ======================
// Quant helpers
// ======================
#[inline]
fn scale_from_exp(exp: i32) -> f32 {
    if exp >= 0 { (1u32 << (exp as u32)) as f32 }
    else { 1.0f32 / ((1u32 << ((-exp) as u32)) as f32) }
}

#[inline]
fn dequant_i8(q: i8, exp: i32) -> f32 {
    (q as f32) * scale_from_exp(exp)
}

// ======================
// PicoDet postprocess
// ======================
#[derive(Copy, Clone)]
struct Box2i { x1: i32, y1: i32, x2: i32, y2: i32 }

#[derive(Copy, Clone)]
struct Det { cls: i32, score: f32, b: Box2i }

static DETS: Mutex<[Det; 256]> = Mutex::new([Det{
    cls: 0, score: 0.0, b: Box2i{x1:0,y1:0,x2:0,y2:0}
}; 256]);
static KEEP: Mutex<[u8; 256]> = Mutex::new([0u8; 256]);

#[inline]
fn iou(a: Box2i, b: Box2i) -> f32 {
    let ax1 = a.x1.max(0);
    let ay1 = a.y1.max(0);
    let ax2 = a.x2.max(0);
    let ay2 = a.y2.max(0);

    let bx1 = b.x1.max(0);
    let by1 = b.y1.max(0);
    let bx2 = b.x2.max(0);
    let by2 = b.y2.max(0);

    let inter_x1 = ax1.max(bx1);
    let inter_y1 = ay1.max(by1);
    let inter_x2 = ax2.min(bx2);
    let inter_y2 = ay2.min(by2);

    let iw = (inter_x2 - inter_x1).max(0) as f32;
    let ih = (inter_y2 - inter_y1).max(0) as f32;
    let inter = iw * ih;

    let area_a = ((ax2 - ax1).max(0) as f32) * ((ay2 - ay1).max(0) as f32);
    let area_b = ((bx2 - bx1).max(0) as f32) * ((by2 - by1).max(0) as f32);
    let uni = area_a + area_b - inter;
    if uni <= 0.0 { 0.0 } else { inter / uni }
}

fn dfl_integral_8(logits: &[f32; 8]) -> f32 {
    let mut m = logits[0];
    for i in 1..8 { if logits[i] > m { m = logits[i]; } }

    let mut exps = [0f32; 8];
    let mut sum = 0f32;
    for i in 0..8 {
        let e = expf(logits[i] - m);
        exps[i] = e;
        sum += e;
    }
    if sum <= 0.0 { return 0.0; }
    let inv = 1.0 / sum;
    let mut acc = 0f32;
    for i in 0..8 { acc += (i as f32) * (exps[i] * inv); }
    acc
}

fn parse_stage(
    dets: &mut [Det; 256],
    det_count: &mut usize,
    score_q: &[u8],
    bbox_q: &[u8],
    hw: usize,
    stride: i32,
    offset: i32,
) {
    let c = 1usize;

    let score_thr_q = {
        let score_scale = scale_from_exp(SCORE_EXP);
        let f = SCORE_THR * SCORE_THR;
        let mut q = (f / score_scale) as i32;
        if q > 127 { q = 127; }
        if q < -128 { q = -128; }
        q as i8
    };

    // ★安全な feature map サイズ
    let fm_w = (INPUT_W as i32) / stride;
    let _fm_h = (INPUT_H as i32) / stride;
    // hw は fm_w*fm_h のはず

    let box_scale = scale_from_exp(BOX_EXP);

    let mut sp = 0usize;
    let mut bp = 0usize;

    for idx in 0..hw {
        let x = (idx as i32) % fm_w;
        let y = (idx as i32) / fm_w;

        for cls in 0..c {
            let sq = score_q[sp] as i8;
            if sq > score_thr_q {
                let center_y = y * stride + offset;
                let center_x = x * stride + offset;

                let mut box_f = [0f32; 32];
                for i in 0..32 {
                    let q = bbox_q[bp + i] as i8;
                    box_f[i] = (q as f32) * box_scale;
                }

                let mut l = [0f32; 8];
                let mut t = [0f32; 8];
                let mut r = [0f32; 8];
                let mut b = [0f32; 8];
                for i in 0..8 {
                    l[i] = box_f[i];
                    t[i] = box_f[8 + i];
                    r[i] = box_f[16 + i];
                    b[i] = box_f[24 + i];
                }

                let dl = dfl_integral_8(&l) * (stride as f32);
                let dt = dfl_integral_8(&t) * (stride as f32);
                let dr = dfl_integral_8(&r) * (stride as f32);
                let db = dfl_integral_8(&b) * (stride as f32);

                let score = sqrtf(dequant_i8(sq, SCORE_EXP));

                let x1 = (center_x as f32) - dl;
                let y1 = (center_y as f32) - dt;
                let x2 = (center_x as f32) + dr;
                let y2 = (center_y as f32) + db;

                if *det_count < dets.len() {
                    dets[*det_count] = Det {
                        cls: cls as i32,
                        score,
                        b: Box2i { x1: x1 as i32, y1: y1 as i32, x2: x2 as i32, y2: y2 as i32 },
                    };
                    *det_count += 1;
                }
            }
            sp += 1;
        }
        bp += 32;
    }
}

fn nms_inplace(dets: &mut [Det], det_count: &mut usize) {
    let n = *det_count;

    // sort desc
    for i in 1..n {
        let key = dets[i];
        let mut j = i;
        while j > 0 && dets[j - 1].score < key.score {
            dets[j] = dets[j - 1];
            j -= 1;
        }
        dets[j] = key;
    }

    let mut keep_guard = KEEP.lock();
    let keep = &mut *keep_guard;
    for i in 0..n { keep[i] = 1; }

    for i in 0..n {
        if keep[i] == 0 { continue; }
        for j in (i + 1)..n {
            if keep[j] == 0 { continue; }
            if iou(dets[i].b, dets[j].b) > NMS_THR { keep[j] = 0; }
        }
    }

    let mut w = 0usize;
    for i in 0..n {
        if keep[i] != 0 {
            dets[w] = dets[i];
            w += 1;
            if w >= TOPK { break; }
        }
    }
    *det_count = w;
}

fn postprocess_pico(
    score0: &[u8], bbox0: &[u8],
    score1: &[u8], bbox1: &[u8],
    score2: &[u8], bbox2: &[u8],
) -> usize {
    let mut dets_guard = DETS.lock();
    let dets: &mut [Det; 256] = &mut *dets_guard;
    let mut det_count = 0usize;

    parse_stage(dets, &mut det_count, score0, bbox0, S0_HW, 8, 4);
    parse_stage(dets, &mut det_count, score1, bbox1, S1_HW, 16, 8);
    parse_stage(dets, &mut det_count, score2, bbox2, S2_HW, 32, 16);

    nms_inplace(dets, &mut det_count);

    println!("Detections: {}", det_count);
    for i in 0..det_count {
        let d = &dets[i];
        println!("  Det {}: cls={}, score={:.3}, box=({}, {}, {}, {})",
            i, d.cls, d.score, d.b.x1, d.b.y1, d.b.x2, d.b.y2);
    }
    det_count
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
    let camera = match Camera::init(CameraDeviceType::GC0308, PixelFormat::RGB565, FrameSize::Size240x240, 12) {
        Ok(cam) => cam,
        Err(_) => return,
    };

    // ---- Load model & init context
    let rc = unsafe { load_simple(MODEL.as_ptr() as u32, MODEL.len() as u32) };
    if rc != 0 { return; }
    let rc = unsafe { init_context_simple() };
    if rc != 0 { return; }

    // ---- Main loop
    let mut rgb_guard = RGB888_224.lock();
    let mut in_guard = INPUT.lock();
    let mut cam_guard = CAM_BUF.lock();
    let rgb888: &mut [u8; IMG_SIZE] = &mut rgb_guard.0;
    let input_q: &mut [u8; INPUT_SIZE] = &mut in_guard.0;
    let cam_buf: &mut [u8; CAM_BUF_SIZE] = &mut *cam_guard;

    for v in input_q.iter_mut() {
        *v = 0;
    }
    for _ in 0..100 {

        // ---- Capture 1 frame
        {
            for v in rgb888.iter_mut() {
                *v = 0;
            }


            let n = match camera.get(cam_buf) {
                Ok(n) => n as usize,
                Err(_) => return,
            };
            if n < CAM_BUF_SIZE { return; }

            // ---- Convert RGB565(240) -> RGB888(224 crop)
            {
                // ---- Preprocess RGB888 -> qint8 input
                preprocess_rgb565be_240_to_rgb888_224_and_qint8(cam_buf, rgb888, input_q);
                // preprocess_rgb565be_128_to_rgb888_244_center_and_qint8(cam_buf, rgb888, input_q);

                let rc = unsafe { set_input_simple(input_q.as_ptr() as u32, INPUT_SIZE as u32) };
                if rc != 0 { return; }
            }
        }

        // ---- Compute
        let rc = unsafe { compute_simple() };
        if rc != 0 { return; }

        // ---- Get outputs
        {
            let mut g_score0 = OUT_SCORE0.lock();
            let mut g_bbox0  = OUT_BBOX0.lock();
            let mut g_score1 = OUT_SCORE1.lock();
            let mut g_bbox1  = OUT_BBOX1.lock();
            let mut g_score2 = OUT_SCORE2.lock();
            let mut g_bbox2  = OUT_BBOX2.lock();

            // index mapping:
            // 0=bbox0, 1=bbox1, 2=bbox2, 3=score0, 4=score1, 5=score2
            if unsafe { get_output_simple(0, g_bbox0.0.as_mut_ptr() as u32, BBOX0_SIZE as u32) } != 0 { return; }
            if unsafe { get_output_simple(1, g_bbox1.0.as_mut_ptr() as u32, BBOX1_SIZE as u32) } != 0 { return; }
            if unsafe { get_output_simple(2, g_bbox2.0.as_mut_ptr() as u32, BBOX2_SIZE as u32) } != 0 { return; }
            if unsafe { get_output_simple(3, g_score0.0.as_mut_ptr() as u32, SCORE0_SIZE as u32) } != 0 { return; }
            if unsafe { get_output_simple(4, g_score1.0.as_mut_ptr() as u32, SCORE1_SIZE as u32) } != 0 { return; }
            if unsafe { get_output_simple(5, g_score2.0.as_mut_ptr() as u32, SCORE2_SIZE as u32) } != 0 { return; }

            let det_count = postprocess_pico(
                &g_score0.0, &g_bbox0.0,
                &g_score1.0, &g_bbox1.0,
                &g_score2.0, &g_bbox2.0,
            );

            // ---- Draw & upload PPM
            let dets_guard = DETS.lock();
            let dets = &*dets_guard;

            for i in 0..det_count {
                let b = dets[i].b;
                draw_rect_rgb888(rgb888, b.x1, b.y1, b.x2, b.y2);
            }

            let mut post_guard = POST_BUF.lock();
            let post_buf: &mut [u8; PPM_MAX] = &mut *post_guard;

            // ---- Build PPM
            let n = build_ppm_p6_224(rgb888, post_buf);

            // ---- POST upload
            match post_ppm(POST_URL, &post_buf[..n], n as i32) {
                Ok(status) => println!("POST status={}", status),
                Err(_) => println!("POST failed"),
            }
        }

        // ---- Delay
        // let mut delay = WasmDelay;
        // delay.delay_ns(100_000_000); // 0.1s
    }
}