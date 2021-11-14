#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOSConfig.h"
/* BLE */
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "ble_sens.h"

static const char *tag = "NimBLE_BLE_HeartRate";


static bool notify_state;

static uint16_t conn_handle;

static const char *device_name = "ble_sensor_1.0";

static int ble_gap_event(struct ble_gap_event *event, void *arg);

static uint8_t ble_addr_type;

/* Variable to simulate data */
static uint8_t data = 90;

/**
 * Utility function to log an array of bytes.
 */
void print_bytes(const uint8_t *bytes, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        MODLOG_DFLT(INFO, "%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

void print_addr(const void *addr)
{
    const uint8_t *u8p;

    u8p = addr;
    MODLOG_DFLT(INFO, "%02x:%02x:%02x:%02x:%02x:%02x",
                u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}


/*
 * Enables advertising with parameters:
 *     o General discoverable mode
 *     o Undirected connectable mode
 */
static void ble_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    /*
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info)
     *     o Advertising tx power
     *     o Device name
     */
    memset(&fields, 0, sizeof(fields));

    /*
     * Advertise two flags:
     *      o Discoverability in forthcoming advertisement (general)
     *      o BLE-only (BR/EDR unsupported)
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /*
     * Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

static void ble_tx_stop(void)
{
    xTimerStop( dataPollTimer, dataRate / portTICK_PERIOD_MS );
}

static void ble_tx_reset(void)
{
    int rc;

    if (xTimerReset(dataPollTimer, dataRate / portTICK_PERIOD_MS ) == pdPASS) {
        rc = 0;
    } else {
        rc = 1;
    }

    assert(rc == 0);

}

static void pollData(void* params)
{
    static uint8_t dd[8];
    int rc;
    struct os_mbuf *om;

    if (!notify_state) {
        ble_tx_stop();
        data = 0;
        return;
    }

    for (int i = 0; i < 8; i++){
        dd[i]=data+i;
    }

    //increment data and loop around max val so we know what's happening
    data++;
    if (data >= 255) {
        data = 0;
    }

    om = ble_hs_mbuf_from_flat(dd, sizeof(dd));
    rc = ble_gattc_notify_custom(conn_handle, hrs_hrm_handle, om);

    assert(rc == 0);

    ble_tx_reset();
}

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            //ESP_LOGI("GAP: ", "BLE GAP EVENT CONNECT\n");
            /* A new connection was established or a connection attempt failed */
            MODLOG_DFLT(INFO, "connection %s; status=%d\n",
                        event->connect.status == 0 ? "established" : "failed",
                        event->connect.status);

            if (event->connect.status != 0) {
                /* Connection failed; resume advertising */
                ble_advertise();
            }
            conn_handle = event->connect.conn_handle;
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            //ESP_LOGI("GAP :", "BLE GAP EVENT DISCONNECT\n");
            MODLOG_DFLT(INFO, "disconnect; reason=%d\n", event->disconnect.reason);

            /* Connection terminated; resume advertising */
            ble_advertise();
            break;
        case BLE_GAP_EVENT_CONN_UPDATE:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_CONN_UPDATE\n");
            break;
        case BLE_GAP_EVENT_CONN_UPDATE_REQ:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_CONN_UPDATE_REQ\n");
            break;
        case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_L2CAP_UPDATE_REQ\n");
            break;
        case BLE_GAP_EVENT_TERM_FAILURE:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_TERM_FAILURE\n");
            break;
        case BLE_GAP_EVENT_DISC:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_DISC\n");
            break;
        case BLE_GAP_EVENT_DISC_COMPLETE:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_DISC_COMPLETE\n");
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_ADV_COMPLETE\n");
            MODLOG_DFLT(INFO, "adv complete\n");
            ble_advertise();
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_ENC_CHANGE\n");
            break;
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_PASSKEY_ACTION\n");
            break;
        case BLE_GAP_EVENT_NOTIFY_RX:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_NOTIFY_RX\n");
            break;
        case BLE_GAP_EVENT_NOTIFY_TX:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_NOTIFY_TX\n");
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_SUBSCRIBE\n");
            MODLOG_DFLT(INFO, "subscribe event; cur_notify=%d\n value handle; "
                        "val_handle=%d\n",
                        event->subscribe.cur_notify, hrs_hrm_handle);
            if (event->subscribe.attr_handle == hrs_hrm_handle) {
                notify_state = event->subscribe.cur_notify;
                ble_tx_reset();
            } else if (event->subscribe.attr_handle != hrs_hrm_handle) {
                notify_state = event->subscribe.cur_notify;
                ble_tx_stop();
            }
            break;
        case BLE_GAP_EVENT_MTU:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_MTU\n");
            MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d mtu=%d\n",
                        event->mtu.conn_handle,
                        event->mtu.value);
            break;
        case BLE_GAP_EVENT_IDENTITY_RESOLVED:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_IDENTITY_RESOLVED\n");
            break;
        case BLE_GAP_EVENT_REPEAT_PAIRING:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_REPEAT_PAIRING\n");
            break;
        case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
            ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_PHY_UPDATE_COMPLETE\n");
            break;
        case BLE_GAP_EVENT_EXT_DISC:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_EXT_DISC\n");
            break;
        case BLE_GAP_EVENT_PERIODIC_SYNC:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_PERIODIC_SYNC\n");
            break;
        case BLE_GAP_EVENT_PERIODIC_REPORT:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_PERIODIC_REPORT\n");
            break;
        case BLE_GAP_EVENT_PERIODIC_SYNC_LOST:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_PERIODIC_SYNC_LOST\n");
            break;
        case BLE_GAP_EVENT_SCAN_REQ_RCVD:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_SCAN_REQ_RCVD\n");
            break;
        case BLE_GAP_EVENT_PERIODIC_TRANSFER:
            //ESP_LOGI("GAP :", "BLE GAP EVENT BLE_GAP_EVENT_PERIODIC_TRANSFER\n");
            break;
        default: //default hit if nothing else gets hit
            //ESP_LOGI("GAP :", "BLE GAP EVENT DEFAULT CATCH STATE - CODE %d:\n", event->type);
            break;

    }

    return 0;
}

static void ble_on_sync(void)
{
    int rc;

    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    assert(rc == 0);

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(ble_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "Device Address: ");
    print_addr(addr_val);
    MODLOG_DFLT(INFO, "\n");

    /* Begin advertising */
    ble_advertise();
}

static void
ble_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

void ble_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    
    nimble_port_run(); //returns when nimble_port_stop() is executed

    nimble_port_freertos_deinit();
}

void app_main(void)
{
    esp_err_t ret;

    //Initialize NVS flash memory
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); //free up nvs
        ret = nvs_flash_init(); //reinit
    }
    ESP_ERROR_CHECK(ret);
    
    //initialize bluetooth controller.
    //Initialize nimble human computer interaction controller
    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());
    
    //initialize nimble port
    nimble_port_init();
    
    // queues all services.
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    //start dataPollTimer
    /*
    BaseType_t xReturn;
    xReturn = xTaskCreate(
        pollData, //function that implements task
        "xTask", //name
        120, //stacksize
        void * 1, //arg
        3, //Priority
        &xTaskHandle); //task handle
        */
    dataPollTimer = xTimerCreate("dataPollTimer", pdMS_TO_TICKS(dataRate), pdTRUE, (void *)0, pollData);

    ret = gatt_svr_init();
    assert(ret == 0);

    //set BLE device name
    ret = ble_svc_gap_device_name_set(device_name);
    assert(ret == 0);

    //start the task on the esp chip
    nimble_port_freertos_init(ble_host_task);
}