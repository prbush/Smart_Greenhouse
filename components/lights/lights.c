#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "lights.h"


// Static private object pointer
static Lights *self;

// Logger tag
const char* LIGHT_TAG = "LIGHTS";

// Private functions

// Public functions privided via struct fn pointers
static void _lights_on(void);
static void _lights_off(void);
static light_state_t _lights_get_state(void);

esp_err_t lights_init(Lights* struct_ptr, gpio_num_t gpio_pin)
{
  esp_err_t return_code = ESP_OK;

  // Assign struct fields
  self = struct_ptr;
  self->gpio_pin_num = gpio_pin;
  self->on = _lights_on;
  self->off = _lights_off;
  self->get_state = _lights_get_state;

  // Disable interrupt
  self->fet_gpio_config.intr_type = GPIO_INTR_DISABLE;
  // Set as output mode
  self->fet_gpio_config.mode = GPIO_MODE_OUTPUT;
  // Bit mask of the pin to be set
  self->fet_gpio_config.pin_bit_mask = (((uint64_t)1) << ((uint64_t)gpio_pin));
  // Disable pull-down mode
  self->fet_gpio_config.pull_down_en = 0;
  // Disable pull-up mode
  self->fet_gpio_config.pull_up_en = 0;
  // Configure GPIO with the given settings
  return_code = gpio_config(&(self->fet_gpio_config));

  if (return_code != ESP_OK) {
    ESP_LOGE(LIGHT_TAG, "Failed to configure Lights GPIO pin.");
  }

  // Set the initial state to off
  self->off();

  return return_code;
}

static void _lights_on(void)
{
  gpio_set_level(self->gpio_pin_num, (uint32_t)LIGHT_ON);

  ESP_LOGI(LIGHT_TAG, "Lights on.");

  self->current_state = LIGHT_ON;
}

static void _lights_off(void)
{
  gpio_set_level(self->gpio_pin_num, (uint32_t)LIGHT_OFF);

  ESP_LOGI(LIGHT_TAG, "Lights off.");

  self->current_state = LIGHT_OFF;
}

static light_state_t _lights_get_state(void)
{
  return self->current_state;
}