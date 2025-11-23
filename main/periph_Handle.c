#include "Inc/periph_Handle.h"
#include "Inc/pwm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define HEATER_GPIO    GPIO_NUM_25      // placeholder 
#define TEMP_OFFSET_P  5                // offset +5 % 
#define TEMP_DELAY_MS  3000             // délai de stabilisation thermique
static TaskHandle_t temp_task_handle = NULL;

data_struct system_data = {0};

static void set_target_temperature(void *arg)
{
    uint8_t desired_temperature = (uint8_t)(uintptr_t)arg;
    uint8_t threshold = desired_temperature + (desired_temperature * TEMP_OFFSET_P) / 100;

    gpio_set_direction(HEATER_GPIO, GPIO_MODE_OUTPUT);

    while (1) {

        uint8_t t = 100;

        if (t < threshold) {
            gpio_set_level(HEATER_GPIO, 1);
        } else {
            gpio_set_level(HEATER_GPIO, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(TEMP_DELAY_MS));
    }
}

static void control_temp_task(bool start, uint8_t temp)
{
    if (temp_task_handle != NULL) {
        vTaskDelete(temp_task_handle);
        temp_task_handle = NULL;
    }

    if (start) {
        xTaskCreate(set_target_temperature,"temp_ctrl",4096,(void*)(uintptr_t)temp,5,&temp_task_handle);
    }
}



void system_logic(void)
{
    if (system_data.mode == 0) {         // MANUAL
        control_temp_task(true, system_data.cmd.temperature);
        update_pwm(system_data.cmd.fan);
        return;
    }

    if (system_data.mode == 1) {         // AUTOMATIC
        control_temp_task(false, 0);
        //TODO create task to manage automatic mode
        return;
    }
}



