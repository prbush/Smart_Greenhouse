#ifndef ENVIRONMENTAL_SENSOR_H
#define ENVIRONMENTAL_SENSOR_H

#include <stdint.h>
#include "bme280.h"
#include "esp_err.h"
#include "driver/i2c.h"

#define BME_280_I2C_ADDR 0x77

typedef struct Env_sensor_readings {
  int32_t temperature;
  int32_t pressure;
  int32_t humidity;
} Env_sensor_readings;

typedef struct Environmental_sensor {
  struct bme280_dev bme_dev_struct;
  struct bme280_settings bme_settings_struct;

  struct bme280_data compensated_readings;

  i2c_port_t i2c_port_num;
  uint32_t i2c_timeout_ticks;
  uint8_t i2c_device_addr;

  struct bme280_data (*get_readings)(void);

} Environmental_sensor;

esp_err_t enviromental_sensor_init(Environmental_sensor *struct_ptr, uint32_t timeout_ticks, i2c_port_t i2c_port_num);

#endif /* ENVIRONMENTAL_SENSOR_H */