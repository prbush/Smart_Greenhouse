#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "fan.h"
#include "lights.h"
#include "pdlc.h"
#include "sdkconfig.h"
#include "environmental_control.h"

extern struct tm global_start_time_info;
extern time_t global_start_time;

// Static private object pointer
static Environmental_control *self;

// Logger tag
const char* ENVIRONMENTAL_TAG = "Environmental control";

// Private functions
void check_for_env_changes_callback(TimerHandle_t xTimer);
static void manage_lights(void);
static void manage_fans(void);
static void manage_pdlc(void);
bool check_slopes(void);
// Public functions privided via struct fn pointers
static status_data_struct _environmental_control_get_statuses(void);
static void _environmental_control_process_env_data(sensor_data_struct sensor_readings);

esp_err_t environmental_control_init(Environmental_control *struct_ptr, Fan *fan, Lights *lights, PDLC *pdlc)
{
  esp_err_t return_code = ESP_OK;

  self = struct_ptr;
  self->fan = fan;
  self->lights = lights;
  self->pdlc = pdlc;
  self->time_series_ptr = NULL;
  self->time_series_index = 0;
  self->timer_id = ENV_TIMER_ID;
  self->timer_period = ONE_MINUTE;
  self->timer_fires_counter = 0;
  self->is_daylight = false;
  self->timer_running = false;
  self->over_temp = false;
  self->over_humidity = false;
  self->get_statuses = _environmental_control_get_statuses;
  self->process_env_data = _environmental_control_process_env_data;
  self->give_up_time_info = global_start_time_info;

  // Initialize the fan, lights, pdlc
  return_code = fan_init(self->fan, CONFIG_FAN_1_GPIO, CONFIG_FAN_2_GPIO);
  if (return_code != ESP_OK) {
    return return_code;
  }

  lights_init(self->lights, CONFIG_LIGHTS_GPIO);
  if (return_code != ESP_OK) {
    return return_code;
  }

  pdlc_init(self->pdlc, CONFIG_PDLC_GPIO);
  if (return_code != ESP_OK) {
    return return_code;
  }

  // Initialize our timer. Note this won't start until we tell it to
  self->timer_handle = xTimerCreate("ENV timer", self->timer_period * CONFIG_FREERTOS_HZ, pdFALSE, 
                                      &(self->timer_id), check_for_env_changes_callback);

  return return_code;
}

static status_data_struct _environmental_control_get_statuses(void)
{
  status_data_struct statuses = {self->fan->get_state(),
                                self->lights->get_state(),
                                self->pdlc->get_state()};

  return statuses;
}



static void _environmental_control_process_env_data(sensor_data_struct sensor_readings)
{
  bool daylight_time = false;
  // Grab the readings and the current time
  self->current_sensor_data = sensor_readings;

  time(&self->time_now);
  localtime_r(&self->time_now, &self->time_now_info);

  if (CLASS_DEMO) {
    // The lights will be on for 5 mins
    daylight_time = (self->time_now_info.tm_min >= global_start_time_info.tm_min) &&
                    (self->time_now_info.tm_min <= (global_start_time_info.tm_min + 5));
    
    self->is_daylight = daylight_time;

  } else {
    // The lights will be on during the daylight hours
    daylight_time = (self->time_now_info.tm_hour >= DAYLIGHT_START) &&
                    (self->time_now_info.tm_hour < DAYLIGHT_END);
    
    self->is_daylight = daylight_time;
  }
/* if needed:

   uW/cm^2
  --------- * Time = j/cm^2 -- > time will be time between measurements (1 sec)
    1,000

*/

  if (self->is_daylight) {
    // Since we are running the sensors at 1Hz, we can just add the current
    // UV sensor readings to the integral
    self->uv_a_integral += sensor_readings.uv_data.UV_A;
    self->uv_b_integral += sensor_readings.uv_data.UV_B;
    self->uv_c_integral += sensor_readings.uv_data.UV_C;
  } else {
    // Make sure we start fresh for the next daylight period
    self->uv_a_integral = 0;
    self->uv_b_integral = 0;
    self->uv_c_integral = 0;
  }

  // Do the stuff
  manage_lights();
  manage_fans();
  manage_pdlc();
}

