

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"

#include "firebase.h"

static Firebase* self;



static const char *HTTP_TAG = "HTTP";

extern const char cert_start[] asm("_binary_certificate_pem_start");
extern const char cert_end[]   asm("_binary_certificate_pem_end");

static esp_err_t _firebase_send_data(const char *data);
static esp_err_t _http_event_handler(esp_http_client_event_t *evt);

void firebase_init(Firebase* fb_struct_ptr, const char* url, QueueHandle_t* sensor_queue)
{
  self = fb_struct_ptr;

  self->firebase_url = url;
  self->certificate = cert_start;
  self->sensor_queue = sensor_queue;

  self->send_data = _firebase_send_data;
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
      // Handle data received from Firebase response if needed
      ESP_LOGI(HTTP_TAG, "HTTP_EVENT_ON_DATA: %s", (char *)evt->data);
    }
    return ESP_OK;
}

static esp_err_t _firebase_send_data(const char *data) {
    esp_http_client_config_t config = {
        .url = self->firebase_url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .cert_pem = cert_start
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_http_client_set_post_field(client, data, strlen(data));

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(HTTP_TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
