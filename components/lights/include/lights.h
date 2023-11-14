#ifndef LIGHTS_H
#define LIGHTS_H

#include "esp_err.h"
#include "driver/gpio.h"

typedef enum light_state {
  LIGHT_OFF = 0,
  LIGHT_ON = 1
} light_state_t;

typedef struct Lights {
  gpio_num_t    gpio_pin_num;
  gpio_config_t fet_gpio_config;

  light_state_t current_state;

  void          (*on)(void);
  void          (*off)(void);
  light_state_t (*get_state)(void);
} Lights;

esp_err_t lights_init(Lights* struct_ptr, gpio_num_t gpio_pin);

#endif /* LIGHTS_H */