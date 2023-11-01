#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"

#include "firebase.h"

esp_err_t firebase_push_data(char* data)
{
  return ESP_OK;
}
