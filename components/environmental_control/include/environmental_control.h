#ifndef ENVIRONMENTAL_CONTROL_H
#define ENVIRONMENTAL_CONTROL_H

#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_err.h"
#include "fan.h"
#include "lights.h"
#include "pdlc.h"
#include "environmental_sensor.h"
#include "uv_sensor.h"

// For the class demo, we define much shorter timescales for environmental control
#define CLASS_DEMO true

#define HUMIDITY_THRESHOLD    80  // degC
#define TEMPERATURE_THRESHOLD 90  // Rh
#define UV_A_THRESHOLD       1000 // uWatt / cm^2
#define UV_B_THRESHOLD        500 // uWatt / cm^2
#define UV_C_THRESHOLD        500 // uWatt / cm^2

#define ENV_TIMER_ID 1337
#define SAMPLES_PER_MINUTE 60
#define DAYLIGHT_START 6 // 06:00am
#define DAYLIGHT_END 18  // 06:00pm (18:00)
#define ONE_MINUTE 60

typedef enum status_state {
  OFF = 0,
  ON = 1
} status_state_t;

typedef struct sensor_data{
  struct bme280_data  bme280_data;
  UV_converted_values uv_data;
  uint16_t            soil_wetness;
  time_t              timestamp;
} sensor_data_struct;

typedef struct status_data {
  status_state_t fan_state;
  status_state_t lights_state;
  status_state_t pdlc_state; 
} status_data_struct;

typedef struct Environmental_control {
  Fan                 *fan;
  Lights              *lights;
  PDLC                *pdlc;

  sensor_data_struct  current_sensor_data;

  TimerHandle_t       timer_handle;
  uint32_t            timer_id;

  time_t              time_now;
  struct tm           time_info;
  struct bme280_data  *time_series_ptr;

  float               uv_a_integral;
  float               uv_b_integral;
  float               uv_c_integral;

  uint32_t            timer_period;
  uint32_t            timer_fires_counter;

  bool                is_daylight;
  bool                timer_running;

  status_data_struct  (*get_statuses)(void);
  void                (*process_env_data)(sensor_data_struct sensor_readings);


} Environmental_control;

esp_err_t environmental_control_init(Environmental_control *struct_ptr, Fan *fan, Lights *lights, PDLC *pdlc);

#endif /* ENVIRONMENTAL_CONTROL_H */
