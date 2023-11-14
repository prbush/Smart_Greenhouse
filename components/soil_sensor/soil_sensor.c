#include <stdio.h>
#include "soc/soc_caps.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "soil_sensor.h"

// Static private object pointer
static Soil_sensor *self;

// Logger tag
const char *SOIL_TAG = "Soil Sensor";

// Private functions
static esp_err_t adc_calibration_init(void);

// Public functions privided via struct fn pointers
static uint16_t _soil_sensor_get_readings(void);

/*!
 * Public init function
 */
esp_err_t soil_sensor_init(Soil_sensor *struct_ptr, adc_unit_t adc_unit, adc_channel_t adc_channel, 
  adc_atten_t atten)
{
  esp_err_t return_code;

  // Assign private object pointer 
  self = struct_ptr;

  // Assign struct fields
  self->init_config.clk_src = 0;   // Use default clock source
  self->init_config.ulp_mode = ADC_ULP_MODE_DISABLE;
  self->init_config.unit_id = adc_unit;
  self->channel_config.atten = atten;
  self->channel_config.bitwidth = ADC_BITWIDTH_12;
  self->adc_channel = adc_channel;
  // Function pointer
  self->get_reading = _soil_sensor_get_readings;
  // Tested min/max values
  self->soil_min_val = SOIL_DRY_COUNTS;
  self->soil_max_val = SOIL_SATURATED_COUNTS;

  // Set up the ADC unit as one-shot
  return_code = adc_oneshot_new_unit(&(self->init_config), &(self->adc_handle));
  if (return_code != ESP_OK) {
    ESP_LOGE(SOIL_TAG, "Failed to configure soil sensor ADC channel bit width.");
    return return_code;
  }
  
  // Set up the ADC channel
  return_code = adc_oneshot_config_channel(self->adc_handle, self->adc_channel, 
                  &(self->channel_config));
  if (return_code != ESP_OK) {
    ESP_LOGE(SOIL_TAG, "Failed to configure soil sensor ADC channel attenuation.");
    return return_code;
  }

  // Calibrate for the offset to Vref written to the eFuse
  return_code = adc_calibration_init();
  if (return_code == ESP_OK) {
      ESP_LOGI(SOIL_TAG, "ADC calibration success");
  } else if (return_code == ESP_ERR_NOT_SUPPORTED || !self->is_calibrated) {
      ESP_LOGW(SOIL_TAG, "ADC eFuse not burnt, skip software calibration");
      return return_code;
  } else {
      ESP_LOGE(SOIL_TAG, "Invalid arg or no memory in soil_sensor_init()");
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

  adc_oneshot_get_calibrated_result(self->adc_handle, self->calibration_handle, self->adc_channel,
    &adc_raw_count);

  // TODO: map the raw counts to 0-100%

  return (uint16_t) adc_raw_count;
}


/*!
 * Calibrate the ADC
 */
static esp_err_t adc_calibration_init(void)
{
  esp_err_t return_code = ESP_FAIL;
  // We'll use curve fitting as it is more accurate and supported by our chip
  adc_cali_curve_fitting_config_t cali_config;   // This is a throw away struct

  // Only calibrate once
  if (!self->is_calibrated) {
    cali_config.unit_id = self->init_config.unit_id;
    cali_config.chan = self->adc_channel;
    cali_config.atten = self->channel_config.atten;
    cali_config.bitwidth = self->channel_config.bitwidth;
    // Second arg is return parameter -- self->calibration_handle will be written to
    return_code = adc_cali_create_scheme_curve_fitting(&cali_config, &(self->calibration_handle));
    
    if (return_code == ESP_OK) {
      self->is_calibrated = true;
    }
  }

  return return_code;
}