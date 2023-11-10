#ifndef FIREBASE_H
#define FIREBASE_H

#include <esp_err.h>
#include <freertos/queue.h>

#include "../../environmental_sensor/include/environmental_sensor.h"
#include "../../uv_sensor/include/uv_sensor.h"

//
// Structs, enums, typedefs
typedef enum status_state {
  OFF = 0,
  ON = 1
} status_state_t;

typedef struct sensor_data{
  struct bme280_data bme280_data;
  UV_converted_values uv_data;
} sensor_data_struct;

typedef struct status_data {
  status_state_t fan_state;
  status_state_t lights_state;
  status_state_t pdlc_state; 
} status_data_struct;

typedef struct Firebase_data {
  sensor_data_struct sensor_data;
  status_data_struct status_data;
} firebase_data_struct;

typedef struct Firebase {
  const char* firebase_url;
  const char* certificate;

  QueueHandle_t* sensor_queue;

  esp_err_t (*send_data)(firebase_data_struct *data);
} Firebase;

void firebase_init(Firebase* fb_struct_ptr, const char* url, QueueHandle_t* sensor_queue);

#endif /* FIREBASE_H */