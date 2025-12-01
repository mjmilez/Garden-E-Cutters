/**
 * @file base_ble.c
 * @brief BLE central logic for the base station that talks to the shears'
 *        log-transfer service.
 *
 * High-level flow:
 *  - bring up NimBLE and set our GAP name
 *  - scan for advertisements from "WM-SHEARS"
 *  - connect when we see it
 *  - discover the custom log service + control/data characteristics
 *  - enable notifications and pass the handles into log_transfer_client
 *  - bleBaseRequestLog() lets the rest of the app ask for a log file by name
 */

#include "base_ble.h"

#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "host/util/util.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "log_transfer_client.h"
#include "log_paths.h"   // for GPS_LOG_FILE_BASENAME etc.

static const char *TAG = "base_ble";

/* Address type chosen by the NimBLE host after sync */
static uint8_t ownAddrType;

/* BLE name of the shears unit we want to connect to */
static const char *targetName = "WM-SHEARS";

/* Optional app callback to report connection state */
static bleBaseConnCallback_t connCallback = NULL;

/* Connection + log-service state discovered after we connect */
static uint16_t s_connHandle        = 0;
static uint16_t s_logSvcStart       = 0;
static uint16_t s_logSvcEnd         = 0;
static uint16_t s_logCtrlChrHandle  = 0;
static uint16_t s_logDataChrHandle  = 0;

/* If the app asks for a log before discovery finishes, we stash it here */
static bool  s_pendingRequest       = false;
static char  s_pendingFilename[64]  = {0};

/* --- Forward declarations --- */
static void startScan(void);
static int  gapEventHandler(struct ble_gap_event *event, void *arg);
static void onSync(void);
static void hostTask(void *param);
static int  gattDiscSvcCb(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *service,
                           void *arg);
static int  gattDiscChrCb(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr,
                           void *arg);

/* -------------------------------------------------------------------------- */
/* Helper: pull the device name out of advertising data                       */
/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */
/* GATT discovery: find the log service + its two characteristics             */
/* -------------------------------------------------------------------------- */

#define LOG_SVC_UUID       0xFFF0
#define LOG_CTRL_CHR_UUID  0xFFF1
#define LOG_DATA_CHR_UUID  0xFFF2

static int gattDiscSvcCb(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         const struct ble_gatt_svc *service,
                         void *arg)
{
	(void)arg;

	if (error->status == 0) {
		/* While discovery is running, this gets called per service.
		 * We only care about the single 0xFFF0 service that carries log transfer.
		 */
		uint16_t uuid16 = ble_uuid_u16(&service->uuid.u);
		if (uuid16 == LOG_SVC_UUID) {
			s_logSvcStart = service->start_handle;
			s_logSvcEnd   = service->end_handle;
			ESP_LOGI(TAG, "Found log svc 0x%04x: start=0x%04x end=0x%04x",
			         uuid16, s_logSvcStart, s_logSvcEnd);
		}
		return 0;
	}

	/* Once NimBLE is done walking services, it reports BLE_HS_EDONE */
	if (error->status == BLE_HS_EDONE) {
		if (s_logSvcStart != 0 && s_logSvcEnd != 0) {
			/* Now walk characteristics inside the log service. */
			int rc = ble_gattc_disc_all_chrs(conn_handle,
			                                 s_logSvcStart,
			                                 s_logSvcEnd,
			                                 gattDiscChrCb,
			                                 NULL);
			if (rc != 0) {
				ESP_LOGE(TAG, "disc_all_chrs failed rc=%d", rc);
			}
		} else {
			ESP_LOGW(TAG, "Log service not found");
		}
	}

	return 0;
}

