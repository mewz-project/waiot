use core::fmt;

#[link(wasm_import_module = "wasi:wasi_snapshot_preview1")]
unsafe extern "C" {
    fn fd_write(fd: i32, iov_ptr: i32, iov_cnt: i32, nwritten_ptr: i32) -> i32;
}

#[repr(C)]
struct Iovec {
    iov_base: i32,
    iov_len: i32,
}

pub struct FdWriter {
    fd: i32,
}

impl FdWriter {
    pub const fn stdout() -> Self { Self { fd: 1 } }
    pub const fn stderr() -> Self { Self { fd: 2 } }

    #[inline]
    pub fn write_all(&self, bytes: &[u8]) {
        let iov = Iovec {
            iov_base: bytes.as_ptr() as i32,
            iov_len: bytes.len() as i32,
        };
        let mut nwritten: i32 = 0;
        unsafe {
            // TODO: use result
            let _ = fd_write(
                self.fd,
                (&iov as *const Iovec) as i32,
                1,
                (&mut nwritten as *mut i32) as i32,
            );
        }
    }
}

impl fmt::Write for FdWriter {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.write_all(s.as_bytes());
        Ok(())
    }
}

struct Cursor<'a> {
    buf: &'a mut [u8],
    len: usize,
}

impl<'a> Cursor<'a> {
    #[inline]
    fn written_len(&self) -> usize {
        self.len
    }
}

impl<'a> fmt::Write for Cursor<'a> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let bytes = s.as_bytes();
        let cap = self.buf.len().saturating_sub(self.len);
        let n = core::cmp::min(cap, bytes.len());
        if n > 0 {
            self.buf[self.len..self.len + n].copy_from_slice(&bytes[..n]);
            self.len += n;
        }
        Ok(())
    }
}

pub fn print(args: fmt::Arguments) {
    const BUF: usize = 256;
    let mut buf = [0u8; BUF];
    let len = {
        let mut cur = Cursor { buf: &mut buf, len: 0 };
        let _ = fmt::write(&mut cur, args);
        cur.written_len()
    };
    FdWriter::stdout().write_all(&buf[..len]);
}

pub fn eprint(args: fmt::Arguments) {
    const BUF: usize = 256;
    let mut buf = [0u8; BUF];
    let len = {
        let mut cur = Cursor { buf: &mut buf, len: 0 };
        let _ = fmt::write(&mut cur, args);
        cur.written_len()
    };
    FdWriter::stderr().write_all(&buf[..len]);
}

#[macro_export]
macro_rules! println {
    ($($arg:tt)*) => {{
        $crate::logging::print(core::format_args!($($arg)*));
        $crate::logging::print(core::format_args!("\n"));
    }};
}

#[macro_export]
macro_rules! eprintln {
    ($($arg:tt)*) => {{
        $crate::logging::eprint(core::format_args!($($arg)*));
        $crate::logging::eprint(core::format_args!("\n"));
    }};
}