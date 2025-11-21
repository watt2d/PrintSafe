#include "Inc/pwm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO 5
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT
#define LEDC_FREQ 4000

static uint32_t duty = 0;
static const uint32_t duty_max = (1 << LEDC_DUTY_RES) - 1;

void pwm_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&t);

    ledc_channel_config_t c = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_OUTPUT_IO,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&c);
}

void pwm_task(void *arg)
{
    while (1) { 
        duty = (duty + 150 <= duty_max) ? duty + 150 : 0;
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
void update_pwm(uint8_t value)
{
    if (value > 100) value = 100;

    uint32_t scaled = ((uint32_t)value * duty_max) / 100;

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, scaled);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    duty = scaled;
}
