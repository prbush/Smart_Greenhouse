/*
  Smart greenhouse firmware for CSE 475, Fall 2023.
*/


//
// Includes

/* C std libs */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

/* FreeRTOS components */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

/* ESP libs */
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_random.h"
#include "led_strip.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

/* Custom components */
#include "environmental_sensor.h"
#include "fan.h"
#include "firebase.h"
#include "lights.h"
#include "soil_sensor.h"
#include "uv_sensor.h"

/* Configuration items from menuconfig tool */
#include "../build/config/sdkconfig.h"

//
// Defines

/* GPIO pin assignments */
#define BLINK_GPIO 48

/* WiFi settings */
// TODO: remove unnecessary definitions and preprocessor commands
#ifdef USE_HOTSPOT
#define EXAMPLE_ESP_WIFI_SSID      "Phil_iPhone"
#define EXAMPLE_ESP_WIFI_PASS      "esp32wificonnection"
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#else
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#endif
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Firebase Realtime Database URL
#define FIREBASE_URL "https://daily-trader-default-rtdb.firebaseio.com/apps.json"

// I2C defines
#define I2C_MASTER_SCL_IO           2       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           1       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0       /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000  /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       2500



//
// Function prototypes

/* Tasks */
void led_task(void* arg);
void firebase_task(void *arg);
void sensors_task(void *arg);
void environmental_control_task(void *arg);

/* Static helper functions and callbacks */
static void blink_led(uint32_t index, uint8_t red, uint8_t green, uint8_t blue, bool led_state);
static void configure_led(void);
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
static void wifi_init_sta(void);
static esp_err_t i2c_master_init(void);

//
// Variables

/* FreeRTOS variables */
static TaskHandle_t led_task_handle = NULL;
static TaskHandle_t firebase_task_handle = NULL;
static TaskHandle_t sensors_task_handle = NULL;
static TaskHandle_t environmental_control_task_handle = NULL;
static EventGroupHandle_t s_wifi_event_group;
static EventGroupHandle_t task_control_events;
static QueueHandle_t firebase_queue;
static QueueHandle_t sensor_queue;
static QueueHandle_t env_ctrl_queue;

/* Const strings */
static const char *WIFI_TAG = "WIFI";
static const char *LED_TAG = "LED";
static const char *I2C_TAG = "I2C";
// Testing JSON string
char * sample_json_string = "{\"Temp\":\"50\",\"Pressure\":\"90\",\"Humidity\":\"55\",\"UV A\":\"300\",\"UV B\":\"250\",\"UV C\":\"125\",\"Soil sensor\":\"1300\"}";

/* Static objects and reference data */
static led_strip_handle_t led_strip;
static uint8_t red, green, blue;
static int s_retry_num = 0;

/* Passable Objects */
Firebase fb;
Environmental_sensor env;
UV_sensor uv;
Soil_sensor soil;

/*

App entry point

*/
void app_main(void)
{
  // Create our event groups and queues
  s_wifi_event_group = xEventGroupCreate();
  task_control_events = xEventGroupCreate();
  firebase_queue = xQueueCreate(10, sizeof(firebase_data_struct));
  sensor_queue = xQueueCreate(10, sizeof(sensor_data_struct));
  env_ctrl_queue = xQueueCreate(10, sizeof(status_data_struct));

  //Initialize NVS, needed for WiFi
  esp_err_t ret = nvs_flash_init();

  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ret = esp_netif_init();
  ESP_ERROR_CHECK(ret);

  ret = esp_event_loop_create_default();
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(WIFI_TAG, "ESP_WIFI_MODE_STA");
  // Connect to WiFi
  wifi_init_sta();

  // Create RTOS threads
  xTaskCreate(firebase_task, "Firebase task", 16384, NULL, 5, &firebase_task_handle);
  xTaskCreate(led_task, "LED task", 4096, NULL, 5, &led_task_handle);
  xTaskCreate(sensors_task, "Sensors task", 8192, NULL, 5, &sensors_task_handle);
  xTaskCreate(environmental_control_task, "Env ctrl task", 8192, NULL, 5, &environmental_control_task_handle);

  // This "main" task will be deleted after returning
}


/*

FreeRTOS tasks

*/
void led_task(void* arg)
{
  uint32_t index = 0;
  bool led_state = false;

  configure_led();

  while (1) {
    red = (uint8_t) (esp_random() % 24);
    green = (uint8_t) (esp_random() % 24);
    blue = (uint8_t) (esp_random() % 24);
    blink_led(index, red, green, blue, led_state);
    led_state = !led_state;
    vTaskDelay(50);
  }
}

void firebase_task(void *arg)
{
  firebase_data_struct firebase_data = {0};

  firebase_init(&fb, FIREBASE_URL, &firebase_queue);
  while(1) {
    // Wait until we get a message from the enviromental control task
    xQueueReceive(firebase_queue, &firebase_data, portMAX_DELAY);

    // Assemble a JSON string with the firebase data
    // ...

    // Send the data to firebase
    fb.send_data(&firebase_data);
  }
}

