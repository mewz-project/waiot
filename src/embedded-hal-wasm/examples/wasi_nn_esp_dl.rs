#![no_std]
#![no_main]

use panic_halt as _;
use spin::Mutex;
use libm::{expf, sqrtf};
use embedded_hal_wasm::println;

#[link(wasm_import_module = "wasi_nn:esp_dl")]
unsafe extern "C" {
    pub fn load_simple(model_ptr_idx: u32, model_size: u32) -> i32;
    pub fn init_context_simple() -> i32;
    pub fn set_input_simple(input_ptr_idx: u32, input_size: u32) -> i32;
    pub fn compute_simple() -> i32;
    pub fn get_output_simple(index: u32, output_ptr_idx: u32, output_buff_max_size: u32) -> i32;
}
static MODEL: &[u8] = include_bytes!("./pedestrian_detect_pico_s8_v1.espdl");
static IMAGE_RGB565_BE: &[u8] = include_bytes!("./people.rgb8");

const INPUT_W: usize = 224;
const INPUT_H: usize = 224;
const INPUT_C: usize = 3;
const INPUT_SIZE: usize = INPUT_W * INPUT_H * INPUT_C;

const S0_HW: usize = 28 * 28;
const S1_HW: usize = 14 * 14;
const S2_HW: usize = 7 * 7;

const SCORE0_SIZE: usize = S0_HW;         // int8
const BBOX0_SIZE: usize = S0_HW * 32;     // int8, 4 * (reg_max+1=8) = 32
const SCORE1_SIZE: usize = S1_HW;
const BBOX1_SIZE: usize = S1_HW * 32;
const SCORE2_SIZE: usize = S2_HW;
const BBOX2_SIZE: usize = S2_HW * 32;

const SCORE_EXP: i32 = -8;
const BOX_EXP: i32 = -3;

const SCORE_THR: f32 = 0.5;
const NMS_THR: f32 = 0.3;
const TOPK: usize = 10;

#[repr(align(16))]
struct Aligned<const N: usize>([u8; N]);

static INPUT: Mutex<Aligned<INPUT_SIZE>> = Mutex::new(Aligned([0; INPUT_SIZE]));

static OUT_SCORE0: Mutex<Aligned<SCORE0_SIZE>> = Mutex::new(Aligned([0; SCORE0_SIZE]));
static OUT_BBOX0: Mutex<Aligned<BBOX0_SIZE>> = Mutex::new(Aligned([0; BBOX0_SIZE]));
static OUT_SCORE1: Mutex<Aligned<SCORE1_SIZE>> = Mutex::new(Aligned([0; SCORE1_SIZE]));
static OUT_BBOX1: Mutex<Aligned<BBOX1_SIZE>> = Mutex::new(Aligned([0; BBOX1_SIZE]));
static OUT_SCORE2: Mutex<Aligned<SCORE2_SIZE>> = Mutex::new(Aligned([0; SCORE2_SIZE]));
static OUT_BBOX2: Mutex<Aligned<BBOX2_SIZE>> = Mutex::new(Aligned([0; BBOX2_SIZE]));

static DETS: Mutex<[Det; 256]> = Mutex::new([Det {
    cls: 0,
    score: 0.0,
    b: Box2i { x1: 0, y1: 0, x2: 0, y2: 0 },
}; 256]);

static KEEP: Mutex<[u8; 256]> = Mutex::new([0u8; 256]);

// -------------------------
// Quant helpers (ESP-DLの pow2 exponent 相当)
// -------------------------
#[inline]
fn scale_from_exp(exp: i32) -> f32 {
    // 2^exp
    if exp >= 0 {
        (1u32 << (exp as u32)) as f32
    } else {
        1.0f32 / ((1u32 << ((-exp) as u32)) as f32)
    }
}

#[inline]
fn dequant_i8(q: i8, exp: i32) -> f32 {
    (q as f32) * scale_from_exp(exp)
}


fn preprocess_rgb888_224_to_qint8(src: &[u8], dst: &mut [u8]) {
    for i in 0..dst.len() {
        // INPUT_EXP=1なら /2 の整数化が最速
        let x = src[i];
        let q = ((x as u16 + 1) >> 1) as i8; // round(x/2)
        dst[i] = q as u8;
    }
}

// -------------------------
// PicoDet postprocess (ESP-DL PicoPostprocessor 相当)
// -------------------------
#[derive(Copy, Clone)]
struct Box2i {
    x1: i32,
    y1: i32,
    x2: i32,
    y2: i32,
}

