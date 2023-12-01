#ifndef SOIL_SENSOR_H
#define SOIL_SENSOR_H

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

/* Measured values for min/max ADC counts
 * Note that we are reading the capacitance of the soil,
 * which will be higher in dry soil vs wet. */ 
#define OPEN_AIR_COUNTS       2725
#define IN_WATER_COUNTS       100
#define SOIL_DRY_COUNTS       2715
#define SOIL_SATURATED_COUNTS 1300

typedef struct Soil_sensor {
  adc_oneshot_unit_handle_t   adc_handle;
  adc_oneshot_unit_init_cfg_t init_config;
  adc_oneshot_chan_cfg_t      channel_config;
  adc_cali_handle_t           calibration_handle;
  adc_channel_t               adc_channel;  // ADC_CHANNEL_7

  uint32_t                    soil_min_val;
  uint32_t                    soil_max_val;

  bool                        is_calibrated;
  
  int                         (*get_reading)(void);
} Soil_sensor;

esp_err_t soil_sensor_init(Soil_sensor *struct_ptr, adc_unit_t adc_unit, adc_channel_t adc_channel, 
  adc_atten_t atten);

#endif /* SOIL_SENSOR_H */