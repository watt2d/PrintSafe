#include "Inc/pwm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Timer commun */
#define LEDC_TIMER       LEDC_TIMER_0
#define LEDC_MODE        LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES    LEDC_TIMER_13_BIT
#define LEDC_FREQ        4000

/* PWM 1 */
#define PWM1_GPIO        5
#define PWM1_CHANNEL     LEDC_CHANNEL_0

/* PWM 2 */
#define PWM2_GPIO        18
#define PWM2_CHANNEL     LEDC_CHANNEL_1

static const uint32_t duty_max = (1 << LEDC_DUTY_RES) - 1;

/* ---------- INIT ---------- */
void pwm_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch0 = {
        .speed_mode = LEDC_MODE,
        .channel    = PWM1_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .gpio_num   = PWM1_GPIO,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&ch0);

    ledc_channel_config_t ch1 = {
        .speed_mode = LEDC_MODE,
        .channel    = PWM2_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .gpio_num   = PWM2_GPIO,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&ch1);
}
static inline void pwm_set(uint8_t percent, ledc_channel_t ch)
{
    if (percent > 100) percent = 100;
    uint32_t duty = (percent * duty_max) / 100;
    ledc_set_duty(LEDC_MODE, ch, duty);
    ledc_update_duty(LEDC_MODE, ch);
}

void update_pwm_Fan(uint8_t value)
{
    pwm_set(value, PWM1_CHANNEL);
}

void update_pwm_Res(uint8_t value)
{
    pwm_set(value, PWM2_CHANNEL);
}
