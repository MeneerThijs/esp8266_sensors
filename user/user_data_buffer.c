#include "user_interface.h"
#include "osapi.h"
#include "mem.h"

#include "user_esp_platform.h"
#include "user_data_buffer.h"

struct sample_bank *sample_bank;

uint8 current_bank;
bool wrapped_banks;

/*
 LOCAL void ICACHE_FLASH_ATTR
 user_memtest() {
 struct sample *ptr;
 int i;

 //	s1 = (struct sample*) os_malloc(sizeof(struct sample) * 200);
 //	sample_bank[0] = (struct sample*) os_malloc(sizeof(struct sample) * 1000);

 //	for (i = 0; i < 20; i++) {
 os_printf("%d %d %p heap size:%d\r\n", i++, sizeof(struct sample),
 sample_bank[0], system_get_free_heap_size());

 //	sample_bank[0][0].period = 1234567;
 //	sample_bank[0][0].rtc = 123456789;
 //
 //	sample_bank[0][500].period = 1234567;
 //	sample_bank[0][500].rtc = 123456789;
 //
 //	os_printf("%d %d %d %d\r\n", sample_bank[0][0].period,
 //			sample_bank[0][500].period, sample_bank[0][0].rtc,
 //			sample_bank[0][500].rtc);

 //		if (ptr == NULL) {
 //			os_printf("malloc failed.\r\n");
 //			break;
 //		} else if (i % 10 == 0) {
 //			os_printf("%d\r\n", i, ptr);
 //		}

 //		ptr->period = 1234567;
 //		ptr->rtc = 123456789;
 //		ptr->next = NULL;
 //
 //		if (samples_root == NULL) {
 //			samples_root = ptr;
 //		} else {
 //			struct sample *s = samples_root;
 //			while (s->next != NULL ) {
 //				s = s->next;
 //			}
 //			s->next = ptr;
 //		}
 //	}
 //	system_os_post(USER_TASK_PRIO_0, MISC_TASK, (os_param_t) user_memtest);
 }

 /*
 LOCAL void ICACHE_FLASH_ATTR
 user_free_sample_bank(uint8 bank_number) {
 os_printf("before: %d\r\n", FREE_HEAP);
 os_free(sample_bank[bank_number].samples);
 os_printf("after: %d\r\n", FREE_HEAP);
 }
 */

LOCAL bool ICACHE_FLASH_ATTR
user_alloc_sample_bank(uint8 bank_number) {
	struct sample *ptr;
	uint16 bank_size = SAMPLE_BANK_SIZE;
	uint32 free_heap, required_size;

//	free_heap = FREE_HEAP;

//	os_printf("free: %d required: %d min_required %d\n", free_heap,
//			REQUIRED_SIZE(bank_size), REQUIRED_SIZE(1));

	/* This doesn't work. Free heap is no guarantee for successful malloc.
	 if (free_heap < REQUIRED_SIZE(SAMPLE_BANK_SIZE)) {
	 if (free_heap >= REQUIRED_SIZE(1)) {
	 bank_size = MAX_SAMPLES(free_heap);
	 os_printf("free: %d max_samples: %d\n", free_heap,
	 MAX_SAMPLES(free_heap));
	 } else
	 return FALSE; // not enough heap available
	 }
	 */

	if (!(sample_bank[bank_number].samples = (struct sample*) os_malloc(
			bank_size * SAMPLE_SIZE)))
		return FALSE;
	sample_bank[bank_number].capacity = bank_size;
	sample_bank[bank_number].next_sample = 0;

#ifdef DEBUG
	os_printf("%d samples in bank [%d] allocated @%p\r\n",
			sample_bank[bank_number].capacity, bank_number,
			sample_bank[bank_number].samples);
//	if (bank_number == 2) // Test rotation fall back.
//		return FALSE;
#endif

	return TRUE;
}

LOCAL void ICACHE_FLASH_ATTR
user_rotate_banks() {
	uint16 tmp_capacity = sample_bank[0].capacity;
	struct sample *tmp_samples = sample_bank[0].samples;
	int i = 0;

	os_printf("%s:%d %p %p\r\n", __FILE__, __LINE__, sample_bank[0].samples, sample_bank[1].samples);
	while (i < current_bank) {
		os_printf("%s:%d Wrapping banks: bank[%d] moved to bank[%d]\r\n",
		__FILE__, __LINE__, i + 1, i);
		sample_bank[i].capacity = sample_bank[i + i].capacity;
		sample_bank[i].next_sample = sample_bank[i + 1].next_sample;
		sample_bank[i].samples = sample_bank[++i].samples;
	}
	sample_bank[i].capacity = tmp_capacity;
	sample_bank[i].next_sample = 0;
	sample_bank[i].samples = tmp_samples;
	os_printf("%s:%d %p %p %d\r\n", __FILE__, __LINE__, sample_bank[0].samples, sample_bank[1].samples, i);

	wrapped_banks = TRUE;
}

