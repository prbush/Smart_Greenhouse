/*
  Smart greenhouse firmware for CSE 475, Fall 2023.
*/


//
// Includes

/* C std libs */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

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
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "led_strip.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"
#include "esp_http_client.h"

/* Custom components */
#include "environmental_sensor.h"
#include "firebase.h"
#include "soil_sensor.h"
#include "uv_sensor.h"
#include "fan.h"
#include "lights.h"
#include "pdlc.h"

/* Configuration items from menuconfig tool */
#include "../build/config/sdkconfig.h"

//
// Defines

/* WiFi settings */
// TODO: remove unnecessary definitions and preprocessor commands
#define USE_HOTSPOT


#ifdef USE_HOTSPOT
// #define EXAMPLE_ESP_WIFI_SSID      "Phil_iPhone"
// #define EXAMPLE_ESP_WIFI_PASS      "esp32wificonnection"
// #define EXAMPLE_ESP_WIFI_SSID      "Gomaas iPhone"
// #define EXAMPLE_ESP_WIFI_PASS      "yousefisawesomeright"
// #define EXAMPLE_ESP_WIFI_SSID      "PandaHat"
// #define EXAMPLE_ESP_WIFI_PASS      "mitosismitosis"
#define EXAMPLE_ESP_WIFI_SSID      "Noire"
#define EXAMPLE_ESP_WIFI_PASS      "ahmad123"
#else
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#endif
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

// The sensor timer go bit
#define SENSOR_CYCLE_START_BIT BIT0

// Firebase Realtime Database URL
#define FIREBASE_URL "https://daily-trader-default-rtdb.firebaseio.com/apps.json"



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
static void obtain_time(void);
static void time_sync_notification_cb(struct timeval *tv);
static void sensor_timer_callback(TimerHandle_t xTimer);


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
static TimerHandle_t sensor_timer_handle;
static uint32_t      sensor_timer_id = 475;

/* Const strings */
static const char *WIFI_TAG = "WIFI";
static const char *LED_TAG = "LED";
static const char *SENSOR_TAG = "Sensor task";
static const char *ENV_CONTROL = "Environmental Control task";
static const char *SNTP_TAG = "SNTP";

/* Static objects and reference data */
static led_strip_handle_t led_strip;
static uint8_t red, green, blue;
static int s_retry_num = 0;
time_t now;
struct tm timeinfo;
struct tm global_start_time_info;
time_t global_start_time;

/* Passable Objects */
Firebase fb;
Environmental_sensor env;
UV_sensor uv;
Soil_sensor soil;
Fan fan;
Lights lights;
PDLC pdlc;
Environmental_control env_ctrl;
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
  sensor_timer_handle = xTimerCreate("Sensor timer", CONFIG_FREERTOS_HZ, pdTRUE, 
                                    &sensor_timer_id, sensor_timer_callback);

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

  obtain_time();

  // Check the time and set via NTP if needed
  // Get current time and local time
  time(&now);
  localtime_r(&now, &timeinfo);

  // Is time set? If not, tm_year will be (1970 - 1900).
  if (timeinfo.tm_year < (2016 - 1900)) {
    ESP_LOGI(SNTP_TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
    obtain_time();
    // update 'now' and 'timeinfo' variable with current time and local time
    time(&now);
    localtime_r(&now, &timeinfo);
  }

  // Get the start time
  global_start_time = now;
  localtime_r(&now, &global_start_time_info);

  // Create RTOS threads
  xTaskCreate(firebase_task, "Firebase task", 16384, NULL, 5, &firebase_task_handle);
  xTaskCreate(led_task, "LED task", 4096, NULL, 5, &led_task_handle);
  xTaskCreate(sensors_task, "Sensors task", 8192, NULL, 5, &sensors_task_handle);
  xTaskCreate(environmental_control_task, "Env ctrl task", 8192, NULL, 5, &environmental_control_task_handle);

  // This "main" task will exit and be cleaned up automatically
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
  EventBits_t         status_bit = 0;

  // Initialize I2C as master
  ESP_ERROR_CHECK(i2c_master_init());
  ESP_LOGI(SENSOR_TAG, "I2C initialized successfully");

  vTaskDelay(10);

  // Initialize the UV sensor
  return_code = uv_sensor_init(&uv, CONFIG_I2C_MASTER_NUM, GAIN_256x, MS_64);
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
  // TODO: identify the soil dry/wet vals and put them here
  return_code = soil_sensor_init(&soil, CONFIG_SOIL_SENSOR_ADC_UNIT, CONFIG_SOIL_SENSOR_ADC_CHANNEL, ADC_ATTEN_DB_11);
  if (return_code != ESP_OK) {
    vTaskDelay(2000);
    esp_restart();
  }

  // Start the sensor timer
  xTimerStart(sensor_timer_handle, 1);

  while(1) {
    // Zero out the structs to start fresh
    memset(&sensor_data, 0, sizeof(sensor_data_struct));
    memset(&env_sensor_readings, 0, sizeof(struct bme280_data));
    memset(&uv_readings, 0, sizeof(UV_converted_values));

    // Wait until we get the event flag set by the timer before running
    status_bit = xEventGroupWaitBits(task_control_events, SENSOR_CYCLE_START_BIT, pdTRUE,
                       pdTRUE, 1000);
                      
    if (!(status_bit & SENSOR_CYCLE_START_BIT)) {
      continue;
    }

    // Get BME280 readings
    return_code = env.get_readings(&env_sensor_readings);

    // Log results
    ESP_LOGI(SENSOR_TAG, "Environmental sensor readings: Temp = %.3lf degC, Pres = %.3lf hPa, Rh = %.3lf %%",
      env_sensor_readings.temperature, env_sensor_readings.pressure, env_sensor_readings.humidity);
    // Copy to sensor_data_struct
    memcpy(&(sensor_data.bme280_data), &env_sensor_readings, sizeof(struct bme280_data));

    // Gather UV sensor readings
    return_code = uv.get_readings(&uv_readings);
    ESP_LOGI(SENSOR_TAG, "UV sensor readings: UV A = %.3lf uW/cm^2, UV B = %.3lf uW/cm^2, UV C = %.3lf uW/cm^2",
      uv_readings.UV_A, uv_readings.UV_B, uv_readings.UV_C);
    // Copy to sensor_data_struct
    memcpy(&(sensor_data.uv_data), &uv_readings, sizeof(UV_converted_values));

    // Gather soil sensor readings
    sensor_data.soil_wetness = soil.get_reading();
    ESP_LOGI(SENSOR_TAG, "Soil sensor reading: %u", sensor_data.soil_wetness);

    // Throw in the timestamp
    time(&now);
    sensor_data.timestamp = now;

    // Send the sensor data to the environmental_control_task
    xQueueGenericSend(sensor_queue, &sensor_data, 1, queueSEND_TO_BACK);
  }
}

