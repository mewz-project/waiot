#include "wasi_nn.h"

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "esp_log.h"

const tflite::Model *model = nullptr;
static std::vector<uint8_t> g_model_storage;

tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;
int inference_count = 0;

static tflite::MicroMutableOpResolver<4> g_resolver;
static bool g_ops_registered = false;
// TODO
// - Keep multiple graphs
// - Keep multiple execution contexts

constexpr int kTensorArenaSize = 10 * 1024;
alignas(16) static uint8_t tensor_arena[kTensorArenaSize];

extern "C"
{
    void dump_ops(const tflite::Model *m)
    {
        const auto *opcodes = m->operator_codes();
        if (!opcodes)
        {
            ESP_LOGW("wasi_nn", "no operator_codes");
            return;
        }
        for (uint32_t i = 0; i < opcodes->size(); ++i)
        {
            auto *oc = opcodes->Get(i);
            int code = oc->builtin_code();
            ESP_LOGI("wasi_nn", "OpCode[%u] builtin=%d", i, code);
        }
        const auto *sg = m->subgraphs();
        if (!sg)
            return;
        for (uint32_t si = 0; si < sg->size(); ++si)
        {
            auto *s = sg->Get(si);
            auto *ops = s->operators();
            ESP_LOGI("wasi_nn", "Subgraph %u operators=%u", si, ops ? ops->size() : 0);
            if (!ops)
                continue;
            for (uint32_t oi = 0; oi < ops->size(); ++oi)
            {
                auto *op = ops->Get(oi);
                int opcode_index = op->opcode_index();
                int code = opcodes->Get(opcode_index)->builtin_code();
                ESP_LOGI("wasi_nn", "  op[%u]: opcode_index=%d builtin=%d", oi, opcode_index, code);
            }
        }
    }

    void dump_model(const tflite::Model *model)
    {
        ESP_LOGI("wasi_nn", "dump_model:");

        ESP_LOGI("wasi_nn", "  Model version: %d", model->version());
        auto *subgraphs = model->subgraphs();
        if (subgraphs)
        {
            ESP_LOGI("wasi_nn", "  Number of subgraphs: %d", subgraphs->size());
            for (uint32_t i = 0; i < subgraphs->size(); i++)
            {
                auto *subgraph = subgraphs->Get(i);
                ESP_LOGI("wasi_nn", "    Subgraph %d name: %s", i,
                         subgraph->name() ? subgraph->name()->c_str() : "N/A");
                ESP_LOGI("wasi_nn", "    Number of operators: %d", subgraph->operators()->size());

                // In/Outputs
                ESP_LOGI("wasi_nn", "    Number of inputs: %d", subgraph->inputs()->size());
                for (uint32_t j = 0; j < subgraph->inputs()->size(); j++)
                {
                    auto in_idx = subgraph->inputs()->Get(j);
                    ESP_LOGI("wasi_nn", "      Input[%d]: tensor index=%d", j, in_idx);
                }

                ESP_LOGI("wasi_nn", "    Number of outputs: %d", subgraph->outputs()->size());
                for (uint32_t j = 0; j < subgraph->outputs()->size(); j++)
                {
                    auto out_idx = subgraph->outputs()->Get(j);
                    ESP_LOGI("wasi_nn", "      Output[%d]: tensor index=%d", j, out_idx);
                }

                // Tensors
                ESP_LOGI("wasi_nn", "    Number of tensors: %d", subgraph->tensors()->size());
                if (subgraph->tensors())
                {
                    for (uint32_t j = 0; j < subgraph->tensors()->size(); j++)
                    {
                        auto *tensor = subgraph->tensors()->Get(j);
                        if (!tensor)
                        {
                            continue;
                        }

                        const auto *tensor_name = tensor->name() ? tensor->name()->c_str() : "<noname>";
                        auto type = tensor->type();
                        auto shape = tensor->shape();

                        if (shape)
                        {
                            std::string dims;
                            for (uint32_t d = 0; d < shape->size(); d++)
                            {
                                dims += std::to_string(shape->Get(d));
                                if (d + 1 < shape->size())
                                {
                                    dims += "x";
                                }
                            }
                            ESP_LOGI("wasi_nn", "     Tensor[%d]: %s (Type=%d, Shape=%s)", j, tensor_name, type,
                                     dims.c_str());
                        }
                    }
                }
            }
        }
    }

    int load(wasm_exec_env_t exec_env, graph_builder_array *builder, graph_encoding encoding,
             execution_target target, graph *g)
    {
        ESP_LOGE("wasi_nn", "load: encoding=%d, target=%d", encoding, target);
        return success;
    }

    int load_simple(wasm_exec_env_t exec_env, uint32_t model_ptr_idx, uint32_t model_size)
    {
        ESP_LOGI("wasi_nn", "load_simple");

        wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
        if (!instance)
        {
            ESP_LOGE("wasi_nn", "load_simple: Failed to get module instance.");
            return invalid_argument;
        }

        // Convert binary to TFLite model
        if (!wasm_runtime_validate_app_addr(instance, model_ptr_idx, model_size))
        {
            ESP_LOGE("wasi_nn", "load_simple: invalid memory range\n");
            return invalid_argument;
        }
        char *src = (char *)wasm_runtime_addr_app_to_native(instance, model_ptr_idx);
        g_model_storage.assign(src, src + model_size);

        ESP_LOGI("wasi_nn", "model[0]=%02x %02x %02x %02x", src[0], src[1], src[2], src[3]);

        model = tflite::GetModel(g_model_storage.data());

        // Verify model
        flatbuffers::Verifier v(g_model_storage.data(), g_model_storage.size());
        if (!tflite::VerifyModelBuffer(v))
        {
            ESP_LOGE("wasi_nn", "VerifyModelBuffer failed");
            g_model_storage.clear();
            model = nullptr;
            return invalid_argument;
        }

        ESP_LOGI("wasi_nn", "load: Model loaded successfully.");
        dump_model(model);

        return success;
    }

    int load_by_name(wasm_exec_env_t exec_env, const char *name, uint32_t namelen, graph *g)
    {

        ESP_LOGE("wasi_nn", "load_by_name is not implemented");
        return success;
    }

    int init_execution_context_simple(wasm_exec_env_t exec_env)
    {
        ESP_LOGI("wasi_nn", "init_execution_context_simple");

        dump_ops(model);

        // Register operations
        ESP_LOGI("wasi_nn", "Registering ops");
        if (!g_ops_registered)
        {
            if (g_resolver.AddFullyConnected() != kTfLiteOk ||
                g_resolver.AddConv2D() != kTfLiteOk ||
                g_resolver.AddReshape() != kTfLiteOk ||
                g_resolver.AddSoftmax() != kTfLiteOk)
            {
                return runtime_error;
            }
            g_ops_registered = true;
        }

        // Build the interpreter
        ESP_LOGI("wasi_nn", "Creating static interpreter");
        tflite::InitializeTarget();
        static tflite::MicroInterpreter static_interp = tflite::MicroInterpreter(model, g_resolver, tensor_arena, kTensorArenaSize);
        interpreter = &static_interp;

        // Allocate memory for the model's tensors.
        ESP_LOGI("wasi_nn", "Allocating tensors");
        TfLiteStatus allocate_status = interpreter->AllocateTensors();
        if (allocate_status != kTfLiteOk)
        {
            ESP_LOGE("wasi_nn", "AllocateTensors() failed");
            return runtime_error;
        }

        auto used = interpreter->arena_used_bytes();
        ESP_LOGI("wasi_nn", "Arena used: %u / %u", (unsigned)used, (unsigned)kTensorArenaSize);

        // Obtain pointers to the model's input and output tensors.
        ESP_LOGI("wasi_nn", "Obtaining input and output tensors");
        input = interpreter->input(0);
        output = interpreter->output(0);
        if (!input || !output)
        {
            ESP_LOGE("wasi_nn", "input/output nullptr");
            return runtime_error;
        }
        if (!tflite::GetTensorData<uint8_t>(input) || !tflite::GetTensorData<uint8_t>(output))
        {
            ESP_LOGE("wasi_nn", "Tensor pointers are null (input/output/data)");
            ESP_LOGE("wasi_nn", "GetTensorData(input)=%p, GetTensorData(output)=%p", tflite::GetTensorData<uint8_t>(input), tflite::GetTensorData<uint8_t>(output));
            return runtime_error;
        }
        ESP_LOGI("wasi_nn", "Input tensor type: %d, bytes: %d, allocation_type=%d", input->type, input->bytes, input->allocation_type);
        ESP_LOGI("wasi_nn", "Output tensor type: %d, bytes: %d, allocation_type=%d", output->type, output->bytes, output->allocation_type);

        // Keep track of how many inferences we have performed.
        inference_count = 0;

        return success;
    }

    int init_execution_context(wasm_exec_env_t exec_env, graph g, graph_execution_context *exec_ctx)
    {
        ESP_LOGI("wasi_nn", "init_execution_context");
        init_execution_context_simple(exec_env);
        *exec_ctx = 0; // dummy execution context handle
        return success;
    }

    int set_input(wasm_exec_env_t exec_env, uint32_t exec_ctx, uint32_t index,
                  const tensor *input_tensor)
    {
        ESP_LOGE("wasi_nn", "set_input");
        return success;
    }

    int set_input_simple(wasm_exec_env_t exec_env, uint32_t input_ptr_idx, uint32_t input_size)
    {
        ESP_LOGI("wasi_nn", "set_input_simple");

        // Convert to native pointer
        wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
        if (!instance)
        {
            ESP_LOGE("wasi_nn", "set_input_simple: Failed to get module instance.");
            return invalid_argument;
        }
        if (!wasm_runtime_validate_app_addr(instance, input_ptr_idx, input_size))
        {
            ESP_LOGE("wasi_nn", "set_input_simple: invalid memory range\n");
            return invalid_argument;
        }
        char *buff = (char *)wasm_runtime_addr_app_to_native(instance, input_ptr_idx);
        ESP_LOGI("wasi_nn", "input[0]=%02x %02x %02x %02x", buff[0], buff[1], buff[2], buff[3]);
        ESP_LOGI("wasi_nn", "input_size=%d, expected=%d", input_size, input->bytes);

        // Copy input data to TFLM input tensor
        memcpy(tflite::GetTensorData<uint8_t>(input), buff, std::min((size_t)input_size, input->bytes));
        return success;
    }

    int compute(wasm_exec_env_t exec_env, graph_execution_context exec_ctx)
    {
        ESP_LOGE("wasi_nn", "compute");
        return success;
    }

    int compute_simple(wasm_exec_env_t exec_env)
    {
        ESP_LOGI("wasi_nn", "compute_simple");

        // Run inference
        TfLiteStatus invoke_status = interpreter->Invoke();
        if (invoke_status != kTfLiteOk)
        {
            ESP_LOGE("wasi_nn", "compute: Invoke failed");
            return runtime_error;
        }
        return success;
    }

    int get_output(wasm_exec_env_t exec_env, uint32_t exec_ctx, uint32_t index,
                   uint8_t *output_buff, uint32_t output_buff_max_size, uint32_t *output_size)
    {
        ESP_LOGE("wasi_nn", "get_output");
        return success;
    }

    int get_output_simple(wasm_exec_env_t exec_env,
                          uint32_t output_ptr_idx, uint32_t output_buff_max_size)
    {
        ESP_LOGI("wasi_nn", "get_output_simple");
        if (output->bytes > output_buff_max_size)
        {
            ESP_LOGE("wasi_nn", "get_output: Output buffer too small");
            return invalid_argument;
        }

        // Convert to native pointer
        wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
        if (!instance)
        {
            ESP_LOGE("wasi_nn", "get_output_simple: Failed to get module instance.");
            return invalid_argument;
        }
        if (!wasm_runtime_validate_app_addr(instance, output_ptr_idx, output->bytes))
        {
            ESP_LOGE("wasi_nn", "get_output_simple: invalid memory range\n");
            return invalid_argument;
        }
        char *output_buff = (char *)wasm_runtime_addr_app_to_native(instance, output_ptr_idx);

        // Copy output data from TFLM output tensor
        ESP_LOGI("wasi_nn", "output bytes=%d, output_buff_max_size=%d", output->bytes, output_buff_max_size);
        memcpy(output_buff, tflite::GetTensorData<uint8_t>(output), output->bytes);
        for (size_t i = 0; i < std::min((size_t)output->bytes, (size_t)16); ++i)
        {
            ESP_LOGI("wasi_nn", " output[%zu]=%02x", i, (uint8_t)output_buff[i]);
        }

        // For debugging:
        // print first 4 bytes as float
        if (output->bytes >= 4)
        {
            float f;
            memcpy(&f, output_buff, sizeof(float));
            ESP_LOGI("wasi_nn", " output as float=%f", f);
        }

        return success;
    }

} // extern "C"