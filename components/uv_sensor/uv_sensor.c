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

// Private functions
static uint8_t    uv_sensor_get_id(void);
static void       uv_sensor_get_status(void);
static esp_err_t  uv_sensor_apply_settings(measurement_mode_t mode, standby_bit_t standby, uint8_t break_time,
                    as7331_gain_t gain, internal_clock_t internal_clock, integration_time_t conversion_time);
static esp_err_t  uv_generic_i2c_write(uint8_t reg_addr, uint8_t* write_data, size_t length);
static esp_err_t  uv_generic_i2c_read(uint8_t reg_addr, uint8_t* return_data, size_t length);

// Public functions
static void       _uv_sensor_power_on(void);
static void       _uv_sensor_reset(void);
static esp_err_t  _uv_sensor_get_readings(UV_converted_values* return_data);


/*!
 * Public init function
 */
esp_err_t uv_sensor_init(UV_sensor *struct_ptr, i2c_port_t i2c_port_num, as7331_gain_t gain, 
  integration_time_t time)
{
  esp_err_t return_code = ESP_OK;
  uint8_t chip_id = 0;

  // Assign private object pointer
  self = struct_ptr;

  // Assign struct fields
  self->i2c_port_num = i2c_port_num;
  self->delay_period = (1 << time); // required delay in ms
  self->i2c_timeout_ticks = (self->delay_period / portTICK_PERIOD_MS) + 1;
  self->i2c_device_addr = AS7331_ADDRESS;

  // See if Gain 32x and time 6 result in the same measurements as default
  self->gain = gain;
  self->conversion_time = time;
  self->power_on        = _uv_sensor_power_on;
  self->reset           = _uv_sensor_reset;
  self->get_readings    = _uv_sensor_get_readings;
  
  // Reset the chip to start fresh
  self->reset();

  // Power the chip on
  self->power_on();

  // Check the chip ID
  chip_id = uv_sensor_get_id();
    if (chip_id != AS7331_ID) {
    ESP_LOGE(UV_TAG, "Chip ID did not match default ID. Got %u, should be %u.", chip_id, AS7331_ID);
    return_code = ESP_FAIL;
    return return_code;
  }

  // Apply settings
  return_code = uv_sensor_apply_settings(AS7331_CMD_MODE, STDBY_OFF, 0, self->gain, AS7331_1024, self->conversion_time);

  return return_code;
}

/*!
 * Command the sensor to reset
 */
static void _uv_sensor_reset(void)
{
  uint8_t reset_command = RESET_BIT;

  uv_generic_i2c_write(AS7331_AGEN, &reset_command, 1);
  // Takes 3 ms to power up and initialize
  vTaskDelay(portTICK_PERIOD_MS / 3);
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
  float lsb_a = 20.75; // nW/cm^2
  float lsb_b = 23.07; // nW/cm^2
  float lsb_c = 10.13;  // nW/cm^2

  // Tell the sensor to start a measurement
  uint8_t OSR_reg_bits = 0x83;
  uv_generic_i2c_write(AS7331_OSR, &OSR_reg_bits, 1);

  vTaskDelay(self->i2c_timeout_ticks + 2);

  // Get the sensor readings. Passing AS7331_MRES1 with a larger size will cause the sensor to enumerate
  // through the next registers until recieving a stop bit
  uv_generic_i2c_read(AS7331_MRES1, ((uint8_t*)&raw_counts), sizeof(UV_adc_raw_values));

  // ESP_LOGI(UV_TAG, "Raw counts --> UV A: %hi, UV B: %hi, UV C: %hi", raw_counts.UV_A, raw_counts.UV_B, raw_counts.UV_C);

  // Check that there are no internal errors
  uv_sensor_get_status();

  // Copy to internal struct storage
  memcpy(&(self->raw_counts), &raw_counts, sizeof(UV_adc_raw_values));

  // Convert the raw counts to measurements, and from nW/cm^2 to uW/cm^2
  converted_vals.UV_A = (raw_counts.UV_A == 0) ? 0.0 : ((raw_counts.UV_A * lsb_a) / 1000);
  converted_vals.UV_B = (raw_counts.UV_B == 0) ? 0.0 : ((raw_counts.UV_B * lsb_b) / 1000);
  converted_vals.UV_C = (raw_counts.UV_C == 0) ? 0.0 : ((raw_counts.UV_C * lsb_c) / 1000);

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

static void _uv_sensor_power_on(void){
  uint8_t osr_bits = 0x02;
  uv_generic_i2c_write(AS7331_OSR, &osr_bits, 1);
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
static esp_err_t uv_sensor_apply_settings(measurement_mode_t mode, standby_bit_t standby, uint8_t break_time,
              as7331_gain_t gain, internal_clock_t internal_clock, integration_time_t conversion_time)
{
  esp_err_t return_code = ESP_OK;
  uint8_t creg1_bits = ((gain << 4) | conversion_time);
  uint8_t creg2_bits = 0x00;
  uint8_t creg3_bits = ((mode << 6) | (standby << 4) | internal_clock);
  uint8_t osr_bits = 0x03;
  uint8_t validate_reg = 0;

  // Coming out of reset, the sensor will be in congifuration mode
  self->reset();
  vTaskDelay(1);
  // Write and verify the control registers
  uv_generic_i2c_write(AS7331_CREG1, &creg1_bits, 1);
  uv_generic_i2c_read(AS7331_CREG1, &validate_reg, 1);
  if (validate_reg != creg1_bits) {
    ESP_LOGE(UV_TAG, "CREG1 register setting did not stick.");
    return_code = ESP_FAIL;
  }

  vTaskDelay(1);

  uv_generic_i2c_write(AS7331_CREG2, &creg2_bits, 1);
  uv_generic_i2c_read(AS7331_CREG2, &validate_reg, 1);
  if (validate_reg != creg2_bits) {
    ESP_LOGE(UV_TAG, "CREG2 register setting did not stick.");
    return_code = ESP_FAIL;
  }

  vTaskDelay(1);

  uv_generic_i2c_write(AS7331_CREG3, &creg3_bits, 1);
  uv_generic_i2c_read(AS7331_CREG3, &validate_reg, 1);
  if (validate_reg != creg3_bits) {
    ESP_LOGE(UV_TAG, "CREG3 register setting did not stick.");
  }

  // Set the OSR reg to be in the measurement state vs config state
  uv_generic_i2c_write(AS7331_OSR, &osr_bits, 1);
  uv_generic_i2c_read(AS7331_OSR, &validate_reg, 1);
  if (validate_reg != osr_bits) {
    ESP_LOGE(UV_TAG, "OSR register setting did not stick.");
    return_code = ESP_FAIL;
  }

  return return_code;
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
