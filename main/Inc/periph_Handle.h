#ifndef AUTOMATIC_HANDLE_H
#define AUTOMATIC_HANDLE_H

#include <stdint.h>

typedef struct {

    /* État + mesures */
    uint8_t system_state;
    uint8_t temperature;
    uint8_t hr;
    uint8_t fan_speed;
    uint8_t mode;

    /* Commandes manuelles */
    struct {
        uint8_t desired_temperature;
        uint8_t desired_fan;
    } manual;

    /* Commandes automatiques */
    struct {
        uint8_t desired_temperature;
        uint8_t desired_fan;
        uint8_t desired_time;
    } automatic;

} data_struct;


extern data_struct system_data;

#endif
