#define _GNU_SOURCE
#include "base_led.h"

#include <gpiod.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* -------------------------------------------------------------------------- */
/* Internal State                                                             */
/* -------------------------------------------------------------------------- */

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static struct gpiod_chip* chip = NULL;
static struct gpiod_line_request* request = NULL;

static unsigned int currentOffset = 0;
static int currentPin = -1;

static pthread_t ledThread;
static atomic_int threadRunning = 0;

typedef enum {
	modeOff = 0,
	modeOn,
	modeBlink
} LedMode;

static atomic_int g_mode = modeOff;
static atomic_uint g_blinkOnMs = 100;
static atomic_uint g_blinkOffMs = 100;

/* -------------------------------------------------------------------------- */
/* Utilities                                                                  */
/* -------------------------------------------------------------------------- */

static void sleepMs(unsigned ms)
{
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}

static void setLevelLocked(int level)
{
	if (!request)
		return;

	enum gpiod_line_value value =
	    level ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;

	if (gpiod_line_request_set_value(request, currentOffset, value) < 0) {
		perror("gpiod_line_request_set_value");
	}
}

static void releaseRequestLocked(void)
{
	if (request) {
		gpiod_line_request_release(request);
		request = NULL;
	}
	currentPin = -1;
}

/* -------------------------------------------------------------------------- */
/* Lazy Initialization                                                        */
/* -------------------------------------------------------------------------- */

static bool ensureLedReadyLocked(int gpioPinBcm)
{
	if (request && currentPin == gpioPinBcm)
		return true;

	releaseRequestLocked();

	if (!chip) {
		chip = gpiod_chip_open("/dev/gpiochip0");
		if (!chip) {
			perror("gpiod_chip_open");
			return false;
		}
	}

	currentOffset = (unsigned)gpioPinBcm;

	struct gpiod_line_settings* settings = gpiod_line_settings_new();
	struct gpiod_line_config* lineCfg = gpiod_line_config_new();
	struct gpiod_request_config* reqCfg = gpiod_request_config_new();

	if (!settings || !lineCfg || !reqCfg) {
		fprintf(stderr, "gpiod: failed to allocate config objects\n");
		if (settings) gpiod_line_settings_free(settings);
		if (lineCfg) gpiod_line_config_free(lineCfg);
		if (reqCfg) gpiod_request_config_free(reqCfg);
		return false;
	}

	gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
	gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

	if (gpiod_line_config_add_line_settings(lineCfg, &currentOffset, 1, settings) < 0) {
		perror("gpiod_line_config_add_line_settings");
		gpiod_line_settings_free(settings);
		gpiod_line_config_free(lineCfg);
		gpiod_request_config_free(reqCfg);
		return false;
	}

	gpiod_request_config_set_consumer(reqCfg, "rpi-base-fw");

	request = gpiod_chip_request_lines(chip, reqCfg, lineCfg);
	if (!request) {
		perror("gpiod_chip_request_lines");
		gpiod_line_settings_free(settings);
		gpiod_line_config_free(lineCfg);
		gpiod_request_config_free(reqCfg);
		return false;
	}

	gpiod_line_settings_free(settings);
	gpiod_line_config_free(lineCfg);
	gpiod_request_config_free(reqCfg);

	currentPin = gpioPinBcm;
	return true;
}

/* -------------------------------------------------------------------------- */
/* Blink Thread                                                               */
/* -------------------------------------------------------------------------- */

static void* blinkTask(void* arg)
{
	(void)arg;

	while (atomic_load(&threadRunning)) {

		if (atomic_load(&g_mode) != modeBlink) {
			sleepMs(50);
			continue;
		}

		unsigned onMs  = atomic_load(&g_blinkOnMs);
		unsigned offMs = atomic_load(&g_blinkOffMs);

		pthread_mutex_lock(&g_lock);
		setLevelLocked(1);
		pthread_mutex_unlock(&g_lock);

		sleepMs(onMs);

		if (atomic_load(&g_mode) != modeBlink)
			continue;

		pthread_mutex_lock(&g_lock);
		setLevelLocked(0);
		pthread_mutex_unlock(&g_lock);

		sleepMs(offMs);
	}

	pthread_mutex_lock(&g_lock);
	setLevelLocked(0);
	pthread_mutex_unlock(&g_lock);

	return NULL;
}

static void ensureBlinkThreadStarted(void)
{
	if (atomic_load(&threadRunning))
		return;

	atomic_store(&threadRunning, 1);

	int rc = pthread_create(&ledThread, NULL, blinkTask, NULL);
	if (rc != 0) {
		fprintf(stderr, "pthread_create(blinkTask) failed: %s\n", strerror(rc));
		atomic_store(&threadRunning, 0);
	}
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void gpioLedSetBlinkMs(uint32_t onMs, uint32_t offMs)
{
	if (onMs == 0)  onMs  = 1;
	if (offMs == 0) offMs = 1;

	atomic_store(&g_blinkOnMs, onMs);
	atomic_store(&g_blinkOffMs, offMs);
}

void gpioLedOn(int gpioPinBcm)
{
	pthread_mutex_lock(&g_lock);

	if (!ensureLedReadyLocked(gpioPinBcm)) {
		pthread_mutex_unlock(&g_lock);
		return;
	}

	atomic_store(&g_mode, modeOn);
	setLevelLocked(1);

	pthread_mutex_unlock(&g_lock);
}

void gpioLedOff(int gpioPinBcm)
{
	pthread_mutex_lock(&g_lock);

	if (!ensureLedReadyLocked(gpioPinBcm)) {
		pthread_mutex_unlock(&g_lock);
		return;
	}

	atomic_store(&g_mode, modeOff);
	setLevelLocked(0);

	pthread_mutex_unlock(&g_lock);
}

void gpioLedBlink(int gpioPinBcm)
{
	pthread_mutex_lock(&g_lock);

	if (!ensureLedReadyLocked(gpioPinBcm)) {
		pthread_mutex_unlock(&g_lock);
		return;
	}

	atomic_store(&g_mode, modeBlink);

	pthread_mutex_unlock(&g_lock);

	ensureBlinkThreadStarted();
}

void gpioLedShutdown(void)
{
	if (atomic_exchange(&threadRunning, 0)) {
		pthread_join(ledThread, NULL);
	}

	pthread_mutex_lock(&g_lock);

	setLevelLocked(0);
	releaseRequestLocked();

	if (chip) {
		gpiod_chip_close(chip);
		chip = NULL;
	}

	pthread_mutex_unlock(&g_lock);
}