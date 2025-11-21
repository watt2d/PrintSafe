#include "Inc/mqtt_ctrl.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "Inc/periph_Handle.h"
#include "Inc/pwm.h"
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

            esp_mqtt_client_subscribe(c, "/PrintSafe/Mode", 0);
            esp_mqtt_client_subscribe(c, "/PrintSafe/Manual/DesiredTemp", 0);
            esp_mqtt_client_subscribe(c, "/PrintSafe/Manual/DesiredFan", 0);
            esp_mqtt_client_subscribe(c, "/PrintSafe/Automatic/DesiredTemp", 0);
            esp_mqtt_client_subscribe(c, "/PrintSafe/Automatic/DesiredFan", 0);


            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_STA);

            char payload[32];
            snprintf(payload, sizeof(payload),
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

            esp_mqtt_client_publish(c, "/PrintSafe/IsConnected",payload, 0, 1, 0);

            break;
        }

        case MQTT_EVENT_DATA: {

            char data_str[128];
            memset(data_str, 0, sizeof(data_str));
            memcpy(data_str, ev->data, ev->data_len);

            if (topic_eq(ev, "/PrintSafe/Mode")) {
                printf("Mode=%s\n", data_str);
                system_data.mode = (uint8_t)atoi(data_str);
            }else if (topic_eq(ev, "/PrintSafe/Manual/DesiredTemp")) {
                printf("DesiredTemp=%s\n", data_str);
                system_data.manual.desired_temperature = (uint8_t)atoi(data_str);
            }else if (topic_eq(ev, "/PrintSafe/Manual/DesiredFan")) {
                system_data.manual.desired_fan   = (uint8_t)atoi(data_str);
                printf("FAN SPEED=%s\n", data_str);
                update_pwm(system_data.manual.desired_fan);
            }else if (topic_eq(ev, "/PrintSafe/Automatic/DesiredTemp")){
                system_data.automatic.desired_fan   = (uint8_t)atoi(data_str);
            }else if (topic_eq(ev, "/PrintSafe/Automatic/DesiredFan")){
                system_data.automatic.desired_fan   = (uint8_t)atoi(data_str);
            }else if (topic_eq(ev, "/PrintSafe/Automatic/DesiredTime")){
                system_data.automatic.desired_fan   = (uint8_t)atoi(data_str);
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
    char buf[16];
    uint16_t i = 0;
    while (1) {

        if (mqtt_client) {
            sprintf(buf, "%d", system_data.temperature);
            esp_mqtt_client_publish(mqtt_client,"/PrintSafe/Temp", buf, 0, 1, 0);
            sprintf(buf, "%d", system_data.hr);
            esp_mqtt_client_publish(mqtt_client,"/PrintSafe/HR",   buf, 0, 1, 0);
        }
        if (i < 65535){
            i++;
        }else{
            i = 0;
        }
        sprintf(buf, "%d", i);
        esp_mqtt_client_publish(mqtt_client,"/PrintSafe/IsAlive",   buf, 0, 1, 0);


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
