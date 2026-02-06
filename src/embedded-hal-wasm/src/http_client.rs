// use core::fmt;

#[repr(i32)]
#[derive(Copy, Clone)]
pub enum Method {
    Get = 0,
    Post = 1,
    Put = 2,
    Delete = 3,
}

#[derive(Copy, Clone, PartialEq, Eq)]
pub struct HostError(pub i32);

// impl fmt::Display for HostError {
//     fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
//         write!(f, "HostError({})", self.0)
//     }
// }

pub type Result<T> = core::result::Result<T, HostError>;

#[link(wasm_import_module = "wasi_waiot:http_client")]
unsafe extern "C" {
    fn http_init(url_ptr: i32, url_len: i32, timeout_ms: i32) -> i32;
    fn http_open(handle: i32, method: i32, content_len: i32) -> i32;
    fn http_set_header(handle: i32, k_ptr: i32, k_len: i32, v_ptr: i32, v_len: i32) -> i32;
    fn http_fetch_headers(handle: i32) -> i32;
    fn http_write(handle: i32, buf_ptr: i32, buf_len: i32) -> i32;
    fn http_read(handle: i32, buf_ptr: i32, buf_len: i32) -> i32;
    fn http_status(handle: i32) -> i32;
    fn http_close(handle: i32) -> i32;
}

#[inline]
fn ptr_len(s: &str) -> (i32, i32) {
    (s.as_ptr() as i32, s.len() as i32)
}

#[inline]
fn bytes_ptr_len(b: &[u8]) -> (i32, i32) {
    (b.as_ptr() as i32, b.len() as i32)
}

#[inline]
fn ok_or_err(rc: i32) -> Result<i32> {
    if rc < 0 { Err(HostError(rc)) } else { Ok(rc) }
}

pub struct HttpClient {
    handle: i32,
}

/// A wrapper for HTTP client on ESP-IDF platform.
/// `close` is called automatically when dropped.
impl HttpClient {
    pub fn init(url: &str, timeout_ms: i32) -> Result<Self> {
        let (uptr, ulen) = ptr_len(url);
        let h = unsafe { http_init(uptr, ulen, timeout_ms) };
        if h < 0 { return Err(HostError(h)); }
        Ok(Self { handle: h })
    }

    /// Open a new HTTP connection.
    /// content_len: set to 0 for no body
    pub fn open(&mut self, method: Method, content_len: i32) -> Result<()> {
        let h = unsafe { http_open(self.handle, method as i32, content_len) };
        if h < 0 { return Err(HostError(h)); }
        Ok(())
    }

    pub fn set_header(&mut self, key: &str, val: &str) -> Result<()> {
        let (kptr, klen) = ptr_len(key);
        let (vptr, vlen) = ptr_len(val);
        ok_or_err(unsafe { http_set_header(self.handle, kptr, klen, vptr, vlen) })?;
        Ok(())
    }

    pub fn fetch_headers(&mut self) -> Result<()> {
        ok_or_err(unsafe { http_fetch_headers(self.handle) })?;
        Ok(())
    }

    pub fn status(&self) -> Result<i32> {
        let s = unsafe { http_status(self.handle) };
        if s < 0 { Err(HostError(s)) } else { Ok(s) }
    }

    pub fn write(&mut self, data: &[u8]) -> Result<usize> {
        let (bptr, blen) = bytes_ptr_len(data);
        let n = ok_or_err(unsafe { http_write(self.handle, bptr, blen) })?;
        Ok(n as usize)
    }

    pub fn read_into(&mut self, buf: &mut [u8]) -> Result<usize> {
        let n = unsafe { http_read(self.handle, buf.as_mut_ptr() as i32, buf.len() as i32) };
        if n < 0 { return Err(HostError(n)); }
        Ok(n as usize)
    }

    pub fn close(&mut self) -> Result<()> {
        if self.handle >= 0 {
            ok_or_err(unsafe { http_close(self.handle) })?;
            self.handle = -1;
        }
        Ok(())
    }
}

impl Drop for HttpClient {
    fn drop(&mut self) {
        let _ = self.close();
    }
}
