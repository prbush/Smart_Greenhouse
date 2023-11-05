#ifndef UV_SENSOR_H
#define UV_SENSOR_H

#include <stdint.h>
#include "bme280.h"
#include "esp_err.h"
#include "driver/i2c.h"

// Configuration State registers
#define AS7331_OSR                      0x00
#define AS7331_AGEN                     0x02  // should be 0x21
#define AS7331_CREG1                    0x06   
#define AS7331_CREG2                    0x07
#define AS7331_CREG3                    0x08
#define AS7331_BREAK                    0x09
#define AS7331_EDGES                    0x0A
#define AS7331_OPTREG                   0x0B
// Measurement State registers
#define AS7331_STATUS                   0x00
#define AS7331_TEMP                     0x01
#define AS7331_MRES1                    0x02
#define AS7331_MRES2                    0x03
#define AS7331_MRES3                    0x04
#define AS7331_OUTCONVL                 0x05
#define AS7331_OUTCONVH                 0x06
// I2C Address
#define AS7331_ADDRESS  0x74

// Measurement modes -- we'll use CMD (forced) mode
typedef enum measurement_mode{
  AS7331_CONT_MODE                = 0x00,
  AS7331_CMD_MODE                 = 0x01,
  AS7331_SYNS_MODE                = 0x02,
  AS7331_SYND_MODE                = 0x03
} measurement_mode_t;

// Internal clock frequency, 1.024 MHz, etc
typedef enum internal_clock {
  AS7331_1024           = 0x00, 
  AS7331_2048           = 0x01,
  AS7331_4096           = 0x02,
  AS7331_8192           = 0x03
} internal_clock_t;

typedef struct UV_values{
  uint16_t UV_A;
  uint16_t UV_B;
  uint16_t UV_C;

} UV_values;

typedef struct UV_sensor{
  UV_values raw_counts;
  UV_values converted_vals;

  uint8_t   (*get_id)(void);
  void      (*reset)(void);
  esp_err_t (*get_readings)(void);
} UV_sensor;

esp_err_t uv_sensor_init(UV_sensor *struct_ptr, uint32_t timeout_ticks, i2c_port_t i2c_port_num);

#endif /* UV_SENSOR_H */