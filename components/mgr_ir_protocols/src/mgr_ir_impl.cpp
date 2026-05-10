#include "drv_ir_rmt.h"
#include "esp_log.h"
#include "ir_engine.h"

static const char *TAG = "mgr_ir_impl";

// Driver handles are managed by drv_ir_rmt, OR we pass data to it.
// Matrix and Raw sending are handled in mgr_ir_main.c and mgr_ir_matrix.c.

