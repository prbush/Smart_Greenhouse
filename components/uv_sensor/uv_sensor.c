#include <stdio.h>
#include "uv_sensor.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "string.h"
#include "sdkconfig.h"

// Static private object pointer
static UV_sensor *self;

// Logger tag
static const char *UV_TAG = "AS7331";

// Helper functions


// Private functions
static uint8_t    _uv_sensor_get_ig(void);
static void       _uv_sensor_reset(void);
static esp_err_t  _uv_get_readings(UV_values* return_data);


// Public init function
esp_err_t uv_sensor_init(UV_sensor *struct_ptr, uint32_t timeout_ticks, i2c_port_t i2c_port_num)
{

}
