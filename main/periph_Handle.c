/**
 * @file system_logic.c
 * @brief Gestion GPIO (I2C, PWM)
 */

#include "Inc/periph_Handle.h"
#include "Inc/pwm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define HEATER_GPIO    GPIO_NUM_25     /**< GPIO de commande chauffage */
#define TEMP_OFFSET_P  5               /**< Décalage +5% sur consigne température */
#define TEMP_DELAY_MS  3000            /**< Délai de mise à jour du chauffage */

#define I2C_PORT        0
#define SDA_PIN         21
#define SCL_PIN         22
#define SHT41_ADDR      0x44

static TaskHandle_t temp_task_handle = NULL;
static TaskHandle_t auto_task_handle = NULL;

/**
 * @brief Structure système globale contenant état, commandes et mesures.
 */
data_struct system_data = {0};

/* -------------------------------------------------------------------------- */
/*                          TÂCHE DE CONTRÔLE CHAUFFAGE                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Tâche FreeRTOS pilotant le chauffage selon une consigne.
 *
 * @param arg Pointeur casté contenant la température désirée.
 *
 * @details
 * - Applique un seuil augmenté de TEMP_OFFSET_P%.
 * - Active/désactive le GPIO du chauffage toutes les TEMP_DELAY_MS ms.
 */
static void set_target_temperature(void *arg)
{
    uint8_t desired_temperature = (uint8_t)(uintptr_t)arg;
    uint8_t threshold = desired_temperature + (desired_temperature * TEMP_OFFSET_P) / 100;

    gpio_set_direction(HEATER_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        if (system_data.temperature < threshold)
            gpio_set_level(HEATER_GPIO, 1);
        else
            gpio_set_level(HEATER_GPIO, 0);

        vTaskDelay(pdMS_TO_TICKS(TEMP_DELAY_MS));
    }
}

/* -------------------------------------------------------------------------- */
/*                         TÂCHE MODE AUTOMATIQUE                              */
/* -------------------------------------------------------------------------- */

static void automatic_mode_task(void *arg)
{
    uint8_t target = system_data.cmd.temperature;
    uint32_t duration_ms = system_data.cmd.time_s * 1000;
    uint32_t elapsed = 0;

    uint8_t threshold = target + (target * TEMP_OFFSET_P) / 100;

    gpio_set_direction(HEATER_GPIO, GPIO_MODE_OUTPUT);

    while (elapsed < duration_ms) {
        if (system_data.temperature < threshold)
            gpio_set_level(HEATER_GPIO, 1);
        else
            gpio_set_level(HEATER_GPIO, 0);

        vTaskDelay(pdMS_TO_TICKS(TEMP_DELAY_MS));
        elapsed += TEMP_DELAY_MS;
    }

    /* Fin du temps → arrêt chauffage */
    gpio_set_level(HEATER_GPIO, 0);

    auto_task_handle = NULL;
    vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------- */
/*                              LOGIQUE SYSTÈME                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Logique principale du système selon le mode sélectionné.
 *
 * @details
 * - MANUAL : active la tâche de chauffage + applique PWM ventilateur.
 * - AUTOMATIC : désactive la régulation manuelle (logique non implémentée).
 */
void system_logic(void)
{
    if (temp_task_handle != NULL) {
        vTaskDelete(temp_task_handle);
        temp_task_handle = NULL;
    }
    if (system_data.mode == MANUAL) {         /* MANUAL */
        //control_temp_task(MANUAL, system_data.cmd.temperature);
        printf("MANUAL\n");
        xTaskCreate(set_target_temperature,"temp_ctrl",4096,(void*)(uintptr_t)system_data.cmd.temperature,5,&temp_task_handle);
        return;
    }else if (system_data.mode == AUTOMATIC) {
    printf("AUTOMATIC\n");
    xTaskCreate(automatic_mode_task,"auto_ctrl",4096,NULL,5,&auto_task_handle);
    return;
    }else if (system_data.mode == STOP){
        printf("STOP\n");
        update_pwm(0);
    }
}
/**
 * @brief Initialise l’I2C + lance la tâche de mesure SHT41.
 *
 * @details
 * Appeler depuis app_main() ou tout module d’initialisation.
 */
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
    uint8_t cmd = 0xFD;   // High precision measurement

    err = i2c_master_write_to_device(
        I2C_PORT,
        SHT41_ADDR,
        &cmd,
        1,
        pdMS_TO_TICKS(50)
    );
    if (err != ESP_OK) return err;

    // Datasheet-safe delay
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


