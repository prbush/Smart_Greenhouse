/*
  Smart greenhouse firmware for CSE 475, Fall 2023.
*/

// Includes
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "led_strip.h"
#include "environmental_sensor.h"


// Defines
#define BLINK_GPIO 48

// Function prototypes
void led_task(void* arg);
static void blink_led(uint32_t index, uint8_t red, uint8_t green, uint8_t blue, bool led_state);
static void configure_led(void);

// FreeRTOS variables
TaskHandle_t led_task_handle = NULL;

// Static variables
static const char *LED_TAG = "LED";
static led_strip_handle_t led_strip;
static uint8_t red, green, blue;


void app_main(void)
{
  xTaskCreate(led_task, "LED task", 4096, NULL, 5, &led_task_handle);
  bmefunc();
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
