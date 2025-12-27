#include "ble_service.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <string.h>

#define BLE_TARGET_NAME "TH-SERVER"   // 目标外设广播名（与采集端匹配）
static const char *TAG = "BLE_NIMBLE_CLIENT";

// 目标 Service / Characteristic UUID（128-bit）
static const ble_uuid128_t SVC_UUID = BLE_UUID128_INIT(0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
                                                      0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0);
static const ble_uuid128_t CHR_UUID = BLE_UUID128_INIT(0x21,0x43,0x65,0x87,0xA9,0xCB,0xED,0x0F,
                                                      0x21,0x43,0x65,0x87,0xA9,0xCB,0xED,0x0F);
static const ble_uuid16_t CCCD_UUID = BLE_UUID16_INIT(BLE_GATT_DSC_CLT_CFG_UUID16);

// 状态缓存
static struct {
    uint16_t conn_handle;
    uint16_t svc_start;
    uint16_t svc_end;
    uint16_t chr_val_handle;
    uint16_t cccd_handle;
    bool notify_enabled;
} g_ctx = {
    .conn_handle = BLE_HS_CONN_HANDLE_NONE,
    .svc_start = 0,
    .svc_end = 0,
    .chr_val_handle = 0,
    .cccd_handle = 0,
    .notify_enabled = false,
};

// 温湿度数据缓存（温度、湿度均为 x100）
static int16_t latest_temp_x100 = 0;
static int16_t latest_humi_x100 = 0;
static uint8_t th_char_val[4];

static void ble_handle_th_payload(const uint8_t *data, uint16_t len)
{
    if (len < 4) return;
    memcpy(th_char_val, data, 4);
    latest_temp_x100 = (int16_t)(data[1] << 8 | data[0]);
    latest_humi_x100 = (int16_t)(data[3] << 8 | data[2]);
    ESP_LOGI(TAG, "Recv TH: T=%.2fC H=%.2f%%", latest_temp_x100 / 100.0f, latest_humi_x100 / 100.0f);
    // TODO: 在此调用 HTTP/MQTT 上传云端
}

static int ble_gap_event_handler(struct ble_gap_event *event, void *arg);

static void ble_start_scanning(void)
{
    struct ble_gap_disc_params params = {
        .itvl = 0x0050,
        .window = 0x0030,
        .filter_policy = BLE_HCI_SCAN_FILT_NO_WL,
        .passive = 0,
        .limited = 0,
        .filter_duplicates = 0,
    };
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &params, ble_gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_disc rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Scanning for %s...", BLE_TARGET_NAME);
    }
}

static int ble_on_cccd_write(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg)
{
    // no-op
    return 0;
}

static int ble_on_read_char(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg)
{
    if (error->status == 0 && attr && attr->om) {
        const uint8_t *data = OS_MBUF_DATA(attr->om, const uint8_t *);
        uint16_t len = OS_MBUF_PKTLEN(attr->om);
        ble_handle_th_payload(data, len);
    }
    return 0;
}

static void ble_enable_notifications(void)
{
    if (g_ctx.cccd_handle == 0 || g_ctx.conn_handle == BLE_HS_CONN_HANDLE_NONE) return;
    uint8_t val[2] = {0x01, 0x00};
    ble_gattc_write_flat(g_ctx.conn_handle, g_ctx.cccd_handle, val, sizeof(val), ble_on_cccd_write, NULL);
    g_ctx.notify_enabled = true;
    // 主动读一次
    if (g_ctx.chr_val_handle) {
        ble_gattc_read(g_ctx.conn_handle, g_ctx.chr_val_handle, ble_on_read_char, NULL);
    }
}

static int ble_discover_descriptor_cb(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t chr_val_handle,
                                      const struct ble_gatt_dsc *dsc, void *arg)
{
    if (error->status == 0 && dsc && ble_uuid_cmp(&dsc->uuid.u, &CCCD_UUID.u) == 0) {
        g_ctx.cccd_handle = dsc->handle;
    }
    if (error->status == BLE_HS_EDONE) {
        ble_enable_notifications();
    }
    return 0;
}

static int ble_discover_characteristic_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg)
{
    if (error->status == 0 && chr && ble_uuid_cmp(&chr->uuid.u, &CHR_UUID.u) == 0) {
        g_ctx.chr_val_handle = chr->val_handle;
        // 发现描述符（查找 CCCD）
        ble_gattc_disc_all_dscs(conn_handle, chr->val_handle + 1, g_ctx.svc_end, ble_discover_descriptor_cb, NULL);
    }
    if (error->status == BLE_HS_EDONE && g_ctx.chr_val_handle == 0) {
        ESP_LOGW(TAG, "Target characteristic not found");
        ble_start_scanning();
    }
    return 0;
}

