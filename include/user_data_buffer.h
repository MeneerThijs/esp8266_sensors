#ifndef __USER_DATA_BUFFER_H__
#define __USER_DATA_BUFFER_H__

// 10 banks of 300 samples seems to give the most efficient memory use.
#define SAMPLE_BANKS 10
#define SAMPLE_BANK_SIZE 300


// for testing: 3 banks of 5 samples
//#define SAMPLE_BANKS 3
//#define SAMPLE_BANK_SIZE 5

typedef enum {
	NA = 0, KWH_SAMPLE = 1, TEMPERATURE_SAMPLE = 2, MISC_SAMPLE = 99
} SAMPLE_TYPE;

struct sample {
	uint32 rtc;
	uint8 type;
	sint16 data;
};

struct sample_bank {
	uint16 capacity;
	uint16 next_sample;
	struct sample *samples;
};

#define SAMPLE_SIZE sizeof(struct sample)

#define FIRST_SAMPLE (wrapped_banks ? \
		sample_bank[current_bank].samples[sample_bank[current_bank].next_sample] : \
		sample_bank[0].samples[0])

#define BANK_NUMBER(sample) (wrapped_banks ? (int) (current_bank + \
		(sample + sample_bank[current_bank].next_sample) / SAMPLE_BANK_SIZE) % \
		(SAMPLE_BANKS) : (int) (sample / SAMPLE_BANK_SIZE))

#define SAMPLE_NUMBER(sample) (wrapped_banks ? (int) (sample + \
		sample_bank[current_bank].next_sample)	% SAMPLE_BANK_SIZE : \
		sample % SAMPLE_BANK_SIZE)

//#define REQUIRED_SIZE(bank_size) (((bank_size * SAMPLE_SIZE - 1) / 8 + 1) * 8 + 16) // =INT(((<struct size>-1)/8+1)*8+16
//#define MAX_SAMPLES(free) ((((free - 16) / 8) * 8) / SAMPLE_SIZE)

bool user_put_sample(uint32 rtc, SAMPLE_TYPE type, sint16 data);

uint16 user_get_sample_count();

uint16 user_get_sample_data(uint16 start_sample, uint16 count, uint32* rtc, uint8* type, sint16* data);

uint16 user_get_samples(uint16 start_sample, uint16 count, struct sample** sample_data);
//int32 user_get_sample();

void user_sensor_storage_init();

#endif
