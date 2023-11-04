#ifndef FIREBASE_H
#define FIREBASE_H

#include <esp_err.h>
#include <freertos/queue.h>

typedef struct Firebase {
  const char* firebase_url;
  const char* certificate;

  QueueHandle_t* sensor_queue;

  esp_err_t (*send_data)(const char* data);
} Firebase;

void firebase_init(Firebase* fb_struct_ptr, const char* url, QueueHandle_t* sensor_queue);

#endif /* FIREBASE_H */