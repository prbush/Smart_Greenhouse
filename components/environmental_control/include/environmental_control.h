// #ifndef ENVIRONMENTAL_CONTROL_H
// #define ENVIRONMENTAL_CONTROL_H

// #include <time.h>
// #include <sys/time.h>
// #include "freertos/timers.h"
// #include "esp_err.h"
// #include "environmental_sensor.h"
// #include "fan.h"
// #include "lights.h"
// #include "pdlc.h"
// #include "firebase.h"

// #define HUMIDITY_THRESHOLD    80  // degC
// #define TEMPERATURE_THRESHOLD 90  // Rh
// #define UV_A_THRESHOLD       1000 // uWatt / cm^2
// #define UV_B_THRESHOLD        500 // uWatt / cm^2
// #define UV_C_THRESHOLD        500 // uWatt / cm^2

// typedef struct Environmental_control {
//   Fan                 *fan;
//   Lights              *lights;
//   PDLC                *pdlc;

//   TimerHandle_t       timer_handle;

//   time_t              time_now;
//   struct tm           time_info;

//   sensor_data_struct  sensor_data;

// } Environmental_control;

// esp_err_t environmental_control_init(Fan *fan, Lights *lights, PDLC *pdlc)

// #endif /* ENVIRONMENTAL_CONTROL_H */
