#include "user_interface.h"
#include "osapi.h"
#include "mem.h"
#include "sntp.h"

#include "user_esp_platform.h"
#include "user_kwh_sensor.h"
#include "user_webserver.h"

LOCAL os_event_t user_task_queue[USER_TASK_QUEUE_LEN];
LOCAL struct sensor *sensors_root = NULL;

LOCAL void // don't use ICACHE_FLASH_ATTR in the interrupt handler
user_irq_handler(uint8 class) {
	if (class == GPIO_IRQ) {
		uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
		struct sensor *node = sensors_root;
		struct message *msg;

		//disable interrupt
		//	gpio_pin_intr_state_set(TRACKER_PIN, GPIO_PIN_INTR_DISABLE);

		while (node != NULL ) {
			if (gpio_status & BIT(node->gpio_id)) {
				//clear interrupt status
				GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS,
						gpio_status & BIT(node->gpio_id));

				msg = (struct message*) os_malloc(sizeof(struct message));
				msg->rtc = system_get_time();
				msg->gpio_id = node->gpio_id;
				msg->handler = node->handler;

				uint32 rtc_now = system_get_time();

#ifdef DEBUG
				os_printf("%s:%d gpio_id:%u handler:%p\r\n", __FILE__, __LINE__,
						node->gpio_id, node->handler);
				if (!system_os_post(USER_TASK_PRIO_0, GPIO_TASK,
						(os_param_t) msg))
					os_printf("Task \"post\" failed\r\n");

				else
					os_printf("Task \"post\" succeed, RTC=%u, GPIO=%u\r\n",
							rtc_now, gpio_status);
#endif
			}
			node = node->next;
		}

		//clear interrupt status
		//		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);

		//	if (GPIO_INPUT_GET(TRACKER_PIN) == 0) {
		//		gpio_pin_intr_state_set(TRACKER_PIN, GPIO_PIN_INTR_POSEDGE);
		//		os_printf("Pos edge\r\n");
		//	} else {
		//		gpio_pin_intr_state_set(TRACKER_PIN, GPIO_PIN_INTR_NEGEDGE);
		//		os_printf("Neg edge\r\n");
		//	}
	}
}

LOCAL void ICACHE_FLASH_ATTR
user_task_handler(os_event_t *event) {
	struct message *msg;

	if (event->sig == GPIO_TASK) {
		os_printf("%s:%d\r\n", __FILE__, __LINE__);
		msg = (struct message *) event->par;

		event->sig = msg->gpio_id;
		event->par = msg->rtc;
		os_printf("%s:%d sig:%u par:%p\r\n", __FILE__, __LINE__, msg->rtc,
				msg->handler);
		if (msg->handler != NULL) {
			msg->handler(event);
		} else {
			os_printf("Callback for IRQ is NULL\r\n");
		}
		os_free(msg);
	} else if (event->sig == MISC_TASK) {
		void (*handler)(void);
		os_printf("%s:%d\r\n", __FILE__, __LINE__);
		handler = (void *) event->par;
		handler();
	}
}

void ICACHE_FLASH_ATTR
user_register_sensor(uint32 gpio_id, uint32 gpio_name, uint32 gpio_func,
		GPIO_INPUT_TYPE input_type, GPIO_INT_TYPE intr_state,
		cb_function handler) {
	struct sensor *node = sensors_root;
	struct sensor *ptr;

#ifdef DEBUG
	os_printf("Registering sensor gpio_id:%u state:%d handler:%p\r\n", gpio_id,
			intr_state, handler);
#endif
	PIN_FUNC_SELECT(gpio_name, gpio_func);
//	gpio_output_set(0, 0, 0, 1 << gpio_id);
	GPIO_DIS_OUTPUT(gpio_id);

	if (input_type == GPIO_PIN_PULLUP) {
		PIN_PULLUP_EN(gpio_name);
//		PIN_PULLDWN_DIS(gpio_name);
//	} else if (input_type == GPIO_PIN_PULLDOWN) {
//		PIN_PULLUP_DIS(gpio_name);
//		PIN_PULLDWN_EN(gpio_name);
	} else if (input_type == GPIO_PIN_FLOAT) {
		PIN_PULLUP_DIS(gpio_name);
//		PIN_PULLDWN_DIS(gpio_name);
	}

	gpio_pin_intr_state_set(gpio_id, intr_state);

	ptr = (struct sensor*) os_malloc(sizeof(struct sensor));
	ptr->gpio_id = gpio_id;
	ptr->handler = handler;
	ptr->next = NULL;

	if (sensors_root == NULL) {
		sensors_root = ptr;
	} else {
		while (node->next != NULL ) {
			node = node->next;
		}
		node->next = ptr;
	}

#ifdef DEBUG
	node = sensors_root;
	while (node != NULL ) {
		os_printf("%s:%d gpio_id:%u handler:%p\r\n", __FILE__, __LINE__,
				node->gpio_id, node->handler);
		node = node->next;
	}
#endif
}

