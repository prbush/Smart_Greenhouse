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
static uint8_t    uv_sensor_get_id(void);
static uint16_t   uv_sensor_get_status(void);
static void       uv_sensor_apply_settings(measurement_mode_t mode, uint8_t standby_enabled, uint8_t break_time,
                    uint8_t gain, uint8_t conversion_time);

// Private functions
// static uint8_t    _uv_sensor_get_id(void);
static void       _uv_sensor_reset(void);
static esp_err_t  _uv_sensor_get_readings(UV_values* return_data);


// Public init function
esp_err_t uv_sensor_init(UV_sensor *struct_ptr, uint32_t timeout_ticks, i2c_port_t i2c_port_num)
{
  esp_err_t return_code = ESP_OK;
  uint8_t chip_id = 0;

  // Assign private object pointer
  self = struct_ptr;

  // Assign struct fields
  // self->get_id        = _uv_sensor_get_id;
  self->reset         = _uv_sensor_reset;
  self->get_readings  = _uv_sensor_get_readings;

  // Check the chip ID
  chip_id = uv_sensor_get_id();
  if (chip_id != AS7331_ID) {
    ESP_LOGE(UV_TAG, "Chip ID did not match default ID. UV Sensor init failed.");
    return_code = ESP_FAIL;
    return return_code;
  }

  // Apply settings




  return return_code;
}
