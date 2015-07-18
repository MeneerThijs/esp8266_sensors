#include "user_interface.h"
#include "osapi.h"
#include "gpio.h"

//#include "sntp.h"

#include "user_esp_platform.h"
#include "user_kwh_sensor.h"

LOCAL uint32 last_neg_edge = 0;

LOCAL void ICACHE_FLASH_ATTR
user_kwh_event_handler(os_event_t *event) {
	int i;
	uint32 rotation_period = event->par - last_neg_edge; // Don't worry about wrapping, it just works.

	if (event->sig == KWH_CHANNEL) {
		uint32 trigger_offset = event->par - last_neg_edge;
		if ((rotation_period < 500000)) {
			os_printf("Discarded edge trigger: rotation period < 500ms\r\n");
			return;
		} else if (rotation_period > 45000000) {
			os_printf("Discarded edge trigger: rotation period > 45s\r\n");
			last_neg_edge = event->par; // otherwise we'll never trigger again.
			return;
		} else {
			last_neg_edge = event->par;
			for (i = 0; i < 300; i++) {
				user_put_sample(event->par, KWH_SAMPLE,
						(rotation_period / 10000));
			}

//			if (!user_put_sample(event->par, KWH_SAMPLE, (rotation_period / 10000)))
//				os_printf("%s:%d SAMPLE NOT SAVED\r\n", __FILE__, __LINE__);

			os_printf("Rotation period: %u.%u %u\r\n",
					rotation_period / 1000000, (rotation_period / 10000) % 100,
					rotation_period / 1000);
		}
	}
//	os_printf("Task sig=%u, par=%u, count=%d\r\n", event->sig, event->par,
//			counter++);
}

LOCAL void ICACHE_FLASH_ATTR
user_dummy_handler(os_event_t *event) {
	uint32 current_stamp;

	os_printf("%s:%d signal:%u parameter:%u\r\n", __FILE__, __LINE__,
			event->sig, event->par);

	current_stamp = sntp_get_current_timestamp();
	os_printf("sntp: %d, %s \r\n", current_stamp,
			sntp_get_real_time(current_stamp));
}

void ICACHE_FLASH_ATTR
user_kwh_sensor_init() {

//	user_register_sensor(12, PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12,
//			GPIO_PIN_PULLDOWN, GPIO_PIN_INTR_NEGEDGE, user_dummy_handler);
//	user_register_sensor(13, PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13,
//			GPIO_PIN_PULLDOWN, GPIO_PIN_INTR_NEGEDGE, user_dummy_handler);
	user_register_sensor(KWH_CHANNEL, KWH_SENSOR_MUX, KWH_SENSOR_FUNC,
			GPIO_PIN_FLOAT, GPIO_PIN_INTR_NEGEDGE, user_kwh_event_handler);
}
