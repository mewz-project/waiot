#[link(wasm_import_module = "wasi:m5")]
unsafe extern "C" {
    // ====================
    // SetUP
    // ====================
    fn m5_setup() -> i32;

    // ====================
    // Display
    // ====================
    // display size
    fn m5_lcd_width() -> i32;
    fn m5_lcd_height() -> i32;

    // display operations
    fn m5_lcd_set_rotation(r: i32) -> i32;
    fn m5_lcd_fill_screen(rgb565: u32) -> i32;
    fn m5_lcd_draw_pixel(x: i32, y: i32, rgb565: u32) -> i32;
    fn m5_lcd_draw_line(x0: i32, y0: i32, x1: i32, y1: i32, rgb565: u32) -> i32;
    fn m5_lcd_draw_rect(x: i32, y: i32, w: i32, h: i32, rgb565: u32) -> i32;
    fn m5_lcd_fill_rect(x: i32, y: i32, w: i32, h: i32, rgb565: u32) -> i32;
    fn m5_lcd_draw_circle(x0: i32, y0: i32, r: i32, rgb565: u32) -> i32;
    fn m5_lcd_fill_circle(x0: i32, y0: i32, r: i32, rgb565: u32) -> i32;

    fn m5_lcd_set_cursor(x: i32, y: i32) -> i32;
    fn m5_lcd_set_text_color(fg: u32, bg: u32) -> i32;
    fn m5_lcd_set_text_size(s: u32) -> i32;
    fn m5_lcd_print(ptr: u32, len: u32) -> i32;
    fn m5_lcd_print_float(ptr: u32) -> i32;

    // ====================
    // IMU
    // ====================
    fn m5_imu_is_enabled() -> i32;
    fn m5_imu_get_accel(ax_ptr: u32, ay_ptr: u32, az_ptr: u32) -> i32;
    fn m5_imu_get_gyro(gx_ptr: u32, gy_ptr: u32, gz_ptr: u32) -> i32;
}

pub struct M5 {
    pub display: Display,
    pub imu: Imu,
}

impl M5 {
    pub fn setup() -> Self {
        unsafe {
            let _ = m5_setup();
        }
        M5 {
            display: Display::new(),
            imu: Imu {},
        }
    }
}

pub enum TFTColor {
    TftBlack = 0x0000,
    TftNavy = 0x000F,
    TftDarkGreen = 0x03E0,
    TftDarkCyan = 0x03EF,
    TftMaroon = 0x7800,
    TftPurple = 0x780F,
    TftOlive = 0x7BE0,
    TftLightGrey = 0xD69A,
    TftDarkGrey = 0x7BEF,
    TftBlue = 0x001F,
    TftGreen = 0x07E0,
    TftCyan = 0x07FF,
    TftRed = 0xF800,
    TftMagenta = 0xF81F,
    TftYellow = 0xFFE0,
    TftWhite = 0xFFFF,
}

pub struct Display;

impl Display {
    pub fn new() -> Self {
        Display
    }

    pub fn width(&self) -> i32 {
        unsafe { m5_lcd_width() }
    }

    pub fn height(&self) -> i32 {
        unsafe { m5_lcd_height() }
    }

    pub fn set_rotation(&self, r: i32) {
        unsafe {
            let _ = m5_lcd_set_rotation(r);
        }
    }

    pub fn fill_screen(&self, colors: TFTColor) {
        unsafe {
            let _ = m5_lcd_fill_screen(colors as u32);
        }
    }

    pub fn draw_pixel(&self, x: i32, y: i32, colors: TFTColor) {
        unsafe {
            let _ = m5_lcd_draw_pixel(x, y, colors as u32);
        }
    }

    pub fn draw_line(&self, x0: i32, y0: i32, x1: i32, y1: i32, colors: TFTColor) {
        unsafe {
            let _ = m5_lcd_draw_line(x0, y0, x1, y1, colors as u32);
        }
    }

    pub fn draw_rect(&self, x: i32, y: i32, w: i32, h: i32, colors: TFTColor) {
        unsafe {
            let _ = m5_lcd_draw_rect(x, y, w, h, colors as u32);
        }
    }

    pub fn fill_rect(&self, x: i32, y: i32, w: i32, h: i32, colors: TFTColor) {
        unsafe {
            let _ = m5_lcd_fill_rect(x, y, w, h, colors as u32);
        }
    }

    pub fn draw_circle(&self, x0: i32, y0: i32, r: i32, colors: TFTColor) {
        unsafe {
            let _ = m5_lcd_draw_circle(x0, y0, r, colors as u32);
        }
    }

    pub fn fill_circle(&self, x0: i32, y0: i32, r: i32, colors: TFTColor) {
        unsafe {
            let _ = m5_lcd_fill_circle(x0, y0, r, colors as u32);
        }
    }

    pub fn set_cursor(&self, x: i32, y: i32) {
        unsafe {
            let _ = m5_lcd_set_cursor(x, y);
        }
    }

    pub fn set_text_color(&self, fg: TFTColor, bg: TFTColor) {
        unsafe {
            let _ = m5_lcd_set_text_color(fg as u32, bg as u32);
        }
    }

    pub fn set_text_size(&self, s: u32) {
        unsafe {
            let _ = m5_lcd_set_text_size(s);
        }
    }

    pub fn print_str(&self, string: &str) {
        let ptr = string.as_ptr() as u32;
        let len = string.len() as u32;
        unsafe {
            let _ = m5_lcd_print(ptr, len);
        }
    }

    pub fn print(&self, ptr: u32, len: u32) {
        unsafe {
            let _ = m5_lcd_print(ptr, len);
        }
    }

    pub fn print_float(&self, v: f32) {
        let v_ptr = &v as *const f32 as u32;
        unsafe {
            let _ = m5_lcd_print_float(v_ptr);
        }
    }
}

pub struct Imu;

impl Imu {
    pub fn is_enabled(&self) -> bool {
        unsafe { m5_imu_is_enabled() != 0 }
    }

    pub fn get_accel(&self, ax: &mut f32, ay: &mut f32, az: &mut f32) -> bool {
        let ax_ptr = ax as *mut f32 as u32;
        let ay_ptr = ay as *mut f32 as u32;
        let az_ptr = az as *mut f32 as u32;
        unsafe { m5_imu_get_accel(ax_ptr, ay_ptr, az_ptr) != 0 }
    }

    pub fn get_gyro(&self, gx: &mut f32, gy: &mut f32, gz: &mut f32) -> bool {
        let gx_ptr = gx as *mut f32 as u32;
        let gy_ptr = gy as *mut f32 as u32;
        let gz_ptr = gz as *mut f32 as u32;
        unsafe { m5_imu_get_gyro(gx_ptr, gy_ptr, gz_ptr) != 0 }
    }
}
