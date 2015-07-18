#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "gpio.h"

#include "user_esp_platform.h"
#include "user_kwh_sensor.h"

void user_rf_pre_init(void) {
}

void ICACHE_FLASH_ATTR
user_init(void) {
	uart_div_modify(0, UART_CLK_FREQ / UART_BIT_RATE);
	system_init_done_cb(user_esp_platform_init);
}
