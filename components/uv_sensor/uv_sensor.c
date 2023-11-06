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
static void       uv_sensor_get_status(void);
static void       uv_sensor_apply_settings(measurement_mode_t mode, standby_bit_t standby, uint8_t break_time,
                    uint8_t gain, internal_clock_t internal_clock, creg1_time_t conversion_time);
static esp_err_t  uv_generic_i2c_write(uint8_t reg_addr, uint8_t* write_data, size_t length);
static esp_err_t  uv_generic_i2c_read(uint8_t reg_addr, uint8_t* return_data, size_t length);

// Private functions
// static uint8_t    _uv_sensor_get_id(void);
static void       _uv_sensor_reset(void);
static esp_err_t  _uv_sensor_get_readings(UV_converted_values* return_data);


// Public init function
esp_err_t uv_sensor_init(UV_sensor *struct_ptr, uint32_t timeout_ticks, i2c_port_t i2c_port_num)
{
  esp_err_t return_code = ESP_OK;
  uint8_t chip_id = 0;

  // Assign private object pointer
  self = struct_ptr;

  // Assign struct fields
  self->i2c_port_num = i2c_port_num;
  self->i2c_timeout_ticks = timeout_ticks;
  self->delay_period = 512; // required delay in ms
  self->i2c_device_addr = AS7331_ADDRESS;

  self->gain = GAIN_8X;
  self->conversion_time = _512;
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
  uv_sensor_apply_settings(AS7331_CMD_MODE, STDBY_OFF, 0, self->gain, AS7331_1024, self->conversion_time);
  //static void uv_sensor_apply_settings(measurement_mode_t mode, standby_bit_t standby, uint8_t break_time,
  //            uint8_t gain, internal_clock_t internal_clock, creg1_time_t conversion_time)
  //...
  // Check statuses?

  return return_code;
}

/*!
 * Command the sensor to reset
 */
static void _uv_sensor_reset(void)
{
  uint8_t reset_command = RESET_BIT;

  uv_generic_i2c_write(AS7331_AGEN, &reset_command, 1);
}

/*!
 * Get the UV readings from the chip (we don't care about temp)
 */
static esp_err_t  _uv_sensor_get_readings(UV_converted_values* return_data)
{
  esp_err_t return_code = ESP_OK;
  UV_adc_raw_values raw_counts = {0};
  UV_converted_values converted_vals;
  // sensitivities at 1.024 MHz clock -- units = uW/cm^2
  float lsbA = 304.69f / ((float)(1 << (11 - self->gain))) / ((float)(1 << self->conversion_time)/1024.0f) / 1000.0f;
  float lsbB = 398.44f / ((float)(1 << (11 - self->gain))) / ((float)(1 << self->conversion_time)/1024.0f) / 1000.0f;
  float lsbC = 191.41f / ((float)(1 << (11 - self->gain))) / ((float)(1 << self->conversion_time)/1024.0f) / 1000.0f;

  uint8_t OSR_reg_bits = 0;
  uv_generic_i2c_read(AS7331_OSR, &OSR_reg_bits, 1);

  // Set the command mode bit
  OSR_reg_bits |= (1 << 7);
  uv_generic_i2c_write(AS7331_OSR, &OSR_reg_bits, 1);

  vTaskDelay((self->delay_period / portTICK_PERIOD_MS) + 1);

  // Get the sensor readings. Passing AS7331_MRES1 with a larger size will cause the sensor to enumerate
  // through the next registers until recieving a stop bit
  uv_generic_i2c_read(AS7331_MRES1, ((uint8_t*)&raw_counts), sizeof(UV_adc_raw_values));

  // Check that there are no internal errors
  uv_sensor_get_status();

  // Copy to internal struct storage
  memcpy(&(self->raw_counts), &raw_counts, sizeof(UV_adc_raw_values));

  // Convert the raw counts to measurements
  converted_vals.UV_A = raw_counts.UV_A * lsbA;
  converted_vals.UV_B = raw_counts.UV_B * lsbB;
  converted_vals.UV_C = raw_counts.UV_C * lsbC;

  // Copy results to internal struct storage
  memcpy(&(self->converted_vals), &converted_vals, sizeof(UV_converted_values));

  // Copy to the return buffer
  memcpy(return_data, &converted_vals, sizeof(UV_converted_values));

  return return_code;
}

