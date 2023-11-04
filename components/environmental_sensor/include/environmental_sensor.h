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

  Env_sensor_readings uncomp_readings;
  Env_sensor_readings comp_readings;

  uint32_t i2c_timeout;
  uint8_t i2c_addr;

  Env_sensor_readings (*get_readings)(void);

} Environmental_sensor;

esp_err_t enviromental_sensor_init(Environmental_sensor *struct_ptr, uint32_t timeout);

#endif /* ENVIRONMENTAL_SENSOR_H */