void sensors_task(void* arg)
{
  sensor_data_struct  sensor_data = {0};
  struct bme280_data  env_sensor_readings = {0};
  UV_converted_values uv_readings = {0};
  esp_err_t           return_code;

  // Initialize I2C as master
  ESP_ERROR_CHECK(i2c_master_init());
  ESP_LOGI(I2C_TAG, "I2C initialized successfully");

  vTaskDelay(10);

  // Initialize the UV sensor
  return_code = uv_sensor_init(&uv, I2C_NUM_0, GAIN_256x, MS_64);
  if (return_code != ESP_OK) {
    vTaskDelay(2000);
    esp_restart();
  }

  vTaskDelay(10);

  // Initialize the BME280 Environmental sensor
  return_code = enviromental_sensor_init(&env, (((1 / portTICK_PERIOD_MS) / 25) + 1) /* 25Hz */, I2C_NUM_0);
  if (return_code != ESP_OK) {
    vTaskDelay(2000);
    esp_restart();
  }

  // Initialize the soil sensor
  return_code = soil_sensor_init(&soil, ADC_CHANNEL_3, ADC_ATTEN_DB_11);

  while(1) {
    // Zero out the structs to start fresh
    memset(&sensor_data, 0, sizeof(sensor_data_struct));
    memset(&env_sensor_readings, 0, sizeof(struct bme280_data));
    memset(&uv_readings, 0, sizeof(UV_converted_values));

    // Get BME280 readings
    return_code = env.get_readings(&env_sensor_readings);
    // TODO: check error code, determine some way of indicating sensor failure over the 

    // Log results
    ESP_LOGI(I2C_TAG, "Environmental sensor readings: Temp = %.3lf degC, Pres = %.3lf hPa, Rh = %.3lf %%",
      env_sensor_readings.temperature, env_sensor_readings.pressure, env_sensor_readings.humidity);
    // Copy to sensor_data_struct
    memcpy(&(sensor_data.bme280_data), &env_sensor_readings, sizeof(struct bme280_data));

    // Gather UV sensor readings
    // ...
    // Gather soil sensor readings
    // ...
    return_code = uv.get_readings(&uv_readings);
    ESP_LOGI(I2C_TAG, "UV sensor readings: UV A = %.3lf uW/cm^2, UV B = %.3lf uW/cm^2, UV C = %.3lf uW/cm^2",
      uv_readings.UV_A, uv_readings.UV_B, uv_readings.UV_C);
    // Copy to sensor_data_struct
    memcpy(&(sensor_data.uv_data), &uv_readings, sizeof(UV_converted_values));

    // Send the sensor data to the environmental_control_task
    xQueueGenericSend(sensor_queue, &sensor_data, 1, queueSEND_TO_BACK);

    vTaskDelay(1000);
  }
}

void environmental_control_task(void *arg)
{
  sensor_data_struct sensor_data = {0};
  status_data_struct status_data = {0};
  firebase_data_struct firebase_data = {0};

  while(1) {

    // Wait until we get a message with sensor data
    xQueueReceive(sensor_queue, &sensor_data, portMAX_DELAY);
    // Check for an empty message

    // Make enviromental changes (fan, pdlc, lights) as needed based on sensor data and set thresholds
    // ...
    
    // Gather statuses of the fan, pdlc, lights
    // ...

    // Assemble the firebase data struct
    memcpy(&(firebase_data.sensor_data), &sensor_data, sizeof(sensor_data_struct));
    memcpy(&(firebase_data.status_data), &status_data, sizeof(status_data_struct));

    // Send the message to the firebase task
    xQueueSend(firebase_queue, &firebase_data, 1);

  }
}


/*

Event handlers

*/
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
      esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
      if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
          esp_wifi_connect();
          s_retry_num++;
          ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
      } else {
          xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      }
      ESP_LOGI(WIFI_TAG,"connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
      s_retry_num = 0;
      xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}


/*

Static functions

*/

//
// Initialization functions
static void configure_led(void)
{
  ESP_LOGI(LED_TAG, "Application configured to blink addressable LED!");
  /* LED strip initialization with the GPIO and pixels number*/
  led_strip_config_t strip_config = {
      .strip_gpio_num = BLINK_GPIO,
      .max_leds = 1, // at least one LED on board
  };
  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 10 * 1000 * 1000, // 10MHz
  };
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
  /* Set all LED off to clear all pixels */
  led_strip_clear(led_strip);
}

static void wifi_init_sta(void)
{
  s_wifi_event_group = xEventGroupCreate();

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta = {
          .ssid = EXAMPLE_ESP_WIFI_SSID,
          .password = EXAMPLE_ESP_WIFI_PASS,
          /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
            * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
            * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
            * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
            */
          .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
          .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
          .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
      },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start() );

  ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
    * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
          pdFALSE,
          pdFALSE,
          portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
    * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
      ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s password:%s",
                EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  } else if (bits & WIFI_FAIL_BIT) {
      ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s, password:%s",
                EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  } else {
      ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
  }
}

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_NUM_0, &conf);

    return i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

//
// Helper functions
static void blink_led(uint32_t index, uint8_t red, uint8_t green, uint8_t blue, bool led_state)
{
  /* If the addressable LED is enabled */
  if (led_state) {
      /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
      led_strip_set_pixel(led_strip, index, red, green, blue);
      /* Refresh the strip to send data */
      led_strip_refresh(led_strip);
  } else {
      /* Set all LED off to clear all pixels */
      led_strip_clear(led_strip);
  }
}