/*!
 * Get the ID of the chip
 */
static uint8_t uv_sensor_get_id(void)
{
  uint8_t chip_id = 0;

  uv_generic_i2c_read(AS7331_AGEN, &chip_id, 1);

  return chip_id;
}

/*!
 * Get the chip status -- indicates sampling errors
 */
static void uv_sensor_get_status(void)
{
  uint16_t status = 0;

  uv_generic_i2c_read(AS7331_STATUS, ((uint8_t*)&status), 2);

  if (status & OUTCONVOF) {
    ESP_LOGE(UV_TAG, "Overflow of internal time reference.");
  }

  if (status & MRESOF) {
    ESP_LOGE(UV_TAG, "Overflow of measurement register(s).");
  } 

  if (status & ADCOF) {
    ESP_LOGE(UV_TAG, "Overflow of internal conversion channel(s).");
  }

  if (status & LDATA) {
    ESP_LOGE(UV_TAG, "Measurement results overwritten.");
  }

  if (status & NOTREADY) {
    ESP_LOGE(UV_TAG, "Measurement in progress.");
  }
}

/*!
 * Apply the desired settings to the chip
 */
static void uv_sensor_apply_settings(measurement_mode_t mode, standby_bit_t standby, uint8_t break_time,
              uint8_t gain, internal_clock_t internal_clock, creg1_time_t conversion_time)
{
  uint8_t creg1_bits = ((mode << 6) | (gain << 4) | conversion_time);
  uint8_t creg3_bits = ((mode << 6) | (standby << 4) | internal_clock);

  uv_generic_i2c_write(AS7331_CREG1, &creg1_bits, 1);
  uv_generic_i2c_write(AS7331_CREG1, &creg3_bits, 1);
}

/*!
 * 
 */
static esp_err_t uv_generic_i2c_write(uint8_t reg_addr, uint8_t* write_data, size_t length)
{
  esp_err_t return_code = ESP_OK;
  int actual_write_length = (length + 1);

  if (actual_write_length > BUFFER_SIZE) {
    ESP_LOGE(UV_TAG, "Write data length exceeds buffer length in uv_generic_i2c_write: Buffer length = %d, requested "
      "read length = %d", BUFFER_SIZE, actual_write_length);
    
    return_code = ESP_FAIL;
    return return_code;
  }

  memset(&(self->write_buffer[0]), 0, BUFFER_SIZE);

  self->write_buffer[0] = reg_addr;

  memcpy(&(self->write_buffer[1]), write_data, length);

  return i2c_master_write_to_device(self->i2c_port_num, self->i2c_device_addr, &(self->write_buffer[0]), 
    actual_write_length, self->i2c_timeout_ticks);
}

static esp_err_t uv_generic_i2c_read(uint8_t reg_addr, uint8_t* return_data, size_t length)
{
  esp_err_t return_code = ESP_OK;

  if (length > BUFFER_SIZE) {
    ESP_LOGE(UV_TAG, "Read data length exceeds buffer length in uv_generic_i2c_read: Buffer length = %d, requested "
      "read length = %d", BUFFER_SIZE, (int)length);
    
    return_code = ESP_FAIL;
    return return_code;
  }

  memset(&(self->read_buffer[0]), 0, BUFFER_SIZE);

  return_code = i2c_master_write_read_device(self->i2c_port_num, self->i2c_device_addr, &reg_addr, 1,
          return_data, length, self->i2c_timeout_ticks);

  // Copy over the data to the internal struct buffer
  memcpy(&(self->read_buffer[0]), return_data, length);
  
  return return_code;
}
