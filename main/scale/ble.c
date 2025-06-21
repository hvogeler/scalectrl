#include "esp_log.h"
#include "nvs_flash.h"
// #include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "modlog/modlog.h"
#include "esp_central.h"
#include "ble.h"
// #include "console/console.h"
// #include "modlog/modlog.h"

char *TAG = "ble";

/*** The UUID of the service containing characteristics: 0000fff0-0000-1000-8000-00805f9b34fb ***/
static const ble_uuid_t *scale_svc_uuid =
    BLE_UUID128_DECLARE(0x00, 0x00, 0xff, 0xf0,
                        0x00, 0x00,
                        0x10, 0x00,
                        0x80, 0x00,
                        0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);

/*** The UUID of the set chatacteristic: 000036f5-0000-1000-8000-00805f9b34fb ***/
static const ble_uuid_t *set_charac_uuid =
    BLE_UUID128_DECLARE(0x00, 0x00, 0x36, 0xf5,
                        0x00, 0x00,
                        0x10, 0x00,
                        0x80, 0x00,
                        0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);

/*** The UUID of the set chatacteristic: 000036f5-0000-1000-8000-00805f9b34fb ***/
static const ble_uuid_t *notify_charac_uuid =
    BLE_UUID128_DECLARE(0x00, 0x00, 0xff, 0xf4,
                        0x00, 0x00,
                        0x10, 0x00,
                        0x80, 0x00,
                        0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb);

static int blecent_gap_event(struct ble_gap_event *event, void *arg);
static void blecent_scan(void);
static void blecent_on_reset(int reason);
static void blecent_on_sync(void);
void blecent_host_task(void *param);

void bt_connect_scale()
{
    ESP_LOGI(TAG, "A");
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "B");

    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init nimble %d ", ret);
        return;
    }
    ESP_LOGI(TAG, "C");

    /* Configure the host. */
    ble_hs_cfg.reset_cb = blecent_on_reset;
    ble_hs_cfg.sync_cb = blecent_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ESP_LOGI(TAG, "D");

    int m;
    /* Set the default device name. */
    m = ble_svc_gap_device_name_set("Decent Scale");
    assert(m == 0);
    ESP_LOGI(TAG, "E");

    nimble_port_freertos_init(blecent_host_task);
    ESP_LOGI(TAG, "F");
}

void blecent_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

/**
 * Initiates the GAP general discovery procedure.
 */
static void blecent_scan(void)
{
    ESP_LOGI(TAG, "in blecent_scan()");

    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params = {0};
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 0;

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    disc_params.passive = 0;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                      blecent_gap_event, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d\n",
                 rc);
    }
}

/**
 * Called when service discovery of the specified peer has completed.
 */
static void
blecent_on_disc_complete(const struct peer *peer, int status, void *arg)
{

    if (status != 0)
    {
        /* Service discovery failed.  Terminate the connection. */
        ESP_LOGE(TAG, "Error: Service discovery failed; status=%d "
                      "conn_handle=%d\n",
                 status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    /* Service discovery has completed successfully.  Now we have a complete
     * list of services, characteristics, and descriptors that the peer
     * supports.
     */
    ESP_LOGI(TAG, "Service discovery complete; status=%d "
                  "conn_handle=%d\n",
             status, peer->conn_handle);

    /* Now perform three GATT procedures against the peer: read,
     * write, and subscribe to notifications for the ANS service.
     */
    // blecent_read_write_subscribe(peer);
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  blecent uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  blecent.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
blecent_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:
        ESP_LOGI(TAG, "ble discovery event type: %d", event->type);
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                     event->disc.length_data);
        if (rc != 0)
        {
            return 0;
        }

        ESP_LOGI(TAG, "=== Device Found ===");
        ESP_LOGI(TAG, "Device: %02x:%02x:%02x:%02x:%02x:%02x", 
         event->disc.addr.val[5], event->disc.addr.val[4], event->disc.addr.val[3],
         event->disc.addr.val[2], event->disc.addr.val[1], event->disc.addr.val[0]);
        ESP_LOGI(TAG, "RSSI: %d", event->disc.rssi);
        ESP_LOGI(TAG, "Ad data length: %d", event->disc.length_data);

        if (rc == 0)
        {
            if (fields.name)
            {
                ESP_LOGI(TAG, "Name (adv): %.*s", fields.name_len, fields.name);
            }
        }

        // Log raw advertisement data
        esp_log_buffer_hex(TAG, event->disc.data, event->disc.length_data);

        ESP_LOGI(TAG, "==================");

        /* An advertisement report was received during GAP discovery. */
        print_adv_fields(&fields);

        /* Try to connect to the advertiser if it looks interesting. */
        // blecent_connect_if_interesting(&event->disc);
        return 0;
        // case BLE_GAP_EVENT_CONNECT:
        //     /* A new connection was established or a connection attempt failed. */
        //     if (event->connect.status == 0) {
        //         /* Connection successfully established. */
        //         ESP_LOGI(INFO, "Connection established ");

        //         rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        //         assert(rc == 0);
        //         print_conn_desc(&desc);
        //         ESP_LOGI(INFO, "\n");

        //         /* Remember peer. */
        //         rc = peer_add(event->connect.conn_handle);
        //         if (rc != 0) {
        //             ESP_LOGE(ERROR, "Failed to add peer; rc=%d\n", rc);
        //             return 0;
        //         }

        //         /* Perform service discovery */
        //         rc = peer_disc_all(event->connect.conn_handle,
        //                     blecent_on_disc_complete, NULL);
        //         if(rc != 0) {
        //             ESP_LOGE(ERROR, "Failed to discover services; rc=%d\n", rc);
        //             return 0;
        //         }
        //     } else {
        //         /* Connection attempt failed; resume scanning. */
        //         ESP_LOGE(ERROR, "Error: Connection failed; status=%d\n",
        //                     event->connect.status);
        //         blecent_scan();
        //     }

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated. */
        ESP_LOGI(TAG, "disconnect; reason=%d ", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);
        ESP_LOGI(TAG, "\n");

        /* Forget about peer. */
        peer_delete(event->disconnect.conn.conn_handle);

        /* Resume scanning. */
        blecent_scan();
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "discovery complete; reason=%d\n",
                 event->disc_complete.reason);
        return 0;

        // case BLE_GAP_EVENT_NOTIFY_RX:
        //     /* Peer sent us a notification or indication. */
        //     ESP_LOGI(INFO, "received %s; conn_handle=%d attr_handle=%d "
        //                 "attr_len=%d\n",
        //                 event->notify_rx.indication ?
        //                 "indication" :
        //                 "notification",
        //                 event->notify_rx.conn_handle,
        //                 event->notify_rx.attr_handle,
        //                 OS_MBUF_PKTLEN(event->notify_rx.om));

        //     /* Attribute data is contained in event->notify_rx.om. Use
        //      * `os_mbuf_copydata` to copy the data received in notification mbuf */
        //     return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                 event->mtu.conn_handle,
                 event->mtu.channel_id,
                 event->mtu.value);
        return 0;

    default:
        return 0;
    }
}

static void blecent_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d\n", reason);
}

static void blecent_on_sync(void)
{
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin scanning for a peripheral to connect to. */
    blecent_scan();
}