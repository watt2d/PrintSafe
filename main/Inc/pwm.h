#pragma once
#include "driver/ledc.h"
#include <stdint.h>

void pwm_init(void);
void update_pwm_Fan(uint8_t value);
void update_pwm_Res(uint8_t value);
