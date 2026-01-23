#pragma once

#include "wasm_export.h"
#include "core/shared/platform/include/platform_wasi_types.h"

// I2C
int32_t waiot_i2c_param_config(wasm_exec_env_t exec_env, int32_t port,
                               int32_t sda_gpio, int32_t scl_gpio,
                               int32_t freq_hz);
int32_t waiot_i2c_driver_install(wasm_exec_env_t exec_env, int32_t port);
int32_t waiot_i2c_master_write(wasm_exec_env_t exec_env, int32_t port,
                               int32_t addr, int32_t write_buff_ptr_idx, int32_t write_size, int32_t ticks_to_wait);
int32_t waiot_i2c_master_read(wasm_exec_env_t exec_env, int32_t port,
                              int32_t addr, int32_t read_buff_ptr_idx, int32_t read_size, int32_t ticks_to_wait);

// GPIO
int32_t
gpio_set_pin_mode(wasm_exec_env_t exec_env, int32_t pin, int32_t dir);
int32_t
gpio_read(wasm_exec_env_t exec_env, int32_t pin);
int32_t
gpio_write(wasm_exec_env_t exec_env, int32_t pin, int32_t value);

// Delay
int32_t delay_ns(wasm_exec_env_t exec_env, int32_t ns);

// LEDC
int32_t pwm_init(wasm_exec_env_t exec_env, int32_t pin, int32_t channel, int32_t freq, int32_t resolution, int32_t speed_mode);
int32_t pwm_set_duty(wasm_exec_env_t exec_env, int32_t channel, int32_t duty, int32_t speed_mode);
int32_t pwm_update_duty(wasm_exec_env_t exec_env, int32_t channel, int32_t speed_mode);
int32_t pwm_set_frequency(wasm_exec_env_t exec_env, int32_t channel, int32_t freq, int32_t speed_mode);

// Logging (WASI: wasi_snapshot_review1.fd_write)
int32_t
fd_write(wasm_exec_env_t exec_env, int32_t fd, int32_t buf_iovec_addr,
         int32_t vec_len, int32_t size_addr);

// Log ring buffer accessors for HTTP serving
int32_t log_get_filled(void);
int32_t log_read_from_oldest(uint32_t offset, uint8_t *out, uint32_t len);


// =====================================
// WASI Preview1 Socket APIs
// =====================================

/*
pub export fn sock_open(
    family: AddressFamily,
    typ: SocketType,
    fd_addr: i32,
) callconv(.c) WasiError

pub export fn sock_bind(
    fd: i32,
    ip_iovec_addr: i32,
    port: i32,
) callconv(.c) WasiError

pub export fn sock_listen(fd: i32, backlog: i32) WasiError

pub export fn sock_accept(fd: i32, new_fd_addr: i32) WasiError

pub export fn sock_recv(fd: i32, iovec_addr: i32, buf_len: i32, flags: i32, recv_len_addr: i32, oflags_addr: i32) WasiError


pub export fn sock_send(fd: i32, buf_iovec_addr: i32, buf_len: i32, flags: i32, send_len_addr: i32) WasiError


pub export fn sock_connect(fd: i32, buf_ioved_addr: i32, port: i32) WasiError


pub export fn sock_shutdown(fd: i32, flag: ShutdownFlag) WasiError


pub export fn sock_getpeeraddr(fd: i32, ip_iovec_addr: i32, type_addr: i32, port_addr: i32) WasiError


pub export fn sock_getlocaladdr(fd: i32, ip_iovec_addr: i32, type_addr: i32, port_addr: i32) WasiError


pub export fn sock_setsockopt(fd: i32, level: i32, optname: i32, optval_addr: i32, optlen: i32) WasiError
*/
__wasi_errno_t waiot_sock_open(wasm_exec_env_t exec_env,
                __wasi_address_family_t family,
                __wasi_sock_type_t type,
                int32_t fd_addr);
__wasi_errno_t waiot_sock_bind(wasm_exec_env_t exec_env,
                __wasi_fd_t fd,
                int32_t ip_iovec_addr,
                int32_t port);
__wasi_errno_t waiot_sock_listen(wasm_exec_env_t exec_env,
                __wasi_fd_t fd,
                int32_t backlog);
__wasi_errno_t waiot_sock_accept(wasm_exec_env_t exec_env,
                __wasi_fd_t fd,
                int32_t new_fd_addr);
__wasi_errno_t waiot_sock_recv(wasm_exec_env_t exec_env,
                __wasi_fd_t fd,
                int32_t iovec_addr,
                int32_t buf_len,
                int32_t flags,
                int32_t recv_len_addr,
                int32_t oflags_addr);
__wasi_errno_t waiot_sock_send(wasm_exec_env_t exec_env,
                __wasi_fd_t fd,
                int32_t iovec_addr,
                int32_t iovec_count,
                int32_t flags,
                int32_t send_len_addr);
__wasi_errno_t waiot_sock_connect(wasm_exec_env_t exec_env,
                __wasi_fd_t fd,
                int32_t ip_iovec_addr,
                int32_t port);
__wasi_errno_t waiot_sock_shutdown(wasm_exec_env_t exec_env,
                __wasi_fd_t fd,
                int32_t flag);
__wasi_errno_t waiot_sock_getpeeraddr(wasm_exec_env_t exec_env,
                __wasi_fd_t fd,
                int32_t ip_iovec_addr,
                int32_t type_addr,
                int32_t port_addr);
__wasi_errno_t waiot_sock_getlocaladdr(wasm_exec_env_t exec_env,
                __wasi_fd_t fd,
                int32_t ip_iovec_addr,
                int32_t type_addr,
                int32_t port_addr);
__wasi_errno_t waiot_sock_setsockopt(wasm_exec_env_t exec_env,
                __wasi_fd_t fd,
                int32_t level,
                int32_t optname,
                int32_t optval_addr,
                int32_t optlen);