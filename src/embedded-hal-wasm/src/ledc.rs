#[link(wasm_import_module = "wasi:ledc/ledc")]
unsafe extern "C" {
    // Initialize LEDC
    fn ledc_init(pin: u32, channel: u32, freq: u32, resolution: u32) -> i32;

    // Set LEDC frequency
    fn ledc_set_freq(channel: u32, freq: u32) -> i32;

    // Set LEDC duty
    fn ledc_set_duty(channel: u32, duty: u32) -> i32;

    // Update LEDC duty
    fn ledc_update_duty(channel: u32) -> i32;
}

pub struct WasmLedc;

impl WasmLedc {
    pub fn init(pin: u32, channel: u32, freq: u32, resolution: u32) -> Result<(), i32> {
        let _ = unsafe { ledc_init(pin, channel, freq, resolution) };
        Ok(())
    }

    pub fn set_freq(channel: u32, freq: u32) -> Result<(), i32> {
        let _ = unsafe { ledc_set_freq(channel, freq) };
        Ok(())
    }

    pub fn set_duty(channel: u32, duty: u32) -> Result<(), i32> {
        let _ = unsafe { ledc_set_duty(channel, duty) };
        Ok(())
    }

    pub fn update_duty(channel: u32) -> Result<(), i32> {
        let _ = unsafe { ledc_update_duty(channel) };
        Ok(())
    }
}
