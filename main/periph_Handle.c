/**
 * @file system_logic.c
 * @brief Gestion des modes MANUAL / AUTOMATIC et pilotage du chauffage + ventilateur.
 */

#include "Inc/periph_Handle.h"
#include "Inc/pwm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

#define HEATER_GPIO    GPIO_NUM_25     /**< GPIO de commande chauffage */
#define TEMP_OFFSET_P  5               /**< Décalage +5% sur consigne température */
#define TEMP_DELAY_MS  3000            /**< Délai de mise à jour du chauffage */

#define I2C_PORT        0
#define SDA_PIN         21
#define SCL_PIN         22
#define SHT41_ADDR      0x44

static TaskHandle_t temp_task_handle = NULL;

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

        uint8_t t = 100;   /* Placeholder en attendant lecture capteur */

        if (t < threshold)
            gpio_set_level(HEATER_GPIO, 1);
        else
            gpio_set_level(HEATER_GPIO, 0);

        vTaskDelay(pdMS_TO_TICKS(TEMP_DELAY_MS));
    }
}

/**
 * @brief Démarrage/arrêt de la tâche de régulation du chauffage.
 *
 * @param start  true : lance la tâche ; false : la stoppe.
 * @param temp   Température de consigne à passer à la tâche.
 */
static void control_temp_task(bool start, uint8_t temp)
{
    if (temp_task_handle != NULL) {
        vTaskDelete(temp_task_handle);
        temp_task_handle = NULL;
    }

    if (start) {
        xTaskCreate(
            set_target_temperature,
            "temp_ctrl",
            4096,
            (void*)(uintptr_t)temp,
            5,
            &temp_task_handle
        );
    }
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
    if (system_data.mode == 0) {         /* MANUAL */
        control_temp_task(true, system_data.cmd.temperature);
        update_pwm(system_data.cmd.fan);
        return;
    }

    if (system_data.mode == 1) {         /* AUTOMATIC */
        control_temp_task(false, 0);
        /* TODO : implémentation du mode automatique */
        return;
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
    uint8_t cmd[2] = {0xFD, 0x00};      // High precision measurement (SHT41 uses 0xFD only; second byte ignored)
    ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_PORT, SHT41_ADDR, cmd, 1, 20 / portTICK_PERIOD_MS));
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t data[6] = {0};
    ESP_ERROR_CHECK(i2c_master_read_from_device(I2C_PORT, SHT41_ADDR, data, 6, 20 / portTICK_PERIOD_MS));

    uint16_t rawT = (data[0] << 8) | data[1];
    uint16_t rawH = (data[3] << 8) | data[4];

    *t = -45 + 175 * ((float)rawT / 65535.0f);
    *h = 100 * ((float)rawH / 65535.0f);

    return ESP_OK;
}

