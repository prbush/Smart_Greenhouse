
#include "environmental_sensor.h"
#include "bme280.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

static Environmental_sensor *self;

static const char *BME_TAG = "BME280";

void BME280_delay_usec(uint32_t msec, void *intf_ptr);
BME280_INTF_RET_TYPE bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);
static void bme280_error_codes_print_result(const char *bme_fn_name, int8_t rslt);

static Env_sensor_readings _environmental_sensor_get_readings(void);

esp_err_t enviromental_sensor_init(Environmental_sensor *struct_ptr, uint32_t timeout)
{
  esp_err_t return_code = ESP_OK;
  BME280_INTF_RET_TYPE bme_return = BME280_OK;
  self = struct_ptr;

  self->i2c_timeout = timeout;
  self->i2c_addr = BME_280_I2C_ADDR;

  self->bme_dev_struct.intf = BME280_I2C_INTF;
  self->bme_dev_struct.write = bme280_i2c_write;
  self->bme_dev_struct.read = bme280_i2c_read;
  self->bme_dev_struct.intf_ptr = &(self->i2c_addr);
  self->bme_dev_struct.delay_us = BME280_delay_usec;

  self->get_readings = _environmental_sensor_get_readings;

  bme_return = bme280_init(&(self->bme_dev_struct));
  bme280_error_codes_print_result("bme280_init", bme_return);

  if (bme_return != BME280_OK) {
    return_code = ESP_FAIL;
  }

  return return_code;
}

void BME280_delay_usec(uint32_t usec, void *intf_ptr)
{
	vTaskDelay((usec / 1000)/portTICK_PERIOD_MS);
}

/*!
 * I2C read function
 */
BME280_INTF_RET_TYPE bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
  // return coines_read_i2c(COINES_I2C_BUS_0, dev_addr, reg_addr, reg_data, (uint16_t)length);
  return ESP_OK;
}

/*!
 * I2C write function
 */
BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
  // return coines_write_i2c(COINES_I2C_BUS_0, dev_addr, reg_addr, (uint8_t *)reg_data, (uint16_t)length);
  return ESP_OK;
}

static Env_sensor_readings _environmental_sensor_get_readings(void)
{
  Env_sensor_readings return_vals = {0};

  return return_vals;
}

static void bme280_error_codes_print_result(const char *bme_fn_name, int8_t rslt)
{
  if (rslt != BME280_OK)
  {
    switch (rslt)
    {
      case BME280_E_NULL_PTR:
        ESP_LOGE(BME_TAG, "Error [%d] : Null pointer error. It occurs when the user tries to assign value "
          "(not address) to a pointer, which has been initialized to NULL.", rslt);
        break;

      case BME280_E_COMM_FAIL:
        ESP_LOGE(BME_TAG, "Error [%d] : Communication failure error. It occurs due to read/write operation "
          "failure and also due to power failure during communication", rslt);
        break;

      case BME280_E_DEV_NOT_FOUND:
        ESP_LOGE(BME_TAG, "Error [%d] : Device not found error. It occurs when the device chip id is incorrectly read.",
          rslt);
        break;

      case BME280_E_INVALID_LEN:
        ESP_LOGE(BME_TAG, "Error [%d] : Invalid length error. It occurs when write is done with invalid length.", 
          rslt);
        break;

      default:
        ESP_LOGE(BME_TAG, "Error [%d] : Unknown error code.", rslt);
        break;
    }
  }
}