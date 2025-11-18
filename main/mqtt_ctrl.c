#include "Inc/mqtt_ctrl.h"
#include "esp_log.h"
#include "esp_event.h"

static const char *TAG = "MQTT_LED";

static void handler(void *args, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t ev = data;
    esp_mqtt_client_handle_t c = ev->client;

    switch (id) {
        case MQTT_EVENT_CONNECTED:
            esp_mqtt_client_subscribe(c, "/Watteu/Task1", 0);
            esp_mqtt_client_publish(c, "/Watteu/Task2", "Hello ESP32", 0, 1, 0);
            break;
        case MQTT_EVENT_DATA:
            esp_mqtt_client_publish(c, "/Watteu/Task2", "Hello Watteu", 0, 1, 0);
            break;
        default:
            break;
    }
}

void mqtt_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_URI,
    };
    esp_mqtt_client_handle_t c = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(c, ESP_EVENT_ANY_ID, handler, NULL);
    esp_mqtt_client_start(c);
}
