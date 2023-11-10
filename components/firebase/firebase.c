

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "cJSON.h"
#include "firebase.h"

// Static private object pointer
static Firebase* self;

// Logger tag
static const char *HTTP_TAG = "HTTP";

// Private functions
static char* assemble_json_string(firebase_data_struct *data);
static esp_err_t http_event_handler(esp_http_client_event_t *evt);

// Public functions
static esp_err_t _firebase_send_data(firebase_data_struct *data);


// SSL cert
extern const char cert_start[] asm("_binary_certificate_pem_start");
extern const char cert_end[]   asm("_binary_certificate_pem_end");


/*!
 * Public init function
 */
void firebase_init(Firebase* fb_struct_ptr, const char* url, QueueHandle_t* sensor_queue)
{
  self = fb_struct_ptr;

  self->firebase_url = url;
  self->certificate = cert_start;
  self->sensor_queue = sensor_queue;

  self->send_data = _firebase_send_data;
}


/*!
 * Http event handler
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
      // Handle data received from Firebase response if needed
      ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ON_DATA: %s", (char *)evt->data);
    }
    return ESP_OK;
}


/*!
 * Public send data fuction -- sends a POST request to Firebase
 */
static esp_err_t _firebase_send_data(firebase_data_struct *data) 
{
  char *serialized_string = NULL;

  esp_http_client_config_t config = {
      .url = self->firebase_url,
      .method = HTTP_METHOD_POST,
      .event_handler = http_event_handler,
      .cert_pem = cert_start
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);

  esp_http_client_set_header(client, "Content-Type", "application/json");

  serialized_string = assemble_json_string(data);

  esp_http_client_set_post_field(client, serialized_string, strlen(serialized_string));

  esp_err_t err = esp_http_client_perform(client);
  if (err != ESP_OK) {
      ESP_LOGE(HTTP_TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
  }

  esp_http_client_cleanup(client);
  return err;
}

/*
Sample JSON

{ "name": "Smart Greenhouse",
  "sensors": {
      "Temperature": 0,
      "Pressure": 0,
      "Humidity": 0,
      "UV A": 0,
      "UV B": 0,
      "UV C": 0
   },

   "Status": {
      "Fan": true,
      "Lights": true,
      "PDLC": true
   }
}
*/


/*!
 * Function to generate a serialized JSON string
 */
static char* assemble_json_string(firebase_data_struct *data)
{
  char *string = NULL;
  cJSON *json = cJSON_CreateObject();
  cJSON *sensor = NULL;
  cJSON *sensors = NULL;
  cJSON *status = NULL;
  cJSON *statuses = NULL;

  // Name field
  cJSON_AddStringToObject(json, "name", "Smart Greenhouse");


  // Make the sensors array
  sensors = cJSON_AddArrayToObject(json, "Sensors");
  // Now we'll fill out the sensors array, starting with Temp
  sensor = cJSON_CreateObject();
  cJSON_AddNumberToObject(sensor, "Temp", data->sensor_data.bme280_data.temperature);
  cJSON_AddItemToArray(sensors, sensor);
  // Pressure
  sensor = cJSON_CreateObject();
  cJSON_AddNumberToObject(sensor, "Pres", data->sensor_data.bme280_data.pressure);
  cJSON_AddItemToArray(sensors, sensor);
  // Humidity
  sensor = cJSON_CreateObject();
  cJSON_AddNumberToObject(sensor, "Rh", data->sensor_data.bme280_data.humidity);
  cJSON_AddItemToArray(sensors, sensor);
  // UV A
  sensor = cJSON_CreateObject();
  cJSON_AddNumberToObject(sensor, "UV A", data->sensor_data.uv_data.UV_A);
  cJSON_AddItemToArray(sensors, sensor);
  // UV B
  sensor = cJSON_CreateObject();
  cJSON_AddNumberToObject(sensor, "UV B", data->sensor_data.uv_data.UV_B);
  cJSON_AddItemToArray(sensors, sensor);
  // UV A
  sensor = cJSON_CreateObject();
  cJSON_AddNumberToObject(sensor, "UV C", data->sensor_data.uv_data.UV_C);
  cJSON_AddItemToArray(sensors, sensor);

  // Now on to the statuses array
  statuses = cJSON_AddArrayToObject(json, "Status");
  // And fill in the status array, starting with Fan
  status = cJSON_CreateObject();
  cJSON_AddBoolToObject(status, "Fan", data->status_data.fan_state);
  cJSON_AddItemToArray(statuses, status);
  // Lights
  status = cJSON_CreateObject();
  cJSON_AddBoolToObject(status, "Lights", data->status_data.lights_state);
  cJSON_AddItemToArray(statuses, status);
  // PDLC
  status = cJSON_CreateObject();
  cJSON_AddBoolToObject(status, "PDLC", data->status_data.pdlc_state);
  cJSON_AddItemToArray(statuses, status);

  string = cJSON_Print(json);

  cJSON_Delete(json);

  return string;
}
