/**
 * @file periph_Handle.c
 * @brief Régulation thermique par PID – modes MANUAL / AUTOMATIC / STOP
 */

#include "Inc/periph_Handle.h"
#include "Inc/pwm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/*                                CONSTANTES                                  */
/* -------------------------------------------------------------------------- */

#define PID_DT_MS          500
#define HEATER_PWM_MAX     100.0f

#define I2C_PORT           0
#define SDA_PIN            21
#define SCL_PIN            22
#define SHT41_ADDR         0x44

/* -------------------------------------------------------------------------- */
/*                             STRUCTURES PID                                  */
/* -------------------------------------------------------------------------- */

typedef struct {
    float kp;
    float ki;
    float kd;
    float integral;
    float prev_error;
    float out_min;
    float out_max;
} pid_t;

/* -------------------------------------------------------------------------- */
/*                            DONNÉES GLOBALES                                 */
/* -------------------------------------------------------------------------- */

data_struct system_data = {0};

static pid_t heater_pid = {
    .kp = 3.0f,
    .ki = 0.2f,
    .kd = 0.5f,
    .integral = 0.0f,
    .prev_error = 0.0f,
    .out_min = 0.0f,
    .out_max = HEATER_PWM_MAX
};

static float current_setpoint = 0.0f;
static bool heater_enabled = false;

static TaskHandle_t heater_task_handle = NULL;

/* -------------------------------------------------------------------------- */
/*                             PID CORE                                        */
/* -------------------------------------------------------------------------- */

static float pid_compute(pid_t *pid, float setpoint, float measured, float dt)
{
    float error = setpoint - measured;

    pid->integral += error * dt;
    float derivative = (error - pid->prev_error) / dt;

    float output =
        pid->kp * error +
        pid->ki * pid->integral +
        pid->kd * derivative;

    pid->prev_error = error;

    if (output > pid->out_max) output = pid->out_max;
    if (output < pid->out_min) output = pid->out_min;

    return output;
}

/* -------------------------------------------------------------------------- */
/*                       TÂCHE RÉGULATION CHAUFFAGE                            */
/* -------------------------------------------------------------------------- */

static void heater_control_task(void *arg)
{
    const float dt = PID_DT_MS / 1000.0f;

    while (1) {
        if (!heater_enabled) {
            update_pwm_Res(0);
            vTaskDelay(pdMS_TO_TICKS(PID_DT_MS));
            continue;
        }

        float pwm =pid_compute(&heater_pid,current_setpoint,system_data.temperature,dt);

        update_pwm_Res((uint8_t)pwm);

        vTaskDelay(pdMS_TO_TICKS(PID_DT_MS));
    }
}

/* -------------------------------------------------------------------------- */
/*                      TÂCHE TIMER MODE AUTOMATIQUE                           */
/* -------------------------------------------------------------------------- */

static void automatic_timer_task(void *arg)
{
    uint32_t duration_ms = system_data.cmd.time_s * 1000;
    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    heater_enabled = false;
    current_setpoint = 0.0f;

    vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------- */
/*                           LOGIQUE SYSTÈME                                   */
/* -------------------------------------------------------------------------- */

void system_logic(void)
{
    if (heater_task_handle == NULL) {
        xTaskCreate(heater_control_task,"heater_pid",4096,NULL,5,&heater_task_handle);
    }

    heater_pid.integral = 0.0f;
    heater_pid.prev_error = 0.0f;

    switch (system_data.mode) {

    case MANUAL:
        current_setpoint = system_data.cmd.temperature;
        heater_enabled = true;
        break;

    case AUTOMATIC:
        current_setpoint = system_data.cmd.temperature;
        heater_enabled = true;
        xTaskCreate(automatic_timer_task,"auto_timer",2048,NULL,4,NULL);
        break;

    case STOP:
    default:
        heater_enabled = false;
        current_setpoint = 0.0f;
        update_pwm_Res(0);
        break;
    }
}

/* -------------------------------------------------------------------------- */
/*                           I2C / SHT41                                       */
/* -------------------------------------------------------------------------- */

void i2c_init(void)
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, cfg.mode, 0, 0, 0));
}

esp_err_t sht41_measure(float *t, float *h)
{
    esp_err_t err;
    uint8_t cmd = 0xFD;

    err = i2c_master_write_to_device(
        I2C_PORT,
        SHT41_ADDR,
        &cmd,
        1,
        pdMS_TO_TICKS(50)
    );
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t data[6] = {0};
    err = i2c_master_read_from_device(
        I2C_PORT,
        SHT41_ADDR,
        data,
        6,
        pdMS_TO_TICKS(50)
    );
    if (err != ESP_OK) return err;

    uint16_t rawT = (data[0] << 8) | data[1];
    uint16_t rawH = (data[3] << 8) | data[4];

    *t = -45.0f + 175.0f * ((float)rawT / 65535.0f);
    *h = 100.0f * ((float)rawH / 65535.0f);

    return ESP_OK;
}
