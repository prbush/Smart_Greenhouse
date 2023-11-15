#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "fan.h"
#include "lights.h"
#include "pdlc.h"
#include "sdkconfig.h"
#include "environmental_control.h"

// Static private object pointer
static Environmental_control *self;
static int timer_id = 1337;

// Logger tag
const char* ENVIRONMENTAL_TAG = "Environmental control";

// Private functions
void timer_callback(TimerHandle_t xTimer);
// Public functions privided via struct fn pointers


esp_err_t environmental_control_init(Environmental_control *struct_ptr, Fan *fan, Lights *lights, PDLC *pdlc)
{
  esp_err_t return_code = ESP_FAIL;

  self = struct_ptr;

  self->timer_handle = xTimerCreate("ENV timer", 60 * CONFIG_FREERTOS_HZ, pdFALSE, &timer_id, timer_callback);

  return return_code;
}