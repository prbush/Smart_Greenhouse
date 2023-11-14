#ifndef FAN_H
#define FAN_H

#include "esp_err.h"
#include "driver/gpio.h"

typedef enum fan_state {
  FAN_OFF = 0,
  FAN_ON = 1
} fan_state_t;

typedef struct Fan {
  gpio_num_t    gpio_pin_num;
  gpio_config_t fet_gpio_config;

  fan_state_t   current_state;

  void          (*on)(void);
  void          (*off)(void);
  fan_state_t   (*get_state)(void);
} Fan;

esp_err_t fan_init(Fan* struct_ptr, gpio_num_t gpio_pin);

#endif /* FAN_H */