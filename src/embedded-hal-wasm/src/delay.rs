use embedded_hal::delay::DelayNs;

#[link(wasm_import_module = "wasi:delay")]
unsafe extern "C" {
    fn delay_ns(ns: i32) -> i32;
}

pub struct WasmDelay;

#[cfg(target_arch = "wasm32")]
impl DelayNs for WasmDelay {
    #[inline]
    fn delay_ns(&mut self, ns: u32) {
        let mut ns_remaining = ns;
        while ns_remaining > i32::MAX as u32 {
            unsafe {
                let _ = delay_ns(i32::MAX);
            }
            ns_remaining -= i32::MAX as u32;
        }
        unsafe {
            let _ = delay_ns(ns_remaining as i32);
        }
    }
}