#[derive(Copy, Clone)]
struct Det {
    cls: i32,
    score: f32,
    b: Box2i,
}

#[inline]
fn iou(a: Box2i, b: Box2i) -> f32 {
    let ax1 = a.x1.max(0) as i32;
    let ay1 = a.y1.max(0) as i32;
    let ax2 = a.x2.max(0) as i32;
    let ay2 = a.y2.max(0) as i32;

    let bx1 = b.x1.max(0) as i32;
    let by1 = b.y1.max(0) as i32;
    let bx2 = b.x2.max(0) as i32;
    let by2 = b.y2.max(0) as i32;

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
    // softmax
    let mut m = logits[0];
    for i in 1..8 {
        if logits[i] > m { m = logits[i]; }
    }
    let mut exps = [0f32; 8];
    let mut sum = 0f32;
    for i in 0..8 {
        let e = expf(logits[i] - m);
        exps[i] = e;
        sum += e;
    }
    if sum <= 0.0 {
        return 0.0;
    }
    let inv = 1.0 / sum;
    let mut acc = 0f32;
    for i in 0..8 {
        acc += (i as f32) * (exps[i] * inv);
    }
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
    // C（カテゴリ数）: pedestrian_detect は通常 1 クラス
    let c = 1usize;

    let score_scale = scale_from_exp(SCORE_EXP);
    let box_scale = scale_from_exp(BOX_EXP);

    // ESP-DL: score_thr_quant = quantize(score_thr^2, 1/score_exp)
    //         -> score_q > score_thr_quant を判定
    // quantize: q = round(f / score_exp)
    // ここでは score_exp = scale_from_exp(SCORE_EXP)
    let score_thr_q = {
        let f = SCORE_THR * SCORE_THR;
        // let mut q = (f / score_scale).round() as i32;
        let mut q = (f / score_scale) as i32;
        if q > 127 { q = 127; }
        if q < -128 { q = -128; }
        q as i8
    };

    // inv_resize_scale は letterbox無しかつ crop無しなら 1.0
    // （ESP-DLは内部で scale を持つが、ここでは NN で 224へ直変換なので 1.0）
    let inv_resize_scale_x = 1.0f32;
    let inv_resize_scale_y = 1.0f32;

    let mut sp = 0usize;
    let mut bp = 0usize;

    for idx in 0..hw {
        // x,y
        let y = (idx / (hw / (INPUT_W / (stride as usize)))) as i32; // 乱暴なので後で安全に
        let x = (idx % (hw / (INPUT_W / (stride as usize)))) as i32;

        for cls in 0..c {
            let sq = score_q[sp] as i8;
            if sq > score_thr_q {
                let center_y = y * stride + offset;
                let center_x = x * stride + offset;

                // bbox 32個を dequant
                let mut box_f = [0f32; 32];
                for i in 0..32 {
                    let q = bbox_q[bp + i] as i8;
                    box_f[i] = (q as f32) * box_scale;
                }
                // 左/上/右/下 それぞれ 8bin
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

                // score は sqrt(dequant(score))
                let score = sqrtf(dequant_i8(sq, SCORE_EXP));

                let x1 = ((center_x as f32) - dl) * inv_resize_scale_x;
                let y1 = ((center_y as f32) - dt) * inv_resize_scale_y;
                let x2 = ((center_x as f32) + dr) * inv_resize_scale_x;
                let y2 = ((center_y as f32) + db) * inv_resize_scale_y;

                if *det_count < dets.len() {
                    dets[*det_count] = Det {
                        cls: cls as i32,
                        score,
                        b: Box2i {
                            x1: x1 as i32,
                            y1: y1 as i32,
                            x2: x2 as i32,
                            y2: y2 as i32,
                        },
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
    // sort by score desc (insertion sort; det_count<=256想定)
    let n = *det_count;
    for i in 1..n {
        let key = dets[i];
        let mut j = i;
        while j > 0 && dets[j - 1].score < key.score {
            dets[j] = dets[j - 1];
            j -= 1;
        }
        dets[j] = key;
    }

    // let mut keep = [true; 256];
    let mut keep_guard = KEEP.lock();
    let keep = &mut *keep_guard;
    for i in 0..n { keep[i] = 1; }
    for i in 0..n {
        if keep[i] == 0 { continue; }
        for j in (i + 1)..n {
            if keep[j] == 0 { continue; }
            if iou(dets[i].b, dets[j].b) > NMS_THR {
                keep[j] = 0;
            }
        }
    }

    // compact
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
) {
    // let mut dets = [Det { cls: 0, score: 0.0, b: Box2i { x1: 0, y1: 0, x2: 0, y2: 0 } }; 256];
    let mut dets_guard = DETS.lock();
    let dets: &mut [Det; 256] = &mut *dets_guard;
    let mut det_count = 0usize;

    // stage params: {{8,8,4,4},{16,16,8,8},{32,32,16,16}}
    // ここでは stride_y=stride_x=stride, offsetも同値として扱う
    parse_stage(dets, &mut det_count, score0, bbox0, S0_HW, 8, 4);
    parse_stage(dets, &mut det_count, score1, bbox1, S1_HW, 16, 8);
    parse_stage(dets, &mut det_count, score2, bbox2, S2_HW, 32, 16);

    nms_inplace(dets, &mut det_count);

    // no_std なので println はしない。
    // ここで dets[0..det_count] を、必要な共有メモリや出力バッファへ書き戻してください。
    // 例: 先頭の1件だけ別領域に書く、など。
    println!("Detections: {}", det_count);
    for i in 0..det_count {
        let d = &dets[i];
        println!("  Det {}: cls={}, score={:.3}, box=({}, {}, {}, {})",
            i, d.cls, d.score, d.b.x1, d.b.y1, d.b.x2, d.b.y2);
    }
    // let _ = (dets, det_count);
}



#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    // load model
    let rc = unsafe { load_simple(MODEL.as_ptr() as u32, MODEL.len() as u32) };
    if rc != 0 {
        return;
    }

    // Init execution context
    let rc = unsafe { init_context_simple() };
    if rc != 0 {
        return;
    }


    // preprocess -> INPUT
    {
        let mut guard = INPUT.lock();
        let input_buff: &mut Aligned<INPUT_SIZE> = &mut *guard;
        preprocess_rgb888_224_to_qint8(
            IMAGE_RGB565_BE,
            &mut input_buff.0,
        );
        
        let rc = unsafe { set_input_simple(input_buff.0.as_ptr() as u32, INPUT_SIZE as u32) };
        if rc != 0 { return; }
    }

    // Compute
    let rc = unsafe { compute_simple() };
    if rc != 0 {
        return;
    }

    // get outputs (index mapping assumed: score0,bbox0,score1,bbox1,score2,bbox2)
    {
        // index mapping from espdl log:
        // 0=bbox0, 1=bbox1, 2=bbox2, 3=score0, 4=score1, 5=score2

        let mut g_score0 = OUT_SCORE0.lock();
        let mut g_bbox0  = OUT_BBOX0.lock();
        let mut g_score1 = OUT_SCORE1.lock();
        let mut g_bbox1  = OUT_BBOX1.lock();
        let mut g_score2 = OUT_SCORE2.lock();
        let mut g_bbox2  = OUT_BBOX2.lock();

        let rc = unsafe { get_output_simple(0, g_bbox0.0.as_mut_ptr() as u32, BBOX0_SIZE as u32) }; // bbox0
        if rc != 0 { return; }
        let rc = unsafe { get_output_simple(1, g_bbox1.0.as_mut_ptr() as u32, BBOX1_SIZE as u32) }; // bbox1
        if rc != 0 { return; }
        let rc = unsafe { get_output_simple(2, g_bbox2.0.as_mut_ptr() as u32, BBOX2_SIZE as u32) }; // bbox2
        if rc != 0 { return; }

        let rc = unsafe { get_output_simple(3, g_score0.0.as_mut_ptr() as u32, SCORE0_SIZE as u32) }; // score0
        if rc != 0 { return; }
        let rc = unsafe { get_output_simple(4, g_score1.0.as_mut_ptr() as u32, SCORE1_SIZE as u32) }; // score1
        if rc != 0 { return; }
        let rc = unsafe { get_output_simple(5, g_score2.0.as_mut_ptr() as u32, SCORE2_SIZE as u32) }; // score2
        if rc != 0 { return; }

        // postprocess は (score0,bbox0, score1,bbox1, score2,bbox2) の順で渡す
        postprocess_pico(
            &g_score0.0, &g_bbox0.0,
            &g_score1.0, &g_bbox1.0,
            &g_score2.0, &g_bbox2.0,
        );        
    }
}