LOCAL void ICACHE_FLASH_ATTR
user_gpio_init() {
	ETS_GPIO_INTR_ATTACH(user_irq_handler, GPIO_IRQ);
	ETS_GPIO_INTR_ENABLE();
}

LOCAL void ICACHE_FLASH_ATTR
user_wifi_init() {
	static char* ssid = WIFI_AP;
	static char* password = WIFI_PASSWORD;
	static char* ntp_server_0 = NTP_SERVER_0;
	static char* ntp_server_1 = NTP_SERVER_1;

	struct station_config stationConf;
	wifi_set_opmode(STATION_MODE);

	//cpu needs to be running to handle interrupts so no LOCAL_SLEEP_T
	wifi_set_sleep_type(MODEM_SLEEP_T);

	os_memcpy(&stationConf.ssid, ssid, 32); // Set settings
	os_memcpy(&stationConf.password, password, 64); // Set settings
	wifi_station_set_config(&stationConf); // Set wifi conf
	wifi_station_connect();

	//Initialize NTP to set internal clock
	sntp_setservername(0, ntp_server_0);
	sntp_setservername(1, ntp_server_1);
	sntp_init();
}

LOCAL void ICACHE_FLASH_ATTR
user_task_scheduler_init() {
	system_os_task(user_task_handler, USER_TASK_PRIO_0, user_task_queue,
	USER_TASK_QUEUE_LEN);
}

LOCAL void ICACHE_FLASH_ATTR
user_print_sysinfo() {
	os_printf("SDK version:%s built:%s %s\r\n", system_get_sdk_version(),
	__DATE__, __TIME__);
	os_printf("CPU freq. %d\n", system_get_cpu_freq());
	system_print_meminfo();

#ifdef DEBUG
	system_set_os_print(1);
#else
	system_set_os_print(0);
#endif

#ifdef DEBUG
	os_printf("Sleep type: %d\n", wifi_get_sleep_type());
	os_printf("Auto connect: %d\n", wifi_station_get_auto_connect());
	os_printf("Phy mode: %d\n", wifi_get_phy_mode());
#endif
}

void ICACHE_FLASH_ATTR
user_esp_platform_init(void) {
	user_print_sysinfo();

	user_wifi_init();
	user_gpio_init();
	user_task_scheduler_init();
	user_sensor_storage_init();

#ifdef KWH_SENSOR
	user_kwh_sensor_init();
#endif

#ifdef WEBSERVER
	user_webserver_init();
#endif

//	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);// select pin to GPIO 13 mode
//	gpio_output_set(0, BIT12, BIT12, 0);
//	GPIO_EN_OUTPUT(13); // set pin to "input" mode
//	PIN_PULLDWN_EN(PERIPHS_IO_MUX_MTDI_U); // disable pullodwn
//	PIN_PULLUP_DIS(PERIPHS_IO_MUX_MTDI_U); // pull - up pin

//	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2); // select pin to GPIO 13 mode
//	GPIO_DIS_OUTPUT(2); // set pin to "input" mode
//	PIN_PULLDWN_EN(PERIPHS_IO_MUX_GPIO2_U); // disable pullodwn
//	PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO2_U); // pull - up pin

//	gpio_output_set(0, BIT2|BIT13, BIT2|BIT13, 0);
//	gpio_output_set(0, BIT2|BIT13, 0, BIT2|BIT13);
}