LOCAL struct sample* ICACHE_FLASH_ATTR
user_get_free_sample_space() {
	struct sample *current_sample;

#ifdef DEBUG
	os_printf("current sample: %d[%d] %p\r\n", current_bank,
			sample_bank[current_bank].next_sample, sample_bank[current_bank].samples);
#endif

	current_sample =
			&sample_bank[current_bank].samples[sample_bank[current_bank].next_sample++];

	if (sample_bank[current_bank].next_sample
			>= sample_bank[current_bank].capacity) {
		DEBUG_LOCATION;
		if (current_bank < SAMPLE_BANKS - 1) {
			DEBUG_LOCATION;
			if (user_alloc_sample_bank(current_bank + 1)) {
				DEBUG_LOCATION;
				current_bank++;
				wrapped_banks = FALSE;
			} else { // Allocation failed. We need to rotate to free the last bank.
				DEBUG_LOCATION;
				user_rotate_banks();
			}
		} else {
			DEBUG_LOCATION;
			user_rotate_banks();
		}
	}

#ifdef DEBUG
	os_printf("next sample: %d[%d] %p\r\n", current_bank,
			sample_bank[current_bank].next_sample, sample_bank[current_bank].samples);
#endif

	return current_sample;
}

bool ICACHE_FLASH_ATTR
user_put_sample(uint32 rtc, SAMPLE_TYPE type, sint16 data) {
	struct sample *s = user_get_free_sample_space();
	if (s) {

		s->type = type;
		s->rtc = rtc;
		s->data = data;

#ifdef DEBUG
		os_printf("saved sample %d %d\r\n", s->type, s->data, s->rtc);
#endif
		return TRUE;
	} else
		return FALSE;
}

uint16 ICACHE_FLASH_ATTR
user_get_sample_count() {
	if (wrapped_banks) {
		return SAMPLE_BANKS * SAMPLE_BANK_SIZE;
	} else {
		return current_bank * SAMPLE_BANK_SIZE
				+ sample_bank[current_bank].next_sample;
	}
	return 0;
}

uint16 ICACHE_FLASH_ATTR
user_get_sample_data(uint16 start_sample, uint16 count, uint32* rtc, uint8* type, sint16* data) {
	uint8 bank_number;
	uint16 sample_number;
	uint16 i;

	uint16 sample_count = user_get_sample_count();

	if (start_sample >= sample_count)
		return 0;

	if (start_sample + count > sample_count)
		count = sample_count - start_sample;

	for (i = start_sample; i < start_sample + count; i++) {
		bank_number = BANK_NUMBER(i);
		sample_number = SAMPLE_NUMBER(i);
		rtc[i] = sample_bank[bank_number].samples[sample_number].rtc;
		type[i] = sample_bank[bank_number].samples[sample_number].type;
		data[i] = sample_bank[bank_number].samples[sample_number].data;
		os_printf("sample: %d[%d]\r\n", bank_number, sample_number);
	}

	return count;
}

uint16 ICACHE_FLASH_ATTR
user_get_samples(uint16 start_sample, uint16 count, struct sample** sample_data) {
	uint8 bank_number;
	uint16 sample_number;
	uint16 i;

	uint16 sample_count = user_get_sample_count();

	if (start_sample >= sample_count)
		return 0;

	if (start_sample + count > sample_count)
		count = sample_count - start_sample;

	for (i = start_sample; i < start_sample + count; i++) {
		bank_number = BANK_NUMBER(i);
		sample_number = SAMPLE_NUMBER(i);
		sample_data[i] = &sample_bank[bank_number].samples[sample_number];
		os_printf("sample: %d[%d]\r\n", bank_number, sample_number);
	}

	return count;
}

/*
 int32 ICACHE_FLASH_ATTR
 user_get_sample() {
 os_printf("First sample: %d[%d] type:%d\n",
 wrapped_banks ? current_bank : 0,
 wrapped_banks ? sample_bank[current_bank].next_sample : 0,
 FIRST_SAMPLE.type);

 if (user_get_sample_count() == 0)
 return FALSE;
 else
 return FIRST_SAMPLE.data;
 //	return sample_bank[current_bank].samples[sample_bank[current_bank].next_sample
 //			- 1].data;
 }
 */

LOCAL void ICACHE_FLASH_ATTR
user_init_sample_banks() {
	int i;

	sample_bank = (struct sample_bank*) os_malloc(
			sizeof(struct sample_bank) * SAMPLE_BANKS);

	for (i = 0; i < SAMPLE_BANKS; i++) {
		sample_bank[i].capacity = 0;
		sample_bank[i].next_sample = 0;
		sample_bank[i].samples = NULL;
	}

	user_alloc_sample_bank(0);
	sample_bank[0].samples[0].type = NA; // Detect unused empty memory
	current_bank = 0;
	wrapped_banks = FALSE;

	os_printf("Sizes: sample: %d sample_sata: %d\r\n", sizeof(struct sample), sizeof(sample_bank[0].samples[0].data));
}

void ICACHE_FLASH_ATTR
user_sensor_storage_init() {
//	os_malloc(system_get_free_heap_size() - 55);
//	os_printf("%d %s:%d\r\n", system_get_free_heap_size(), __FILE__,
//	__LINE__);
	user_init_sample_banks();
}
