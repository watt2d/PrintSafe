#ifndef AUTOMATIC_HANDLE_H
#define AUTOMATIC_HANDLE_H

#include <stdint.h>
#include <esp_err.h>

typedef struct {

    /* État + mesures */
    uint8_t temperature;
    uint8_t hr;
    uint8_t fan_speed;
    uint8_t mode;
    struct {
        uint8_t temperature;
        uint8_t fan;
        uint32_t time_s;
    } cmd;

} data_struct;


extern data_struct system_data;
extern void system_logic(void);
void i2c_init(void);
esp_err_t sht41_measure(float *t, float *h);

#endif
