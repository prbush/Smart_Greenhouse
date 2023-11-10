#ifndef SOIL_SENSOR_H
#define SOIL_SENSOR_H

#include "esp_err.h"
#include "driver/adc.h"

typedef struct Soil_sensor {
  adc_channel_t             adc_channel;
  adc_atten_t               attenuation;
  adc_bitwidth_t            adc_bitwidth;
  
  uint16_t                  (*get_reading)(void);
} Soil_sensor;

esp_err_t soil_sensor_init(Soil_sensor *struct_ptr, adc_channel_t adc_channel, adc_atten_t atten);

#endif /* SOIL_SENSOR_H */