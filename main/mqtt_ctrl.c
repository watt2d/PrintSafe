/**
 * @file mqtt_ctrl.c
 * @brief Gestion MQTT : abonnement, réception des commandes,
 *        publication périodique température / humidité / aliveness.
 */

#include "Inc/mqtt_ctrl.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "Inc/periph_Handle.h"
#include "Inc/pwm.h"
#include "driver/gpio.h"

static const char *TAG = "MQTT";

/* -------------------------------------------------------------------------- */
/*                          HANDLE MQTT GLOBAL                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Handle MQTT partagé par tout le module.
 */
static esp_mqtt_client_handle_t mqtt_client = NULL;

/* -------------------------------------------------------------------------- */
/*                        FONCTIONS INTERNES                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Comparaison stricte d'un topic MQTT.
 *
 * @param[in] ev  Évènement MQTT reçu.
 * @param[in] t   Topic attendu.
 *
 * @return 1 si correspondance exacte, 0 sinon.
 */
static int topic_eq(esp_mqtt_event_handle_t ev, const char *t)
{
    size_t n = strlen(t);
    return (ev->topic_len == n && strncmp(ev->topic, t, n) == 0);
}

/**
 * @brief Handler principal MQTT.
 *
 * @param args   Non utilisé.
 * @param base   Base d’évènement.
 * @param id     ID d’évènement MQTT.
 * @param data   Données d’évènement.
 *
 * @details
 * - Lors de la connexion : souscrit aux topics nécessaires et publie l’adresse MAC.
 * - Lors de la réception de données : met à jour `system_data` et appelle `system_logic()`
 *   si le mode est manuel.
 */
static void handler(void *args, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t ev = data;
    esp_mqtt_client_handle_t c = ev->client;

    switch (id) {

        case MQTT_EVENT_CONNECTED: {

            esp_mqtt_client_subscribe(c, "/PrintSafe/Mode", 0);
            esp_mqtt_client_subscribe(c, "/PrintSafe/Cmd/Temperature", 0);
            esp_mqtt_client_subscribe(c, "/PrintSafe/Cmd/Fan", 0);
            esp_mqtt_client_subscribe(c, "/PrintSafe/Cmd/Time", 0);
            esp_mqtt_client_subscribe(c, "/PrintSafe/Cmd/ResOn", 0);


            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_STA);

            char payload[32];
            snprintf(payload, sizeof(payload),
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

            esp_mqtt_client_publish(c, "/PrintSafe/IsConnected", payload, 0, 1, 0);
            break;
        }

        case MQTT_EVENT_DATA: {

            char data_str[128];
            memset(data_str, 0, sizeof(data_str));
            memcpy(data_str, ev->data, ev->data_len);

            if (topic_eq(ev, "/PrintSafe/Mode")) {
                printf("\n");
                if(atoi(data_str) == 0){
                    system_data.mode = MANUAL;   
                }else if (atoi(data_str) == 1){
                    system_data.mode = AUTOMATIC;
                }else if (atoi(data_str) == 2){
                    system_data.mode = STOP;
                }else{
                    printf("invalid\n");
                }
                system_logic();
                esp_mqtt_client_publish(c, "/PrintSafe/Mode/Ack", "4", 0, 1, 0);
            } else if (topic_eq(ev, "/PrintSafe/Cmd/Temperature")) {
                system_data.cmd.temperature = atoi(data_str);
            } else if (topic_eq(ev, "/PrintSafe/Cmd/Fan")) {
                system_data.cmd.fan = atoi(data_str);
                update_pwm_Fan(system_data.cmd.fan);
                update_pwm_Res(system_data.cmd.fan);
            } else if (topic_eq(ev, "/PrintSafe/Cmd/Time")) {
                system_data.cmd.time_s = atoi(data_str);
            }else if (topic_eq(ev, "/PrintSafe/Cmd/ResOn")) {
                printf("\n");
                if(atoi(data_str) == 0){
                    gpio_set_level(GPIO_NUM_26, 0);
                    printf("OFF\n");
                }else if (atoi(data_str) == 1){
                    gpio_set_level(GPIO_NUM_26, 1);
                    printf("ON\n");
                }
            }

            break;
        }

        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */
/*                     TACHE PERIODIQUE DE PUBLICATION                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Tâche FreeRTOS : envoi périodique de température, HR et signal de vie.
 *
 * @param arg Non utilisé.
 *
 * @details
 * Publie toutes les 5 secondes :
 * - `/PrintSafe/Temp` = température système
 * - `/PrintSafe/HR`   = humidité
 * - `/PrintSafe/IsAlive` = compteur cyclique 16 bits
 */
void mqtt_SendTAndHR(void *arg)
{
    char buf[16];
    uint16_t i = 0;
    float t, h;

    while (1) {

        if (mqtt_client) {

            if (sht41_measure(&t, &h) == ESP_OK) {
                // Mise à jour du système si tu veux
                system_data.temperature = (int)t;
                system_data.hr = (int)h;

                sprintf(buf, "%.2f", t);
                esp_mqtt_client_publish(mqtt_client, "/PrintSafe/Temp", buf, 0, 1, 0);

                sprintf(buf, "%.2f", h);
                esp_mqtt_client_publish(mqtt_client, "/PrintSafe/HR", buf, 0, 1, 0);
            }
        }

        if (++i == 65535) i = 0;

        sprintf(buf, "%u", i);
        esp_mqtt_client_publish(mqtt_client, "/PrintSafe/IsAlive", buf, 0, 1, 0);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* -------------------------------------------------------------------------- */
/*                              INITIALISATION MQTT                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialise et démarre le client MQTT.
 *
 * @details
 * - Crée la configuration du broker.
 * - Initialise le client.
 * - Enregistre le handler d’évènements.
 * - Lance le client MQTT.
 */
void mqtt_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_URI,
    };

    mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}
