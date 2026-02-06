#[link(wasm_import_module = "wasi_waiot:camera")]
unsafe extern "C" {
    fn camera_init(device_type: i32, pixel_format: i32, frame_size: i32, jpeg_quality: i32) -> i32;
    fn camera_get(buf_ptr: i32, buf_size: i32) -> i32;
    fn if_camera_config_changed(pixel_format: i32, frame_size: i32, jpeg_quality: i32) -> i32;
}

#[repr(i32)]
#[derive(Copy, Clone)]
pub enum CameraDeviceType {
    OV2640 = 0,
    GC0308 = 1,
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
    pub fn init(camera_device: CameraDeviceType, pixel_format: PixelFormat, frame_size: FrameSize, jpeg_quality: i32) -> Result<Self> {
        // Check if the configuration has changed
        if unsafe { if_camera_config_changed(pixel_format as i32, frame_size as i32, jpeg_quality) } == 0 {
            return Ok(Self);
        }
        
        // If changed, initialize the camera
        unsafe { 
            let ret = camera_init(camera_device as i32, pixel_format as i32, frame_size as i32, jpeg_quality);
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