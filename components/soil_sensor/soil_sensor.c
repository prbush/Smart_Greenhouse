#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "soil_sensor.h"

// Static private object pointer
static Soil_sensor *self;

// Logger tag
const char *SOIL_TAG = "Soil Sensor";

// Private functions
static bool adc_calibration_init(void);

// Public functions
static uint16_t _soil_sensor_get_readings(void);

/*!
 * Public init function
 */
esp_err_t soil_sensor_init(Soil_sensor *struct_ptr, adc_channel_t adc_channel, adc_atten_t atten,
  uint32_t soil_dry_value, uint32_t soil_saturated_value)
{
  esp_err_t return_code;
  bool is_calibrated = false;

  // Assign private object pointer 
  self = struct_ptr;

  // Assign struct fields
  // ...
  self->adc_channel = adc_channel;
  self->attenuation = atten;
  self->adc_bitwidth = ADC_BITWIDTH_12;
  self->get_reading = _soil_sensor_get_readings;
  // TODO: Identify the soil min and max values
  self->soil_min_val = soil_dry_value;
  self->soil_max_val = soil_saturated_value;

  // Calibrate for the offset to Vref written to the eFuse
  is_calibrated = adc_calibration_init();

  return_code = adc1_config_width(self->adc_bitwidth);

  if (return_code != ESP_OK) {
    ESP_LOGE(SOIL_TAG, "Failed to configure soil sensor ADC channel bit width.");
    return return_code;
  }
  
  return_code = adc1_config_channel_atten(self->adc_channel, self->attenuation);
  
  if (return_code != ESP_OK) {
    ESP_LOGE(SOIL_TAG, "Failed to configure soil sensor ADC channel attenuation.");
    return return_code;
  }

  return return_code;
}


/*!
 * Read the ADC as one-shot
 */
static uint16_t _soil_sensor_get_readings(void)
{
  int adc_raw_count = 0;

  adc_raw_count = (self->adc_channel);

  return (uint16_t) adc_raw_count;
}


/*!
 * Calibrate the ADC
 */
static bool adc_calibration_init(void)
{
    esp_err_t ret;
    bool cali_enable = false;

    ret = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP_FIT);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(SOIL_TAG, "Calibration scheme not supported, skip software"
          "calibration");
    } else if (ret == ESP_ERR_INVALID_VERSION) {
        ESP_LOGW(SOIL_TAG, "eFuse not burnt, skip software calibration");
    } else if (ret == ESP_OK) {
        cali_enable = true;
        esp_adc_cal_characterize(ADC_UNIT_1, self->attenuation, 
          self->adc_bitwidth, 0, &(self->cal));
    } else {
        ESP_LOGE(SOIL_TAG, "Invalid arg");
    }

    return cali_enable;
}