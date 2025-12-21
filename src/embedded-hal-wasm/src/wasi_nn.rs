#![allow(non_camel_case_types)]

pub mod ffi {
    #![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]
    use core::ffi::c_char;

    #[repr(C)]
    #[derive(Debug, Clone, Copy)]
    pub struct wasi_nn_u32_list_t {
        pub len: u32,
        pub ptr: *mut u32,
    }

    #[repr(C)]
    #[derive(Debug, Clone, Copy)]
    pub struct wasi_nn_u8_list_t {
        pub len: u32,
        pub ptr: *mut u8,
    }

    #[repr(C)]
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub enum wasi_nn_error {
        success = 0,
        invalid_argument = 1,
        invalid_encoding = 2,
        missing_memory = 3,
        busy = 4,
        runtime_error = 5,
        unsupported_operation = 6,
        too_large = 7,
        not_found = 8,
    }

    pub type graph = u32;
    pub type graph_execution_context = u32;

    #[repr(C)]
    #[derive(Debug, Clone, Copy)]
    pub struct graph_builder_array {
        pub buf_offset: u32,
        pub size: u32,
    }

    #[repr(C)]
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub enum graph_encoding {
        openvino = 0,
        onnx = 1,
        tensorflow = 2,
        pytorch = 3,
        tensorflowlite = 4,
        ggml = 5,
        autodetect = 6,
        unknown_backend = 7,
    }

    #[repr(C)]
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub enum execution_target {
        cpu = 0,
        gpu = 1,
        tpu = 2,
    }

    #[repr(C)]
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub enum tensor_type {
        fp16 = 0,
        fp32 = 1,
        up8 = 2,
        ip32 = 3,
    }

    #[repr(C)]
    #[derive(Debug, Clone, Copy)]
    pub struct tensor {
        pub dimensions: wasi_nn_u32_list_t, // array of dimensions
        pub r#type: u8,
        pub _pad: [u8; 3],
        pub data: wasi_nn_u8_list_t,
    }