static int gattDiscChrCb(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         const struct ble_gatt_chr *chr,
                         void *arg)
{
	(void)arg;

	if (error->status == 0) {
		/* Same pattern as services: this is called once per characteristic. */
		uint16_t uuid16 = ble_uuid_u16(&chr->uuid.u);

		if (uuid16 == LOG_CTRL_CHR_UUID) {
			s_logCtrlChrHandle = chr->val_handle;
			ESP_LOGI(TAG, "Found log CTRL chr 0x%04x val_handle=0x%04x",
			         uuid16, s_logCtrlChrHandle);
		} else if (uuid16 == LOG_DATA_CHR_UUID) {
			s_logDataChrHandle = chr->val_handle;
			ESP_LOGI(TAG, "Found log DATA chr 0x%04x val_handle=0x%04x",
			         uuid16, s_logDataChrHandle);
		}
		return 0;
	}

	if (error->status == BLE_HS_EDONE) {
		/* At this point characteristic discovery for the service is finished.
		 * We don't start the log client until we've found both handles.
		 */
		if (s_logCtrlChrHandle != 0 && s_logDataChrHandle != 0) {
			/* Turn on notifications by writing the CCCD (0x2902).
			 * In this simple layout the CCCD is val_handle + 1.
			 */
			uint8_t cccd_val[2] = {0x01, 0x00};  // notifications enabled

			int rc = ble_gattc_write_flat(conn_handle,
			                              s_logCtrlChrHandle + 1,
			                              cccd_val,
			                              sizeof(cccd_val),
			                              NULL,
			                              NULL);
			if (rc != 0) {
				ESP_LOGE(TAG, "Failed to enable NOTIFY on CTRL chr rc=%d", rc);
			}

			rc = ble_gattc_write_flat(conn_handle,
			                          s_logDataChrHandle + 1,
			                          cccd_val,
			                          sizeof(cccd_val),
			                          NULL,
			                          NULL);
			if (rc != 0) {
				ESP_LOGE(TAG, "Failed to enable NOTIFY on DATA chr rc=%d", rc);
			}

			/* Now that we know the handles and have notifications turned on,
			 * wire the connection into the log transfer client.
			 */
			log_transfer_client_cfg_t cfg = {
				.connHandle    = conn_handle,
				.ctrlChrHandle = s_logCtrlChrHandle,
				.dataChrHandle = s_logDataChrHandle,
			};
			log_transfer_client_init(&cfg);
			ESP_LOGI(TAG, "Log transfer client initialized");

			/* If the app already asked for a file, kick that request now. */
			if (s_pendingRequest) {
				ESP_LOGI(TAG, "Issuing queued log request for '%s'", s_pendingFilename);
				log_transfer_client_request_file(s_pendingFilename);
				s_pendingRequest = false;
			}
		} else {
			ESP_LOGW(TAG, "Log transfer chars not fully discovered (ctrl=0x%04x data=0x%04x)",
			         s_logCtrlChrHandle, s_logDataChrHandle);
		}
	}

	return 0;
}

/* -------------------------------------------------------------------------- */
/* GAP event handler: scan, connect, discover, and route notifications        */
/* -------------------------------------------------------------------------- */

static void startScan(void);