static int ble_discover_service_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *svc, void *arg)
{
    if (error->status == 0 && svc && ble_uuid_cmp(&svc->uuid.u, &SVC_UUID.u) == 0) {
        g_ctx.svc_start = svc->start_handle;
        g_ctx.svc_end = svc->end_handle;
        ESP_LOGI(TAG, "Service found: start=%u end=%u", g_ctx.svc_start, g_ctx.svc_end);
    }
    if (error->status == BLE_HS_EDONE) {
        if (g_ctx.svc_start && g_ctx.svc_end) {
            ble_gattc_disc_chrs_by_uuid(conn_handle, g_ctx.svc_start, g_ctx.svc_end, &CHR_UUID.u, ble_discover_characteristic_cb, NULL);
        } else {
            ESP_LOGW(TAG, "Service not found, restart scan");
            ble_start_scanning();
        }
    }
    return 0;
}

static void ble_reset_context(void)
{
    g_ctx.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    g_ctx.svc_start = g_ctx.svc_end = 0;
    g_ctx.chr_val_handle = 0;
    g_ctx.cccd_handle = 0;
    g_ctx.notify_enabled = false;
}

static int ble_gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        struct ble_hs_adv_fields fields;
        if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) == 0) {
            if (fields.name && fields.name_len == strlen(BLE_TARGET_NAME) &&
                memcmp(fields.name, BLE_TARGET_NAME, fields.name_len) == 0) {
                ESP_LOGI(TAG, "Found %s, connecting...", BLE_TARGET_NAME);
                ble_gap_disc_cancel();
                struct ble_gap_conn_params cp = {
                    .scan_itvl = 0x0050,
                    .scan_window = 0x0030,
                    .itvl_min = 24,
                    .itvl_max = 40,
                    .latency = 0,
                    .supervision_timeout = 400,
                    .min_ce_len = 0,
                    .max_ce_len = 0,
                };
                int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr, 30000, &cp, ble_gap_event_handler, NULL);
                if (rc != 0) {
                    ESP_LOGE(TAG, "ble_gap_connect rc=%d", rc);
                    ble_start_scanning();
                }
            }
        }
        return 0;
    }
    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status != 0) {
            ESP_LOGE(TAG, "Connect fail rc=%d", event->connect.status);
            ble_reset_context();
            ble_start_scanning();
            return 0;
        }
        g_ctx.conn_handle = event->connect.conn_handle;
        ESP_LOGI(TAG, "Connected, discovering service");
        ble_gattc_disc_svc_by_uuid(g_ctx.conn_handle, &SVC_UUID.u, ble_discover_service_cb, NULL);
        return 0;
    }
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "Disconnected");
        ble_reset_context();
        ble_start_scanning();
        return 0;
    case BLE_GAP_EVENT_NOTIFY_RX:
        if (event->notify_rx.om) {
            const uint8_t *data = OS_MBUF_DATA(event->notify_rx.om, const uint8_t *);
            uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
            ble_handle_th_payload(data, len);
        }
        return 0;
    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update: %d", event->mtu.value);
        return 0;
    default:
        return 0;
    }
}

static void ble_on_sync_callback(void)
{
    uint8_t addr_type;
    ble_hs_id_infer_auto(0, &addr_type);
    ble_hs_id_copy_addr(addr_type, NULL, NULL);
    ble_start_scanning();
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_service_init(void)
{
    ble_reset_context();

    esp_err_t ret = ESP_OK;
#ifdef esp_nimble_hci_and_controller_init
    ret = esp_nimble_hci_and_controller_init();
#else
    ret = esp_nimble_hci_init();
#endif
    if (ret) {
        ESP_LOGE(TAG, "HCI/controller init failed: %d", ret);
        return ret;
    }

    ret = nimble_port_init();
    if (ret) {
        ESP_LOGE(TAG, "NimBLE stack init failed: %d", ret);
        return ret;
    }

    ble_hs_cfg.reset_cb = NULL;
    ble_hs_cfg.sync_cb = ble_on_sync_callback;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("ESP32-TH-CLT");

    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "NimBLE client init done");
    return ESP_OK;
}

void ble_get_latest_th(int16_t *temp_x100, int16_t *humi_x100)
{
    if (temp_x100) *temp_x100 = latest_temp_x100;
    if (humi_x100) *humi_x100 = latest_humi_x100;
}
