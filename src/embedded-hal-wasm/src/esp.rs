#[link(wasm_import_module = "wasi:camera/camera")]
unsafe extern "C" {
    fn camera_init() -> i32;
    fn camera_get(buf_ptr: i32, buf_size: i32) -> i32;
}

#[repr(i32)]
#[derive(Copy, Clone)]
pub enum PixelFormat {
    RGB565 = 0,
    YUV422 = 1,
    YUV420 = 2,
    Grayscale = 3,
    JPEG = 4,
    RGB888 = 5,
    RAW = 6,
    RGB444 = 7,
    RGB555 = 8,
    RAW8 = 9,
}

#[repr(i32)]
#[derive(Copy, Clone)]
pub enum FrameSize {
    Size96x96 = 0, // 96x96
    SizeQQVGA = 1, // 160x120
    Size128x128 = 2, // 128x128
    SizeQCIF = 3, // 176x144
    SizeHQVGA = 4, // 240x176
    Size240x240 = 5, // 240x240
    SizeQVGA = 6, // 320x240
    Size320x320 = 7, // 320x320
    SizeCIF = 8, // 400x296
    SizeHVGA = 9, // 480x320
    SizeVGA = 10, // 640x480
    SizeSVGA = 11, // 800x600
    SizeXGA = 12, // 1024x768
    SizeHD = 13, // 1280x720
    SizeSXGA = 14, // 1280x1024
    SizeUXGA = 15, // 1600x1200
}


#[repr(i32)]
#[derive(Copy, Clone)]
pub enum CameraGrabMode {
    WhenEmpty = 0,
    Latest = 1,
}

pub type Result<T> = core::result::Result<T, i32>;

pub struct Camera;

impl Camera {
    pub fn init() -> Result<Self> {
        unsafe { 
            let ret = camera_init();
            if ret < 0 {
                return Err(ret);
            }
        }
        Ok(Self)
    }

    pub fn get(&self, buf: &mut [u8]) -> Result<i32> {
        unsafe { 
            let ret = camera_get(buf.as_mut_ptr() as i32, buf.len() as i32);
            if ret < 0 {
                return Err(ret);
            }
            Ok(ret)
        }
    }
}