static int gapEventHandler(struct ble_gap_event *event, void *arg)
{
	(void)arg;

	switch (event->type) {

	case BLE_GAP_EVENT_DISC: {
		/* Advertising report while we are scanning: filter by name. */
		struct ble_hs_adv_fields fields;
		int rc = ble_hs_adv_parse_fields(&fields,
		                                 event->disc.data,
		                                 event->disc.length_data);
		if (rc != 0) {
			return 0;
		}

		char name[32];
		getAdvName(&fields, name, sizeof(name));

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

		/* Stop scanning and initiate connection to this peer. */
		ble_gap_disc_cancel();

		struct ble_gap_conn_params connParams = {0};
		connParams.scan_itvl           = 0x0010;
		connParams.scan_window         = 0x0010;
		connParams.itvl_min            = 0x0010;
		connParams.itvl_max            = 0x0020;
		connParams.latency             = 0;
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

	case BLE_GAP_EVENT_CONNECT:
		if (event->connect.status == 0) {
			ESP_LOGI(TAG, "Connected to WM-SHEARS");
			s_connHandle = event->connect.conn_handle;

			if (connCallback) {
				connCallback(true);
			}

			/* Reset discovery state and kick off service discovery. */
			s_logSvcStart      = 0;
			s_logSvcEnd        = 0;
			s_logCtrlChrHandle = 0;
			s_logDataChrHandle = 0;

			ESP_LOGI(TAG, "Starting service discovery on shears");
			ble_gattc_disc_all_svcs(s_connHandle, gattDiscSvcCb, NULL);
		} else {
			ESP_LOGW(TAG, "Connection failed, restarting scan");
			s_connHandle = 0;
			if (connCallback) {
				connCallback(false);
			}
			startScan();
		}
		break;

	case BLE_GAP_EVENT_DISCONNECT:
		ESP_LOGI(TAG, "Disconnected, restarting scan");
		s_connHandle        = 0;
		s_logSvcStart       = 0;
		s_logSvcEnd         = 0;
		s_logCtrlChrHandle  = 0;
		s_logDataChrHandle  = 0;
		if (connCallback) {
			connCallback(false);
		}
		startScan();
		break;

	case BLE_GAP_EVENT_DISC_COMPLETE:
		/* We ran a finite scan and didn't connect to anything. Loop again. */
		ESP_LOGI(TAG, "Scan complete → restart scanning");
		startScan();
		break;

	case BLE_GAP_EVENT_NOTIFY_RX: {
		/* Notification from the shears: hand it to the log-transfer client. */
		uint16_t attr_handle = event->notify_rx.attr_handle;
		struct os_mbuf *om   = event->notify_rx.om;
		uint16_t len         = OS_MBUF_PKTLEN(om);

		uint8_t buf[200];
		if (len > sizeof(buf)) {
			len = sizeof(buf);
		}
		os_mbuf_copydata(om, 0, len, buf);

		if (attr_handle == s_logCtrlChrHandle) {
			log_transfer_client_on_ctrl_notify(buf, len);
		} else if (attr_handle == s_logDataChrHandle) {
			log_transfer_client_on_data_notify(buf, len);
		}
		break;
	}

	default:
		break;
	}

	return 0;
}

/* -------------------------------------------------------------------------- */
/* Scanning + NimBLE stack bring-up                                          */
/* -------------------------------------------------------------------------- */

static void startScan(void)
{
	/* Central-side scan: look for the shears and report results to gapEventHandler. */
	struct ble_gap_disc_params params = {0};

	params.passive           = 0;
	params.itvl              = 0x0010;
	params.window            = 0x0010;
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

static void onSync(void)
{
	/* Called once the NimBLE host and controller are ready. */
	int rc = ble_hs_id_infer_auto(0, &ownAddrType);
	if (rc != 0) {
		ESP_LOGE(TAG, "Address type error rc=%d", rc);
		return;
	}

	/* Give the base a readable GAP name in case we ever need to debug it. */
	ble_svc_gap_device_name_set("WM-BASE");

	/* Start the scan loop; everything else is driven from gapEventHandler. */
	startScan();
}

static void hostTask(void *param)
{
	(void)param;
	nimble_port_run();
	nimble_port_freertos_deinit();
}

/* -------------------------------------------------------------------------- */
/* Public API: base-side BLE lifecycle                                       */
/* -------------------------------------------------------------------------- */

void bleBaseInit(bleBaseConnCallback_t cb)
{
	/* App calls this once from app_main() to bring up BLE on the base. */
	connCallback = cb;

	/* NVS is required by the controller; handle the "no free pages" case. */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	/* Wire up NimBLE host config and sync callback. */
	nimble_port_init();
	ble_hs_cfg.sync_cb = onSync;

	/* Register the standard GAP/GATT services (device name, etc.). */
	ble_svc_gap_init();
	ble_svc_gatt_init();

	/* Spin up the NimBLE host on its own FreeRTOS task. */
	nimble_port_freertos_init(hostTask);

	ESP_LOGI(TAG, "BLE init complete");
}

/**
 * @brief Ask the shears to send a log file by name.
 *
 * If the log service / characteristics are already known, this calls straight
 * into log_transfer_client. If discovery is still in progress, the request is
 * queued and sent once discovery finishes and notifications are enabled.
 */
esp_err_t bleBaseRequestLog(const char *filename)
{
	if (!filename || filename[0] == '\0') {
		return ESP_ERR_INVALID_ARG;
	}

	/* If we already have handles and the client is wired up, go direct. */
	if (s_logCtrlChrHandle != 0 && s_logDataChrHandle != 0) {
		return log_transfer_client_request_file(filename);
	}

	/* Otherwise, remember this request so we can kick it after discovery. */
	strncpy(s_pendingFilename, filename, sizeof(s_pendingFilename));
	s_pendingFilename[sizeof(s_pendingFilename) - 1] = '\0';
	s_pendingRequest = true;

	ESP_LOGI(TAG, "GATT not ready yet, queued log request for '%s'", s_pendingFilename);
	return ESP_OK;
}
