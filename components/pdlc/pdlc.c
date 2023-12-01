#include <stdio.h>
#include <unistd.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "pdlc.h"


// Static private object pointer
static PDLC *self;

// Logger tag
const char* PDLC_TAG = "PDLC";

// Private functions

// Public functions privided via struct fn pointers
static void _pdlc_on(void);
static void _pdlc_off(void);
static pdlc_state_t _pdlc_get_state(void);

esp_err_t pdlc_init(PDLC* struct_ptr, gpio_num_t gpio_pin)
{
  esp_err_t return_code = ESP_OK;

  // Assign struct fields
  self = struct_ptr;
  self->gpio_pin_num = gpio_pin;
  self->on = _pdlc_on;
  self->off = _pdlc_off;
  self->get_state = _pdlc_get_state;

  // Disable interrupt
  self->fet_gpio_config.intr_type = GPIO_INTR_DISABLE;
  // Set as output mode
  self->fet_gpio_config.mode = GPIO_MODE_OUTPUT;
  // Bit mask of the pin to be set
  self->fet_gpio_config.pin_bit_mask = (((uint64_t)1) << ((uint64_t)gpio_pin));
  // Disable pull-down mode
  self->fet_gpio_config.pull_down_en = 1;
  // Disable pull-up mode
  self->fet_gpio_config.pull_up_en = 1;
  // Configure GPIO with the given settings
  return_code = gpio_config(&(self->fet_gpio_config));

  if (return_code != ESP_OK) {
    ESP_LOGE(PDLC_TAG, "Failed to configure PDLC GPIO pin.");
  }

  // Set the initial state to off
  self->off();

  return return_code;
}

static void _pdlc_on(void)
{
  // Have to cycle through the modes
  gpio_set_level(self->gpio_pin_num, (uint32_t)PDLC_ON);

  ESP_LOGI(PDLC_TAG, "PDLC on.");

  self->current_state = PDLC_ON;
}

static void _pdlc_off(void)
{
  // Have to cycle through the modes
  gpio_set_level(self->gpio_pin_num, (uint32_t)PDLC_OFF);

  ESP_LOGI(PDLC_TAG, "PDLC off.");

  self->current_state = PDLC_OFF;
}

static pdlc_state_t _pdlc_get_state(void)
{
  return self->current_state;
}