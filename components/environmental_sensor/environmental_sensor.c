
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

esp_err_t enviromental_sensor_init(Environmental_sensor *struct_ptr, uint32_t timeout_ticks, i2c_port_t i2c_port_num)
{
  esp_err_t return_code = ESP_OK;
  BME280_INTF_RET_TYPE bme_return = BME280_OK;
  self = struct_ptr;

  self->i2c_port_num = i2c_port_num;
  self->i2c_timeout_ticks = timeout_ticks;
  self->i2c_device_addr = BME_280_I2C_ADDR;

  self->bme_dev_struct.intf = BME280_I2C_INTF;
  self->bme_dev_struct.write = bme280_i2c_write;
  self->bme_dev_struct.read = bme280_i2c_read;
  self->bme_dev_struct.intf_ptr = &(self->i2c_device_addr);
  self->bme_dev_struct.delay_us = BME280_delay_usec;

  self->get_readings = _environmental_sensor_get_readings;

  bme_return = bme280_init(&(self->bme_dev_struct));
  bme280_error_codes_print_result("bme280_init", bme_return);

  self->bme_settings_struct.osr_p = BME280_OVERSAMPLING_1X;
  self->bme_settings_struct.osr_h = BME280_OVERSAMPLING_1X;
  self->bme_settings_struct.osr_t = BME280_OVERSAMPLING_1X;
  self->bme_settings_struct.filter = BME280_FILTER_COEFF_OFF;
  self->bme_settings_struct.standby_time = BME280_STANDBY_TIME_0_5_MS;

  bme_return = bme280_set_sensor_settings((BME280_SEL_OSR_PRESS | BME280_SEL_OSR_TEMP | BME280_SEL_OSR_HUM |
    BME280_SEL_FILTER | BME280_SEL_STANDBY), &(self->bme_settings_struct), &(self->bme_dev_struct));
  bme280_error_codes_print_result("bme280_set_sensor_settings", bme_return);

  bme_return = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &(self->bme_dev_struct));
  bme280_error_codes_print_result("bme280_set_sensor_mode", bme_return);


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
  return i2c_master_read_from_device(self->i2c_port_num, self->i2c_device_addr, reg_data, length, 
    self->i2c_timeout_ticks);
}

/*!
 * I2C write function
 */
BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
  return i2c_master_write_to_device(self->i2c_port_num, self->i2c_device_addr, reg_data, length, 
    self->i2c_timeout_ticks);
}

static Env_sensor_readings _environmental_sensor_get_readings(void)
{
  Env_sensor_readings return_vals = {0};

  /*
  int8_t bme280_get_sensor_data(uint8_t sensor_comp, struct bme280_data *comp_data, struct bme280_dev *dev);
  
  log results

  int8_t bme280_compensate_data(uint8_t sensor_comp,
                              const struct bme280_uncomp_data *uncomp_data,
                              struct bme280_data *comp_data,
                              struct bme280_calib_data *calib_data);

  log results

  return results
  */
  return return_vals;
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