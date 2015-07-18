#ifndef __USER_DEVICE_H__
#define __USER_DEVICE_H__

#include "gpio.h"
#include "user_config.h"
#include "user_data_buffer.h"

#define USER_TASK_QUEUE_LEN 5
#define HEAP_RESERVE 5000

#define GPIO_IRQ 1

typedef enum {
	GPIO_TASK = 0, TEMPERATURE_TASK = 2, MISC_TASK = 99
} TASK_TYPE;

typedef enum {
	GPIO_PIN_PULLUP = 0,
//	GPIO_PIN_PULLDOWN,
	GPIO_PIN_FLOAT
} GPIO_INPUT_TYPE;

typedef void (*cb_function)(os_event_t *);

struct sensor {
	uint32 gpio_id;
	cb_function handler;
	struct sensor *next;
};

struct message {
	uint32_t rtc;
	uint32 gpio_id;
	cb_function handler;
};

#ifdef DEBUG
#define DEBUG_LOCATION os_printf("%s:%d\r\n", __FILE__, __LINE__)
#define DEBUG_HEAP os_printf("%s:%d %d\r\n", __FILE__, __LINE__, system_get_free_heap_size())
#else
#define DEBUG_LOCATION
#define DEBUG_HEAP
#endif

//#define FREE_HEAP system_get_free_heap_size() < HEAP_RESERVE ? 0 : system_get_free_heap_size() // allocating the full heap causes RESET (SDK 1.0.1)

void user_register_sensor(uint32 gpio_id, uint32 gpio_name, uint32 gpio_func,
		GPIO_INPUT_TYPE input_type, GPIO_INT_TYPE intr_state,
		cb_function handler);

void user_esp_platform_init();

#endif
