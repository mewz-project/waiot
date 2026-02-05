#include "wasi_nn_esp_dl.h"

#include "esp_log.h"

// ESP-DL
#include "dl_model_base.hpp"
#include "dl_tensor_base.hpp"
#include "fbs_model.hpp"

#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstring>

static const char *TAG = "wasi_nn_espdl";

static std::vector<uint8_t> g_model_storage;

static fbs::FbsModel *g_fbs_model = nullptr;
static dl::Model *g_model = nullptr;

static dl::TensorBase *g_input = nullptr;
static dl::TensorBase *g_output = nullptr;