void check_for_env_changes_callback(TimerHandle_t xTimer)
{
  ESP_LOGE(ENVIRONMENTAL_TAG, "In timer callback.");

  self->timer_running = false;
  self->time_series_index = 0;
  // Sanity check that another timer didn't magically fire this callback
  if (*(uint32_t*)pvTimerGetTimerID(xTimer) != self->timer_id) {
    ESP_LOGE(ENVIRONMENTAL_TAG, "Timer ID did not match.");
    return;
  }

  /* Simple case first -- if the current temp and humidity are below
   * threshold values, then we can turn the fan off and move on. 
   */
  self->over_temp = self->current_sensor_data.bme280_data.temperature > TEMPERATURE_THRESHOLD;
  self->over_humidity = self->current_sensor_data.bme280_data.humidity > HUMIDITY_THRESHOLD;

  if (!(self->over_temp || self->over_humidity)) {
    // Reset the timer run counter
    self->timer_fires_counter = 0;
    // Turn off the fan
    self->fan->off();
  } else {
    /* Now the more complex case -- we are still over temp/humidity.
    * In this case, we need to see if the slopes are negative. 
    *  -If they are, then we can keep running, up to MAX_TIMER_FIRES to try
    *   to get below threshold values.
    *  -If the slopes are positive, then we cannot correct by using the fan
    *   and should stop trying. 
    */
    bool has_negative_slope = check_slopes();
    if (!has_negative_slope) {
      time(&(self->give_up_time));
      localtime_r(&(self->give_up_time), &(self->give_up_time_info));
      self->timer_fires_counter = 0;

      // Turn the fans on
      self->fan->off();

    } else {
      // We have a negative slope, need to see if we should keep trying
      if (self->timer_fires_counter == MAX_TIMER_FIRES) {
        time(&(self->give_up_time));
        localtime_r(&(self->give_up_time), &(self->give_up_time_info));
        self->timer_fires_counter = 0;

        // Turn the fans on
        self->fan->off();
      } else {
        self->timer_fires_counter++;
      }
    }
  }
  
  // Free the time series array
  free(self->time_series_ptr);
  return;
}

static void manage_lights(void) 
{
  // The lights will be on during daylight hours, off otherwise
  if (self->is_daylight && (self->lights->get_state() != LIGHT_ON)) {
    self->lights->on();
  } else if (!self->is_daylight && (self->lights->get_state() != LIGHT_OFF)) {
    self->lights->off();
  }
}

static void manage_pdlc(void)
{
  bool above_threshold = false;

  if (self->is_daylight) {

    above_threshold = (self->uv_a_integral > UV_A_THRESHOLD) ||
                      (self->uv_b_integral > UV_B_THRESHOLD) ||
                      (self->uv_c_integral > UV_C_THRESHOLD);

    if (above_threshold) {
      if (self->pdlc->get_state() != PDLC_OFF) {
        self->pdlc->off();
      }
    } else {
      if (self->pdlc->get_state() != PDLC_ON) {
        self->pdlc->on();
      }
    }

  } else if (self->pdlc->get_state() != PDLC_OFF) {
    self->pdlc->off();
  }
}

static void manage_fans(void)
{
  /*
  If values are above the threshold:
    1) start the timer
    2) create an array for storing requisite data for a time series
    3) set some status flag that the time series has been created
    4) In subsequent calls, save the sensor data to the time series
      - Need to check indicied to prevent overflow
    5) When the timer has fired, set some status bit to indicate that the time
       series is now expired and should be deleted. 

    Each "thing" will require different logic...
    Fan:
      If temp or humidity is above threshold, turn on
      If the timer fires and the values are below threshold, turn off
      If timer fires and values are above threshold but slope is negative,
        restart the timer and keep going
          -- There needs to be some reasonable cutoff -- some max number of times
             the timer can be set
  */
  if (!self->timer_running) {
    // Check thresholds
    self->over_temp = self->current_sensor_data.bme280_data.temperature > TEMPERATURE_THRESHOLD;
    self->over_humidity = self->current_sensor_data.bme280_data.humidity > HUMIDITY_THRESHOLD;

    if (self->over_temp || self->over_humidity) {
      // See if we should try to correct

      // We'll keep trying if either this is the first time it has run or
      // if an hour has elapsed since the last time we tried
      bool keep_trying = (time(&(self->give_up_time)) == time(&global_start_time)) ||
                         (self->give_up_time_info.tm_hour < self->time_now_info.tm_hour);

      if (keep_trying) {
        // Start the timer
        xTimerStart(self->timer_handle, 1);
        self->timer_running = true;

        // Allocate our array
        self->time_series_ptr = (struct bme280_data*) malloc(sizeof(struct bme280_data) * SAMPLES_PER_MINUTE);
        self->time_series_index = 0;

        // Store the current bme280 data for later comparison
        self->time_series_ptr[self->time_series_index++] = self->current_sensor_data.bme280_data;

        // Turn the fans on
        self->fan->on();
      }
    }

  } else {
    // Store the current sensor data
    if (self->time_series_index > (SAMPLES_PER_MINUTE - 1)) {
      self->time_series_ptr[self->time_series_index++] = self->current_sensor_data.bme280_data;
    }
  }
}

bool check_slopes(void)
{
  // We want to check that the slope over the entire timer period is negative
  self->time_series_index--;
  double start_humidity = self->time_series_ptr[0].humidity;
  double end_humidity   = self->time_series_ptr[self->time_series_index].humidity;
  double start_temp     = self->time_series_ptr[0].temperature;
  double end_temp       = self->time_series_ptr[self->time_series_index].temperature;

  bool negative_temp_slope = (end_temp < start_temp);
  bool negative_humidity_slope = (end_humidity < start_humidity);
  bool return_value = false;

  // Only over humidity
  if (self->over_humidity && !self->over_temp) {
    return_value = negative_humidity_slope;
  }

  // Only over temp
  if (self->over_temp && !self->over_humidity) {
    return_value = negative_temp_slope;
  }

  // Both over temp and over humidity
  if (self->over_humidity && self->over_temp) {
    return_value = negative_humidity_slope && negative_temp_slope;
  }

  return return_value;
}