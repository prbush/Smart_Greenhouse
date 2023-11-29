#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "fan.h"
#include "lights.h"
#include "pdlc.h"
#include "sdkconfig.h"
#include "environmental_control.h"




// Testing counter
uint32_t toggle = 0;
bool lights_on = false;
bool fan_on = false;
bool pdlc_on = false;








// Static private object pointer
static Environmental_control *self;

// Logger tag
const char* ENVIRONMENTAL_TAG = "Environmental control";

// Private functions
void check_for_env_changes_callback(TimerHandle_t xTimer);
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
  self->timer_id = ENV_TIMER_ID;
  self->get_statuses = _environmental_control_get_statuses;
  self->process_env_data = _environmental_control_process_env_data;

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
  self->timer_handle = xTimerCreate("ENV timer", 60 * CONFIG_FREERTOS_HZ, pdFALSE, 
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
  /*
  Make this a coroutine. Have early exit if values are below threashold. Make sure
  to add to the accumuators for UV a,b,c. Need to do conversion:

   uW/cm^2
  --------- * Time = j/cm^2 -- > time will be time between measurements (1 sec)
    1,000

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
    
    Lights:
      Lights should simply be on from 6AM local to 6PM local -- check the time

    PDLC: Turn on if UV B or C accumulated vales are above threshold.
          Turn off after the lights have been turned off
          !! Can we check for sunset times? If so, turn off at sunset
  */

  if (toggle % 5 == 0) {
    fan_on = !fan_on;
    if (!fan_on) {
      self->fan->off();
    } else {
      self->fan->on();
    }
  }

  if (toggle % 7 == 0) {
    lights_on = !lights_on;
    if (!lights_on) {
      self->lights->off();
    } else {
      self->lights->on();
    }
  }

  if (toggle % 3 == 0) {
    pdlc_on = !pdlc_on;
    if (!pdlc_on) {
      self->pdlc->off();
    } else {
      self->pdlc->on();
    }
  }

  toggle ++;
}

void check_for_env_changes_callback(TimerHandle_t xTimer)
{
  // Sanity check that another timer didn't magically fire this callback
  if ((uint32_t)pvTimerGetTimerID(xTimer) != self->timer_id) {
    return;
  }


  return;
}