    #[link(wasm_import_module = "wasi:wasi_nn")]
    unsafe extern "C" {
        pub fn load(
            builder: *mut graph_builder_array,
            encoding: graph_encoding,
            target: execution_target,
            g: *mut graph,
        ) -> i32;

        pub fn load_by_name(name: *const c_char, namelen: u32, g: *mut graph) -> i32;

        pub fn init_execution_context(g: graph, exec_ctx: *mut graph_execution_context) -> i32;

        pub fn set_input(exec_ctx: u32, index: u32, input_tensor: *const tensor) -> i32;

        pub fn compute(exec_ctx: graph_execution_context) -> i32;

        pub fn get_output(
            exec_ctx: u32,
            index: u32,
            output_buff: *mut u8,
            output_buff_max_size: u32,
            output_size: *mut u32,
        ) -> i32;

        pub fn load_simple(buffer_ptr: u32, buffer_size: u32) -> i32;
        pub fn init_execution_context_simple() -> i32;
        pub fn set_input_simple(input_data_ptr: u32, input_data_size: u32) -> i32;
        pub fn compute_simple() -> i32;
        pub fn get_output_simple(output_buff_ptr: u32, output_buff_size: u32) -> i32;
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Error {
    InvalidArgument,
    InvalidEncoding,
    MissingMemory,
    Busy,
    RuntimeError,
    UnsupportedOperation,
    TooLarge,
    NotFound,
    Unknown(i32),
}

impl Error {
    #[inline]
    pub fn from_code(code: i32) -> Result<(), Self> {
        use Error::*;
        match code {
            0 => Ok(()),
            1 => Err(InvalidArgument),
            2 => Err(InvalidEncoding),
            3 => Err(MissingMemory),
            4 => Err(Busy),
            5 => Err(RuntimeError),
            6 => Err(UnsupportedOperation),
            7 => Err(TooLarge),
            8 => Err(NotFound),
            x => Err(Unknown(x)),
        }
    }
}

#[inline]
fn status_to_result(code: i32) -> Result<(), Error> {
    Error::from_code(code)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GraphEncoding {
    OpenVINO,
    ONNX,
    TensorFlow,
    PyTorch,
    TensorFlowLite,
    GGML,
    AutoDetect,
    UnknownBackend,
}

impl From<GraphEncoding> for ffi::graph_encoding {
    fn from(g: GraphEncoding) -> Self {
        use GraphEncoding::*;
        match g {
            OpenVINO => Self::openvino,
            ONNX => Self::onnx,
            TensorFlow => Self::tensorflow,
            PyTorch => Self::pytorch,
            TensorFlowLite => Self::tensorflowlite,
            GGML => Self::ggml,
            AutoDetect => Self::autodetect,
            UnknownBackend => Self::unknown_backend,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ExecutionTarget {
    CPU,
    GPU,
    TPU,
}
impl From<ExecutionTarget> for ffi::execution_target {
    fn from(t: ExecutionTarget) -> Self {
        match t {
            ExecutionTarget::CPU => Self::cpu,
            ExecutionTarget::GPU => Self::gpu,
            ExecutionTarget::TPU => Self::tpu,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TensorType {
    FP16,
    FP32,
    UP8,
    IP32,
}
impl From<TensorType> for u8 {
    fn from(t: TensorType) -> Self {
        match t {
            TensorType::FP16 => 0,
            TensorType::FP32 => 1,
            TensorType::UP8 => 2,
            TensorType::IP32 => 3,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct Tensor<'a> {
    dims: &'a [u32],
    data: &'a [u8],
    ty: TensorType,
}

impl<'a> Tensor<'a> {
    pub const fn new(dims: &'a [u32], data: &'a [u8], ty: TensorType) -> Self {
        Self { dims, data, ty }
    }
    fn as_ffi(&self) -> ffi::tensor {
        let dims_ptr = if self.dims.is_empty() {
            core::ptr::null_mut()
        } else {
            self.dims.as_ptr() as *mut u32
        };
        let data_ptr = if self.data.is_empty() {
            core::ptr::null_mut()
        } else {
            self.data.as_ptr() as *mut u8
        };
        ffi::tensor {
            dimensions: ffi::wasi_nn_u32_list_t {
                len: self.dims.len() as u32,
                ptr: dims_ptr,
            },
            r#type: u8::from(self.ty),
            _pad: [0; 3],
            data: ffi::wasi_nn_u8_list_t {
                len: self.data.len() as u32,
                ptr: data_ptr,
            },
        }
    }
    pub fn dims(&self) -> &'a [u32] {
        self.dims
    }
    pub fn data(&self) -> &'a [u8] {
        self.data
    }
    pub fn ty(&self) -> TensorType {
        self.ty
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Graph(pub u32);
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ExecCtx(pub u32);

pub struct WasiNN;

impl WasiNN {
    pub const fn new() -> Self {
        Self {}
    }
    pub fn load(
        &self,
        mut builder: ffi::graph_builder_array,
        encoding: GraphEncoding,
        target: ExecutionTarget,
    ) -> Result<Graph, Error> {
        let mut g: ffi::graph = 0;
        let rc = unsafe {
            ffi::load(
                &mut builder as *mut _,
                encoding.into(),
                target.into(),
                &mut g as *mut _,
            )
        };
        status_to_result(rc).map(|_| Graph(g))
    }

    pub fn init_execution_context(&self, graph: Graph) -> Result<ExecCtx, Error> {
        let mut ctx: ffi::graph_execution_context = 0;
        let rc = unsafe { ffi::init_execution_context(graph.0, &mut ctx as *mut _) };
        status_to_result(rc).map(|_| ExecCtx(ctx))
    }

    pub fn set_input(&self, ctx: ExecCtx, index: u32, tensor: &Tensor<'_>) -> Result<(), Error> {
        let t_ffi = tensor.as_ffi();
        let rc = unsafe { ffi::set_input(ctx.0, index, &t_ffi as *const _) };
        status_to_result(rc)
    }

    pub fn compute(&self, ctx: ExecCtx) -> Result<(), Error> {
        let rc = unsafe { ffi::compute(ctx.0) };
        status_to_result(rc)
    }

    pub fn get_output_into(
        &self,
        ctx: ExecCtx,
        index: u32,
        out: &mut [u8],
    ) -> Result<usize, Error> {
        let mut written: u32 = 0;
        let rc = unsafe {
            ffi::get_output(
                ctx.0,
                index,
                if out.is_empty() {
                    core::ptr::null_mut()
                } else {
                    out.as_mut_ptr()
                },
                out.len() as u32,
                &mut written,
            )
        };
        status_to_result(rc).map(|_| written as usize)
    }

    pub fn load_simple(&self, buffer: &[u8], buffer_size: u32) -> i32 {
        unsafe { ffi::load_simple(buffer.as_ptr() as u32, buffer_size) }
    }

    pub fn init_execution_context_simple(&self) -> i32 {
        unsafe { ffi::init_execution_context_simple() }
    }

    pub fn set_input_simple(&self, input_data: &[u8], input_data_size: u32) -> i32 {
        unsafe { ffi::set_input_simple(input_data.as_ptr() as u32, input_data_size) }
    }

    pub fn compute_simple(&self) -> i32 {
        unsafe { ffi::compute_simple() }
    }

    pub fn get_output_simple(&self, output_buff: &mut [u8], output_buff_size: u32) -> i32 {
        unsafe { ffi::get_output_simple(output_buff.as_mut_ptr() as u32, output_buff_size) }
    }
}
