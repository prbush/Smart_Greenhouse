#ifndef PDLC_H
#define PDLC_H

#include "esp_err.h"
#include "driver/gpio.h"

typedef enum pdlc_state {
  PDLC_OFF = 0,
  PDLC_ON = 1
} pdlc_state_t;

typedef struct PDLC {
  gpio_num_t    gpio_pin_num;
  gpio_config_t fet_gpio_config;

  pdlc_state_t  current_state;

  void          (*on)(void);
  void          (*off)(void);
  pdlc_state_t  (*get_state)(void);
} PDLC;

esp_err_t pdlc_init(PDLC* struct_ptr, gpio_num_t gpio_pin);

#endif /* PDLC_H */