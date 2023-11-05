
#include "environmental_sensor.h"
#include "bme280.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "string.h"
#include "sdkconfig.h"

// Static private object pointer
static Environmental_sensor *self;

// Logger tag
static const char *BME_TAG = "BME280";

// BME280 lib porting functions
void                      BME280_delay_usec(uint32_t msec, void *intf_ptr);
BME280_INTF_RET_TYPE      bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
BME280_INTF_RET_TYPE      bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);

// Helper functions
static void               bme280_error_codes_print_result(const char *bme_fn_name, int8_t rslt);

// Private functions
static esp_err_t          _environmental_sensor_get_readings(struct bme280_data* return_data);



// Public init function
esp_err_t enviromental_sensor_init(Environmental_sensor *struct_ptr, uint32_t timeout_ticks, i2c_port_t i2c_port_num)
{
  esp_err_t return_code = ESP_OK;
  BME280_INTF_RET_TYPE bme_return = BME280_OK;
  // Assign private object pointer
  self = struct_ptr;
  // Assign struct fields
  self->i2c_port_num = i2c_port_num;
  self->i2c_timeout_ticks = timeout_ticks;
  self->i2c_device_addr = BME_280_I2C_ADDR;
  self->get_readings = _environmental_sensor_get_readings;
  // Assign bme280_dev struct fields
  self->bme_dev_struct.intf = BME280_I2C_INTF;
  self->bme_dev_struct.write = bme280_i2c_write;
  self->bme_dev_struct.read = bme280_i2c_read;
  self->bme_dev_struct.intf_ptr = &(self->i2c_device_addr);
  self->bme_dev_struct.delay_us = BME280_delay_usec;

  // Initialize the BME280
  bme_return = bme280_init(&(self->bme_dev_struct));
  bme280_error_codes_print_result("bme280_init", bme_return);

  // Must first get the settings before we can change them
  bme_return = bme280_get_sensor_settings(&(self->bme_settings_struct), &(self->bme_dev_struct));
  bme280_error_codes_print_result("bme280_init", bme_return);
  
  // Assign bme280_settings struct fields
  // Using recommended "indoor navigation" settings for low noise readings @ 25Hz
  self->bme_settings_struct.osr_p = BME280_OVERSAMPLING_16X;
  self->bme_settings_struct.osr_h = BME280_OVERSAMPLING_1X;
  self->bme_settings_struct.osr_t = BME280_OVERSAMPLING_2X;
  self->bme_settings_struct.filter = BME280_FILTER_COEFF_16;

  // Set BME280 settings
  bme_return = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &(self->bme_settings_struct), 
    &(self->bme_dev_struct));
  bme280_error_codes_print_result("bme280_set_sensor_settings", bme_return);

  // Get the delay period
  // We'll run @25Hz, so could just set this to 40000 (time in uSec)
  bme_return = bme280_cal_meas_delay(&(self->delay_period), &(self->bme_settings_struct));
  bme280_error_codes_print_result("bme280_cal_meas_delay", bme_return);

  // Set the BME280 sensor mode to Forced (polling) mode
  bme_return = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &(self->bme_dev_struct));
  bme280_error_codes_print_result("bme280_set_sensor_mode", bme_return);


  if (bme_return != BME280_OK) {
    return_code = ESP_FAIL;
  }

  return return_code;
}

void BME280_delay_usec(uint32_t usec, void *intf_ptr)
{
  // Convert from uSec to mSec
	vTaskDelay(((usec / 1000) / portTICK_PERIOD_MS) + 1);
}

/*!
 * I2C read function
 */
BME280_INTF_RET_TYPE bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{

  if (length > BUFFER_SIZE) {
    ESP_LOGE(BME_TAG, "Read data length exceeds buffer length in bme280_i2c_read: Buffer length = %d, requested "
      "read length = %lu", BUFFER_SIZE, length);
    
    return BME280_E_INVALID_LEN;
  }

  memset(&(self->read_buffer[0]), 0, BUFFER_SIZE);

  return i2c_master_write_read_device(self->i2c_port_num, self->i2c_device_addr, &reg_addr, 1,
          &(self->read_buffer[0]), length, self->i2c_timeout_ticks);
}

/*
static esp_err_t mpu9250_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, MPU9250_SENSOR_ADDR, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

*/

/*!
 * I2C write function
 */
BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
  int actual_write_length = (length + 1);

  if (actual_write_length > BUFFER_SIZE) {
    ESP_LOGE(BME_TAG, "Write data length exceeds buffer length in bme280_i2c_write: Buffer length = %d, requested "
      "read length = %d", BUFFER_SIZE, actual_write_length);
    
    return BME280_E_INVALID_LEN;
  }

  memset(&(self->write_buffer[0]), 0, BUFFER_SIZE);

  self->write_buffer[0] = reg_addr;

  memcpy(&(self->write_buffer[1]), reg_data, length);

  return i2c_master_write_to_device(self->i2c_port_num, self->i2c_device_addr, &(self->write_buffer[0]), 
    actual_write_length, self->i2c_timeout_ticks);
}

static void bme280_error_codes_print_result(const char *bme_fn_name, int8_t rslt)
{
  if (rslt != BME280_OK)
  {
    switch (rslt)
    {
      case BME280_E_NULL_PTR:
        ESP_LOGE(BME_TAG, "%s() returned error [%d] : Null pointer error. It occurs when the user tries to assign value "
          "(not address) to a pointer, which has been initialized to NULL.", bme_fn_name, rslt);
        break;

      case BME280_E_COMM_FAIL:
        ESP_LOGE(BME_TAG, "%s() returned error [%d] : Communication failure error. It occurs due to read/write operation "
          "failure and also due to power failure during communication", bme_fn_name, rslt);
        break;

      case BME280_E_DEV_NOT_FOUND:
        ESP_LOGE(BME_TAG, "%s() returned error [%d] : Device not found error. It occurs when the device chip id is "
          "incorrectly read.", bme_fn_name, rslt);
        break;

      case BME280_E_INVALID_LEN:
        ESP_LOGE(BME_TAG, "%s() returned error [%d] : Invalid length error. It occurs when write is done with invalid "
          "length.", bme_fn_name, rslt);
        break;

      default:
        ESP_LOGE(BME_TAG, "%s() returned error [%d] : Unknown error code.", bme_fn_name, rslt);
        break;
    }
  }
}

static esp_err_t _environmental_sensor_get_readings(struct bme280_data* return_data)
{
  esp_err_t return_code = ESP_OK;
  BME280_INTF_RET_TYPE bme_return = BME280_OK;

  // Set the sensor to forced mode
  // Set the BME280 sensor mode to Forced (polling) mode
  bme_return = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &(self->bme_dev_struct));
  bme280_error_codes_print_result("bme280_set_sensor_mode", bme_return);
  if (bme_return != BME280_OK) {
    return_code = ESP_FAIL;
    return return_code;
  }

  // Delay until the measurement is done
  self->bme_dev_struct.delay_us(self->delay_period, self->bme_dev_struct.intf_ptr);
  // Write the results to the struct
  bme_return = bme280_get_sensor_data(BME280_ALL, &(self->compensated_readings), &(self->bme_dev_struct));
  bme280_error_codes_print_result("bme280_get_sensor_data", bme_return);
  if (bme_return != BME280_OK) {
    return_code = ESP_FAIL;
    return return_code;
  }
  
  // Copy the results to the return struct
  memcpy(return_data, &(self->compensated_readings), sizeof(struct bme280_data));
  return return_code;
}