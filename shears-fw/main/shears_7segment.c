#include "shears_7segment.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char* TAG = "shears_7segment";

#define SEVEN_SEG_O0_PIN	((gpio_num_t)32)
#define SEVEN_SEG_O1_PIN	((gpio_num_t)33)
#define SEVEN_SEG_O2_PIN	((gpio_num_t)25)
#define SEVEN_SEG_O3_PIN	((gpio_num_t)26)

/*
 * digit : O[3:0]
 *   0   : 0000
 *   1   : 1000
 *   2   : 0010
 *   3   : 1010
 *   4   : 0100
 *   5   : 1100
 *   6   : 0110
 *   7   : 1110
 *   8   : 0001
 *   9   : 1001
 */
static const uint8_t digitToRawMap[10] = {
	0x0,
	0x8,
	0x2,
	0xA,
	0x4,
	0xC,
	0x6,
	0xE,
	0x1,
	0x9
};

void shears7SegmentInit(void)
{
	gpio_config_t ioConf = {
		.pin_bit_mask =
			(1ULL << SEVEN_SEG_O0_PIN) |
			(1ULL << SEVEN_SEG_O1_PIN) |
			(1ULL << SEVEN_SEG_O2_PIN) |
			(1ULL << SEVEN_SEG_O3_PIN),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};

	ESP_ERROR_CHECK(gpio_config(&ioConf));

	shears7SegmentClear();

	ESP_LOGI(TAG, "7-segment initialized");
}

void shears7SegmentSetRaw(uint8_t value)
{
	uint8_t o0 = (value >> 0) & 0x1;
	uint8_t o1 = (value >> 1) & 0x1;
	uint8_t o2 = (value >> 2) & 0x1;
	uint8_t o3 = (value >> 3) & 0x1;

	gpio_set_level(SEVEN_SEG_O0_PIN, o0);
	gpio_set_level(SEVEN_SEG_O1_PIN, o1);
	gpio_set_level(SEVEN_SEG_O2_PIN, o2);
	gpio_set_level(SEVEN_SEG_O3_PIN, o3);
}

bool shears7SegmentSetDigit(uint8_t digit)
{
	if (digit > 9) {
		ESP_LOGW(TAG, "Invalid digit: %u", digit);
		return false;
	}

	shears7SegmentSetRaw(digitToRawMap[digit]);
	return true;
}

void shears7SegmentClear(void)
{
	shears7SegmentSetDigit(0);
}

void shears7SegmentShowFixType(uint8_t fixType)
{
	if (!shears7SegmentSetDigit(fixType)) {
		shears7SegmentSetDigit(0);
	}
}