void environmental_control_task(void *arg)
{
  esp_err_t return_code = ESP_FAIL;
  sensor_data_struct sensor_data = {0};
  status_data_struct status_data = {0};
  firebase_data_struct firebase_data = {0};

  return_code = environmental_control_init(&env_ctrl, &fan, &lights, &pdlc);

  while(1) {

    // Wait until we get a message with sensor data
    xQueueReceive(sensor_queue, &sensor_data, portMAX_DELAY);

    // Make enviromental changes (fan, pdlc, lights) as needed based on sensor data and set thresholds
    env_ctrl.process_env_data(sensor_data);

    // Gather statuses of the fan, pdlc, lights
    status_data = env_ctrl.get_statuses();
    
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
      .strip_gpio_num = CONFIG_BLINK_GPIO,
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
        .sda_io_num = CONFIG_I2C_MASTER_SDA,
        .scl_io_num = CONFIG_I2C_MASTER_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CONFIG_I2C_FAST_MODE
    };

    i2c_param_config(I2C_NUM_0, &conf);

    return i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, CONFIG_I2C_MASTER_RX_BUF, CONFIG_I2C_MASTER_TX_BUF, 0);
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


void time_sync_notification_cb(struct timeval *tv)
{
  // Just prints a message that time sync has occured
  ESP_LOGI(SNTP_TAG, "Notification of a time synchronization event");
}


static void obtain_time(void)
{
  int retry = 0;
  const int retry_count = 15;
  char strftime_buf[64];

  ESP_LOGI(SNTP_TAG, "Initializing and starting SNTP");

  // Will get the time and sync once an hour.
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
  config.sync_cb = time_sync_notification_cb; // Just a notification callback

  esp_netif_sntp_init(&config);

  // wait for time to be set
  while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
    ESP_LOGI(SNTP_TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
  }

  if (retry < retry_count) {
    ESP_LOGI(SNTP_TAG, "Time successfully synced.");
  } else {
    ESP_LOGI(SNTP_TAG, "Time sync unsuccessful. System time is not set.");
  }

  // Set timezone to Pacific Standard Time and print local time
  // setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1); // America/Los_Angles TZ string
  tzset();
  time(&now);
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(SNTP_TAG, "The current date/time is: %s", strftime_buf);
}

static void sensor_timer_callback(TimerHandle_t xTimer)
{
  EventBits_t wifi_status = xEventGroupGetBits(s_wifi_event_group);
  // Sanity check that another timer didn't magically fire this callback
  if (*(uint32_t*)pvTimerGetTimerID(xTimer) != sensor_timer_id) {
    return;
  }

  // Check that we are connected to WiFi
  if (wifi_status & WIFI_CONNECTED_BIT) {
    ESP_LOGI(SENSOR_TAG, "Sensor loop starting.");
    xEventGroupSetBits(task_control_events, SENSOR_CYCLE_START_BIT);
  } else {
    ESP_LOGE(WIFI_TAG, "WiFi not connected, unable to run sensor tasks.");
  }
}
