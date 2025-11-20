#include "shears_led.h"
#include "shears_ble.h"
#include "gps_logger.h"

static void bleConnChanged(bool connected) {
    if (connected) {
        shearsLedSetSolidOn();
    } else {
        shearsLedSetOff();
        shearsLedSetBlinking(true);
    }
}

void app_main(void) {
    /* LED status system */
    shearsLedInit();
    shearsLedSetBlinking(true);

    /* GPS logger (SPIFFS + UART + button + tasks) */
    gpsLoggerInit();

    /* BLE peripheral behavior (advertise as WM-SHEARS) */
    shearsBleInit(bleConnChanged);

    /* Nothing else in foreground; all work is in tasks. */
}
