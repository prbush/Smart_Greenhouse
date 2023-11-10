#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "soil_sensor.h"

// Static private object pointer
static Soil_sensor *self;

// Static ADC objects
adc_cali_handle_t calibration_handle;
adc_oneshot_unit_handle_t adc1_handle;

// Logger tag
const char *SOIL_TAG = "Soil Sensor";

// Private functions
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, 
  adc_cali_handle_t *out_handle);

// Public functions
static uint16_t _soil_sensor_get_readings(void);

/*!
 * Public init function
 */
esp_err_t soil_sensor_init(Soil_sensor *struct_ptr, adc_channel_t adc_channel, adc_atten_t atten)
{
  esp_err_t return_code;
  bool is_calibrated = false;

  // Assign private object pointer 
  self = struct_ptr;

  // Assign struct fields
  // ...
  self->adc_channel = adc_channel;
  self->attenuation = atten;
  self->adc_bitwidth = ADC_BITWIDTH_DEFAULT;
  self->get_reading = _soil_sensor_get_readings;
  // self->oneshot_handle = &adc1_handle;
  // self->calibration_handle = &calibration_handle;


  // Init the ADC as one-shiot
  adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
  };

  return_code = adc_oneshot_new_unit(&init_config1, &adc1_handle);

  if (return_code != ESP_OK) {
    ESP_LOGE(SOIL_TAG, "adc_oneshot_new_unit failed in soil_sensor_init().");
    return return_code;
  }

  // Setup the specific channel as one-shot
  adc_oneshot_chan_cfg_t config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT, // max bitwidth
    .atten = atten,
  };

  return_code = adc_oneshot_config_channel(adc1_handle, adc_channel, &config);

  if (return_code != ESP_OK) {
    ESP_LOGE(SOIL_TAG, "adc_oneshot_config_channel failed in soil_sensor_init().");
    return return_code;
  }

  // Calibrate
  is_calibrated = adc_calibration_init(ADC_UNIT_1, adc_channel, atten, &calibration_handle);

  if (!is_calibrated) {
    ESP_LOGE(SOIL_TAG, "ADC calibration failed.");
    return_code = ESP_FAIL;
  }

  return return_code;
}


/*!
 * Calibrate the ADC
 */
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, 
  adc_cali_handle_t *out_handle)
{
  adc_cali_handle_t handle = NULL;
  esp_err_t return_code = ESP_FAIL;
  bool calibrated = false;

  if (!calibrated) {
    ESP_LOGI(SOIL_TAG, "calibration scheme version is Line Fitting");
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    return_code = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    if (return_code == ESP_OK) {
        calibrated = true;
    }
  }

  *out_handle = handle;
  if (return_code == ESP_OK) {
      ESP_LOGI(SOIL_TAG, "Calibration Success");
  } else if (return_code == ESP_ERR_NOT_SUPPORTED || !calibrated) {
      ESP_LOGW(SOIL_TAG, "eFuse not burnt, skip software calibration");
  } else {
      ESP_LOGE(SOIL_TAG, "Invalid arg or no memory");
  }

  return calibrated;
}

/*!
 * Read the ADC as one-shot
 */
static uint16_t _soil_sensor_get_readings(void)
{
  int adc_raw_count = 0;

  adc_oneshot_read(&adc1_handle, self->adc_channel, &adc_raw_count);

  return (uint16_t) adc_raw_count;
}