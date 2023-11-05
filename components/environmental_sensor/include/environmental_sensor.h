#ifndef ENVIRONMENTAL_SENSOR_H
#define ENVIRONMENTAL_SENSOR_H

#include <stdint.h>
#include "bme280.h"
#include "esp_err.h"
#include "driver/i2c.h"

#define BME_280_I2C_ADDR 0x77

#define BUFFER_SIZE 128

typedef struct Environmental_sensor {
  struct bme280_dev bme_dev_struct;
  struct bme280_settings bme_settings_struct;

  struct bme280_data compensated_readings;

  i2c_port_t i2c_port_num;
  uint32_t i2c_timeout_ticks;
  uint32_t delay_period;
  uint8_t i2c_device_addr;

  uint8_t read_buffer[BUFFER_SIZE];
  uint8_t write_buffer[BUFFER_SIZE];

  esp_err_t (*get_readings)(struct bme280_data* return_data);

} Environmental_sensor;

esp_err_t enviromental_sensor_init(Environmental_sensor *struct_ptr, uint32_t timeout_ticks, i2c_port_t i2c_port_num);

#endif /* ENVIRONMENTAL_SENSOR_H */