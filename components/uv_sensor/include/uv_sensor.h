#ifndef UV_SENSOR_H
#define UV_SENSOR_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

// Configuration State registers
#define AS7331_OSR                      0x00
#define AS7331_AGEN                     0x02
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
// Special purpose bits
#define RESET_BIT                       (1 << 3)
#define OUTCONVOF                       (1 << 7)
#define MRESOF                          (1 << 6)
#define ADCOF                           (1 << 5)
#define LDATA                           (1 << 4)
#define NOTREADY                        (1 << 2)
// I2C Address
#define AS7331_ADDRESS  0x74
// Default Chip ID
#define AS7331_ID       0x21
// Settings values
#define GAIN_8X         0x08

#define BUFFER_SIZE 128

// Measurement modes -- we'll use CMD (forced) mode
typedef enum measurement_mode{
  AS7331_CONT_MODE                = 0x00,
  AS7331_CMD_MODE                 = 0x01,
  AS7331_SYNS_MODE                = 0x02,
  AS7331_SYND_MODE                = 0x03
} measurement_mode_t;

// Conversion time -- all times in ms
typedef enum creg1_time {
  _1      = 0b0000,
  _2      = 0b0001,
  _4      = 0b0010,
  _8      = 0b0011,
  _16     = 0b0100,
  _32     = 0b0101,
  _64     = 0b0110,
  _128    = 0b0111,
  _256    = 0b1000,
  _512    = 0b1001,
  _1024   = 0b1010,
  _2048   = 0b1011,
  _4096   = 0b1100,
  _8192   = 0b1101,
  _16384  = 0b1110,
} creg1_time_t;
// Optional standby bit
typedef enum standby_bit {
  STDBY_OFF = 0,
  STDBY_ON  = 1
} standby_bit_t;

// Internal clock frequency, 1.024 MHz, etc
typedef enum internal_clock {
  AS7331_1024           = 0x00, 
  AS7331_2048           = 0x01,
  AS7331_4096           = 0x02,
  AS7331_8192           = 0x03
} internal_clock_t;

typedef struct UV_adc_raw_values{
  uint16_t UV_A;
  uint16_t UV_B;
  uint16_t UV_C;
} UV_adc_raw_values;

typedef struct UV_converted_values{
  float UV_A;
  float UV_B;
  float UV_C;
} UV_converted_values;

typedef struct UV_sensor{
  UV_adc_raw_values raw_counts;
  UV_converted_values converted_vals;

  i2c_port_t i2c_port_num;
  uint32_t i2c_timeout_ticks;
  uint32_t delay_period;
  uint8_t i2c_device_addr;

  uint8_t gain;
  creg1_time_t conversion_time;

  uint8_t read_buffer[BUFFER_SIZE];
  uint8_t write_buffer[BUFFER_SIZE];

  // uint8_t   (*get_id)(void);
  void      (*reset)(void);
  esp_err_t (*get_readings)(UV_converted_values* return_data); 
} UV_sensor;

esp_err_t uv_sensor_init(UV_sensor *struct_ptr, uint32_t timeout_ticks, i2c_port_t i2c_port_num);

#endif /* UV_SENSOR_H */