#![no_std]
#![no_main]

use panic_halt as _;

use embedded_hal_wasm::http_client::{HttpClient, Method};

const BUF_SIZE: usize = 512;

#[unsafe(no_mangle)]
pub extern "C" fn _start() -> () {
    // HTTP GET
    let url = "http://example.com/";
    let mut buffer = [0u8; BUF_SIZE];

    // Open HTTP connection
    let content_length = 0; // No body for GET request
    let mut client = match HttpClient::open(Method::Get, url, 5_000, content_length) {
        Ok(c) => c,
        Err(_) => {
            return;
        }
    };

    let _ = client.fetch_headers();
    
    // Read response
    for _ in 0..10 {
        let n = client.read_into(&mut buffer).unwrap_or(0);
        if n == 0 {
            break;
        }
        
        // Here process the received data in `buffer[..n]`
    }

    let _ = client.close();
}