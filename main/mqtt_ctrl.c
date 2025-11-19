#include "Inc/mqtt_ctrl.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"

static const char *TAG = "MQTT";

/* Handle MQTT partagé par tout le module */
static esp_mqtt_client_handle_t mqtt_client = NULL;

/* Compare stricte du topic */
static int topic_eq(esp_mqtt_event_handle_t ev, const char *t)
{
    size_t n = strlen(t);
    return (ev->topic_len == n && strncmp(ev->topic, t, n) == 0);
}

/* Handler MQTT */
static void handler(void *args, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t ev = data;
    esp_mqtt_client_handle_t c = ev->client;

    switch (id) {

        case MQTT_EVENT_CONNECTED: {

            esp_mqtt_client_subscribe(c, "/Printsafe/Mode", 0);
            esp_mqtt_client_subscribe(c, "/Printsafe/Manual/DesiredTemp", 0);
            esp_mqtt_client_subscribe(c, "/Printsafe/Manual/DesiredFan", 0);
            esp_mqtt_client_subscribe(c, "/Printsafe/Automatic/DesiredTemp", 0);
            esp_mqtt_client_subscribe(c, "/Printsafe/Automatic/DesiredFan", 0);


            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_STA);

            char payload[32];
            snprintf(payload, sizeof(payload),
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

            esp_mqtt_client_publish(c, "/PrintSafe/IsConnected",
                                    payload, 0, 1, 0);

            break;
        }

        case MQTT_EVENT_DATA: {

            char data_str[128];
            memset(data_str, 0, sizeof(data_str));
            memcpy(data_str, ev->data, ev->data_len);

            if (topic_eq(ev, "/Printsafe/Mode")) {
                printf("Mode=%s\n", data_str);
            }
            else if (topic_eq(ev, "/Printsafe/Manual/DesiredTemp")) {
                printf("DesiredTemp=%s\n", data_str);
            }
            else if (topic_eq(ev, "/Printsafe/Manual/DesiredFan")) {
                printf("DesiredFan=%s\n", data_str);
            }

            break;
        }

        default:
            break;
    }
}

/* Task d'envoi périodique de T° et HR */
void mqtt_SendTAndHR(void *arg)
{
    while (1) {

        if (mqtt_client) {

            esp_mqtt_client_publish(mqtt_client,
                    "/PrintSafe/Temp", "1", 0, 1, 0);

            esp_mqtt_client_publish(mqtt_client,
                    "/PrintSafe/HR",   "2", 0, 1, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* Init MQTT */
void mqtt_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_URI,
    };

    mqtt_client = esp_mqtt_client_init(&cfg);

    esp_mqtt_client_register_event(mqtt_client,
                                   ESP_EVENT_ANY_ID,
                                   handler,
                                   NULL);

    esp_mqtt_client_start(mqtt_client);
}
