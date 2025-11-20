/**
 * @file base_ble.c
 * @brief Implements BLE central behavior for the base station using NimBLE.
 *
 * This module:
 *   - Scans for BLE advertisements
 *   - Filters devices by name ("WM-SHEARS")
 *   - Initiates connections automatically
 *   - Handles reconnects after disconnect
 *   - Notifies application layer via callback when link goes up/down
 *
 * No UI/LED logic is in this file; that's delegated to base_led.c
 */

#include "base_ble.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* Module tag */
static const char *TAG = "base_ble";

/* Our BLE controller address type */
static uint8_t ownAddrType;

/* BLE name of the shears unit we want to connect to */
static const char *targetName = "WM-SHEARS";

/* App-supplied callback for connection state changes */
static bleBaseConnCallback_t connCallback = NULL;

/* --- Forward declarations --- */
static void startScan(void);
static int gapEventHandler(struct ble_gap_event *event, void *arg);
static void onSync(void);
static void hostTask(void *param);


/**
 * @brief Extracts the advertised device name from BLE advertisement fields.
 */
static void getAdvName(const struct ble_hs_adv_fields *fields,
                       char *out,
                       size_t outLen)
{
    if (outLen == 0) return;

    if (fields->name_len > 0 && fields->name != NULL) {
        size_t len = fields->name_len;
        if (len >= outLen) len = outLen - 1;
        memcpy(out, fields->name, len);
        out[len] = '\0';
    } else {
        out[0] = '\0';
    }
}


/**
 * @brief NimBLE GAP event handler.
 *
 * Handles:
 *   - Discovery events
 *   - Connect/disconnect
 *   - Scan completion
 */
static int gapEventHandler(struct ble_gap_event *event, void *arg) {

    switch (event->type) {

    /* Found BLE advertisement */
    case BLE_GAP_EVENT_DISC: {
        struct ble_hs_adv_fields fields;

        int rc = ble_hs_adv_parse_fields(&fields,
                                         event->disc.data,
                                         event->disc.length_data);
        if (rc != 0) return 0;

        char name[32];
        getAdvName(&fields, name, sizeof(name));

        /* Only care about the shears */
        if (strcmp(name, targetName) != 0) {
            return 0;
        }

        ESP_LOGI(TAG,
                 "Saw WM-SHEARS: %02X:%02X:%02X:%02X:%02X:%02X",
                 event->disc.addr.val[5],
                 event->disc.addr.val[4],
                 event->disc.addr.val[3],
                 event->disc.addr.val[2],
                 event->disc.addr.val[1],
                 event->disc.addr.val[0]);

        /* Stop scanning and initiate connection */
        ble_gap_disc_cancel();

        struct ble_gap_conn_params connParams = {0};
        connParams.scan_itvl = 0x0010;
        connParams.scan_window = 0x0010;
        connParams.itvl_min = 0x0010;
        connParams.itvl_max = 0x0020;
        connParams.latency = 0;
        connParams.supervision_timeout = 0x0258;

        rc = ble_gap_connect(ownAddrType,
                             &event->disc.addr,
                             300,
                             &connParams,
                             gapEventHandler,
                             NULL);

        if (rc != 0) {
            ESP_LOGE(TAG, "ble_gap_connect() failed rc=%d", rc);
            startScan();
        }

        break;
    }

    /* Connection established or failed */
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connected to WM-SHEARS");
            if (connCallback) connCallback(true);
        } else {
            ESP_LOGW(TAG, "Connection failed, restarting scan");
            if (connCallback) connCallback(false);
            startScan();
        }
        break;

    /* Link lost */
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected, restarting scan");
        if (connCallback) connCallback(false);
        startScan();
        break;

    /* Passive scan finished — restart continuous scanning */
    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "Scan complete → restart scanning");
        startScan();
        break;

    default:
        break;
    }

    return 0;
}


/**
 * @brief Begin BLE scan for the target name.
 */
static void startScan(void) {
    struct ble_gap_disc_params params = {0};

    params.passive = 0;  /* active scan for scan responses */
    params.itvl = 0x0010;
    params.window = 0x0010;
    params.filter_duplicates = 0;

    int rc = ble_gap_disc(ownAddrType,
                          BLE_HS_FOREVER,
                          &params,
                          gapEventHandler,
                          NULL);

    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start scan rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Scanning for \"%s\"...", targetName);
    }
}


/**
 * @brief Called when BLE host stack is fully initialized.
 */
static void onSync(void) {
    int rc = ble_hs_id_infer_auto(0, &ownAddrType);
    if (rc != 0) {
        ESP_LOGE(TAG, "Address type error rc=%d", rc);
        return;
    }

    /* Advertise as WM-BASE */
    ble_svc_gap_device_name_set("WM-BASE");

    /* Start scanning immediately */
    startScan();
}


/**
 * @brief NimBLE host FreeRTOS task.
 */
static void hostTask(void *param) {
    nimble_port_run();        // Runs until shutdown
    nimble_port_freertos_deinit();
}


/**
 * @brief Initialize BLE host+controller, GAP, GATT, and begin scanning.
 */
void bleBaseInit(bleBaseConnCallback_t cb) {
    connCallback = cb;

    /* Initialize NVS for BLE controller */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Init NimBLE */
    nimble_port_init();
    ble_hs_cfg.sync_cb = onSync;

    /* Initialize GAP & GATT services */
    ble_svc_gap_init();
    ble_svc_gatt_init();

    /* Start NimBLE host thread */
    nimble_port_freertos_init(hostTask);

    ESP_LOGI(TAG, "BLE init complete");
}
