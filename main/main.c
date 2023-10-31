/*
  Smart greenhouse firmware for CSE 475, Fall 2023.
*/

// Includes
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_random.h"
#include "led_strip.h"
#include "driver/gpio.h"
#include "environmental_sensor.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
// Configuration items from menuconfig tool
#include "../build/config/sdkconfig.h"


// Defines
// GPIO pins
#define BLINK_GPIO 48
// WiFi settings
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


// Function prototypes
void led_task(void* arg);
void tcp_client(void* arg);
static void blink_led(uint32_t index, uint8_t red, uint8_t green, uint8_t blue, bool led_state);
static void configure_led(void);
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
static void wifi_init_sta(void);


// FreeRTOS variables
static TaskHandle_t led_task_handle = NULL;
static TaskHandle_t socket_task_handle = NULL;
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

// Static variables
static const char *WIFI_TAG = "WIFI";
static const char *LED_TAG = "LED";
static const char *SOCKET_TAG = "SOCKET";
static led_strip_handle_t led_strip;
static uint8_t red, green, blue;
static int s_retry_num = 0;


void app_main(void)
{
  //Initialize NVS, needed for WiFi
  esp_err_t ret = nvs_flash_init();

  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ret = esp_netif_init();
  ESP_ERROR_CHECK(ret);

  

  ESP_LOGI(WIFI_TAG, "ESP_WIFI_MODE_STA");
  // Connect to WiFi
  wifi_init_sta();

  // Test socket
  xTaskCreate(tcp_client, "Socket task", 4096, NULL, 5, &socket_task_handle);

  // Create RTOS threads
  xTaskCreate(led_task, "LED task", 4096, NULL, 5, &led_task_handle);
}

void led_task(void* arg)
{
  uint32_t index = 0;
  bool led_state = false;

  configure_led();

  while (1) {
    red = (uint8_t) (esp_random() % 256);
    green = (uint8_t) (esp_random() % 256);
    blue = (uint8_t) (esp_random() % 256);
    blink_led(index, red, green, blue, led_state);
    led_state = !led_state;
    vTaskDelay(50);
  }

}

static void blink_led(uint32_t index, uint8_t red, uint8_t green, uint8_t blue, bool led_state)
{
  ESP_LOGI(LED_TAG, "Turning the LED %s!", led_state ? "ON" : "OFF");
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

static void configure_led(void)
{
  ESP_LOGI(LED_TAG, "Example configured to blink addressable LED!");
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

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &event_handler,
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

static void event_handler(void* arg, esp_event_base_t event_base,
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


void tcp_client(void* arg)
{
    char rx_buffer[128];
    char host_ip[] = "192.168.0.11";
    int addr_family = 0;
    int ip_protocol = 0;
    int port = 39999;
    const char *payload = "Message from ESP32 ";

    while (1) {
        struct sockaddr_in dest_addr;
        inet_pton(AF_INET, host_ip, &dest_addr.sin_addr);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(SOCKET_TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(SOCKET_TAG, "Socket created, connecting to %s:%d", host_ip, port);

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(SOCKET_TAG, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(SOCKET_TAG, "Successfully connected");

        while (1) {
            int err = send(sock, payload, strlen(payload), 0);
            if (err < 0) {
                ESP_LOGE(SOCKET_TAG, "Error occurred during sending: errno %d", errno);
                break;
            }

            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(SOCKET_TAG, "recv failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(SOCKET_TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(SOCKET_TAG, "%s", rx_buffer);
            }
        }

        if (sock != -1) {
            ESP_LOGE(SOCKET_TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
}