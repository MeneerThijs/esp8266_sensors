#ifndef __USER_KWH_SENSOR_H__
#define __USER_KWH_SENSOR_H__

//#define KWH_CHANNEL (GPIO_ID_PIN(2))
//#define KWH_SENSOR_MUX PERIPHS_IO_MUX_GPIO2_U
//#define KWH_SENSOR_FUNC FUNC_GPIO2

#define KWH_CHANNEL (GPIO_ID_PIN(12))
#define KWH_SENSOR_MUX PERIPHS_IO_MUX_MTDI_U
#define KWH_SENSOR_FUNC FUNC_GPIO12

/*
struct kwh_param {
    uint32 ;
    os_timer_t key_5s;
    os_timer_t key_50ms;
    key_function short_press;
    key_function long_press;
};
*/

void user_kwh_sensor_init();

#endif
