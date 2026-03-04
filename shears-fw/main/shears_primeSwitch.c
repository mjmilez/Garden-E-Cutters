/* shears_primeSwitch.c
 *
 * Prime switch state tracking. ISR-safe (no malloc, no logging).
 */

#include "shears_primeSwitch.h"

#include "esp_attr.h"
#include <stdint.h>

#define PRIME_ACTIVE_LEVEL 0

static volatile bool primed = false;
static volatile bool primedEdgeFlag = false;
static volatile bool unprimedEdgeFlag = false;

void shearsPrimeSwitchInit(int initialLevel)
{
	bool newPrimed = (initialLevel == PRIME_ACTIVE_LEVEL);
	primed = newPrimed;
	primedEdgeFlag = false;
	unprimedEdgeFlag = false;
}

void IRAM_ATTR shearsPrimeSwitchUpdateFromLevel(int level)
{
	bool newPrimed = (level == PRIME_ACTIVE_LEVEL);
	if (newPrimed == primed) {
		return;
	}

	primed = newPrimed;

	if (primed) {
		primedEdgeFlag = true;
	} else {
		unprimedEdgeFlag = true;
	}
}

bool shearsPrimeSwitchIsPrimed(void)
{
	return primed;
}

bool shearsPrimeSwitchConsumePrimedEdge(void)
{
	if (!primedEdgeFlag) {
		return false;
	}
	primedEdgeFlag = false;
	return true;
}

bool shearsPrimeSwitchConsumeUnprimedEdge(void)
{
	if (!unprimedEdgeFlag) {
		return false;
	}
	unprimedEdgeFlag = false;
	return true;
}