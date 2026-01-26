#![no_std]
#![no_main]

use core::panic::PanicInfo;

use embedded_hal_wasm::http_client::{HttpClient, Method, Result};

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

fn consume_received_chunk(_bytes: &[u8]) {
}

fn curl_example_com(buf: &mut [u8]) -> Result<()> {
    let url = "http://example.com/";
    let mut cli = HttpClient::open(Method::Get, url, 5_000, None)?;

    let _ = cli.set_header("User-Agent", "wamr-wasm/0.1");

    let _status = cli.status()?;

    loop {
        let n = cli.read_into(buf)?;
        if n == 0 { break; }
        consume_received_chunk(&buf[..n]);
    }

    cli.close()?;
    Ok(())
}

fn post_to_10_0_0_1(buf: &mut [u8]) -> Result<()> {
    let url = "http://10.0.0.1/";
    let body = br#"{"hello":"world"}"#;

    let mut cli = HttpClient::open(Method::Post, url, 5_000, Some(body.len()))?;

    cli.set_header("Content-Type", "application/json")?;
    let _ = cli.set_header("User-Agent", "wamr-wasm/0.1");

    let _written = cli.write(body)?;
    let _status = cli.status()?;

    loop {
        let n = cli.read_into(buf)?;
        if n == 0 { break; }
        consume_received_chunk(&buf[..n]);
    }

    cli.close()?;
    Ok(())
}

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> i32 {
    let mut buf = [0u8; 1024];

    if let Err(e) = curl_example_com(&mut buf) {
        return e.0;
    }
    if let Err(e) = post_to_10_0_0_1(&mut buf) {
        return e.0;
    }
    0
}