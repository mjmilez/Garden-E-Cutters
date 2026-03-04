/*
 * shears_piezo.c
 *
 * Piezo buzzer driver (LEDC) for the shears firmware.
 */

#include "shears_piezo.h"

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIEZO_PIN_A       GPIO_NUM_21
#define PIEZO_PIN_B       GPIO_NUM_5

#define BEEP_ON_MS        80
#define BEEP_OFF_MS       80

#define PIEZO_LEDC_TIMER      LEDC_TIMER_0
#define PIEZO_LEDC_CHANNEL_A  LEDC_CHANNEL_0
#define PIEZO_LEDC_CHANNEL_B  LEDC_CHANNEL_1
#define PIEZO_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define PIEZO_LEDC_RES        LEDC_TIMER_8_BIT
#define PIEZO_LEDC_DUTY       128
#define PIEZO_LEDC_FREQ_HZ    4000

void shearsPiezoInit(void)
{
	ledc_timer_config_t timerConf = {
		.speed_mode       = PIEZO_LEDC_MODE,
		.timer_num        = PIEZO_LEDC_TIMER,
		.duty_resolution  = PIEZO_LEDC_RES,
		.freq_hz          = PIEZO_LEDC_FREQ_HZ,
		.clk_cfg          = LEDC_AUTO_CLK
	};
	ledc_timer_config(&timerConf);

	ledc_channel_config_t channelConfA = {
		.gpio_num   = PIEZO_PIN_A,
		.speed_mode = PIEZO_LEDC_MODE,
		.channel    = PIEZO_LEDC_CHANNEL_A,
		.intr_type  = LEDC_INTR_DISABLE,
		.timer_sel  = PIEZO_LEDC_TIMER,
		.duty       = 0,
		.hpoint     = 0
	};
	ledc_channel_config(&channelConfA);

	ledc_channel_config_t channelConfB = {
		.gpio_num   = PIEZO_PIN_B,
		.speed_mode = PIEZO_LEDC_MODE,
		.channel    = PIEZO_LEDC_CHANNEL_B,
		.intr_type  = LEDC_INTR_DISABLE,
		.timer_sel  = PIEZO_LEDC_TIMER,
		.duty       = 0,
		.hpoint     = 0
	};
	channelConfB.flags.output_invert = 1;
	ledc_channel_config(&channelConfB);
}

void shearsPiezoSet(bool enable)
{
	ledc_set_duty(PIEZO_LEDC_MODE,
	              PIEZO_LEDC_CHANNEL_A,
	              enable ? PIEZO_LEDC_DUTY : 0);
	ledc_update_duty(PIEZO_LEDC_MODE, PIEZO_LEDC_CHANNEL_A);

	ledc_set_duty(PIEZO_LEDC_MODE,
	              PIEZO_LEDC_CHANNEL_B,
	              enable ? PIEZO_LEDC_DUTY : 0);
	ledc_update_duty(PIEZO_LEDC_MODE, PIEZO_LEDC_CHANNEL_B);
}

void shearsPiezoBeepPattern(int count)
{
	for (int i = 0; i < count; i++) {
		shearsPiezoSet(true);
		vTaskDelay(pdMS_TO_TICKS(BEEP_ON_MS));
		shearsPiezoSet(false);
		vTaskDelay(pdMS_TO_TICKS(BEEP_OFF_MS));
	}
}

void shearsPiezoToneMs(uint32_t durationMs)
{
	shearsPiezoSet(true);
	vTaskDelay(pdMS_TO_TICKS(durationMs));
	shearsPiezoSet(false);
}