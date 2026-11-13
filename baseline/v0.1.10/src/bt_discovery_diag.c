#include <stdbool.h>
#include <stddef.h>
#include "bt_discovery_diag.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "BT_DISC";

void bt_discovery_diag_log_cod(const char *stage)
{
    esp_bt_cod_t cod = {0};
    esp_err_t err = esp_bt_gap_get_cod(&cod);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "CoD[%s] read=%s", stage, esp_err_to_name(err));
        return;
    }
    unsigned long raw = ((unsigned long)cod.service << 13) |
                        ((unsigned long)cod.major << 8) |
                        ((unsigned long)cod.minor << 2) | cod.reserved_2;
    ESP_LOGI(TAG, "CoD[%s] local=0x%06lX major=%u minor=%u service=0x%03X",
             stage, raw, (unsigned)cod.major, (unsigned)cod.minor, (unsigned)cod.service);
}

void bt_discovery_diag_after_server_start(void)
{
    bt_discovery_diag_log_cod("server-start");
    ESP_LOGI(TAG, "identity_test=%d (0=observe 1=Computer 2=Computer+EIR)", BT_DISCOVERY_TEST);
#if BT_DISCOVERY_TEST >= 1
    /* Called after SPP START so profile initialization cannot overwrite CoD.
     * Only device class changes; keep service bits chosen by the profiles.
     * A DIY terminal is a computer, not an audio/HID device. */
    const esp_bt_cod_t cod = {
        .major = ESP_BT_COD_MAJOR_DEV_COMPUTER,
        .minor = 0, /* Computer, uncategorized. */
    };
    esp_err_t err = esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_MAJOR_MINOR);
    ESP_LOGI(TAG, "set_cod Computer request=%s", esp_err_to_name(err));
#endif
#if BT_DISCOVERY_TEST >= 2
    esp_bt_eir_data_t eir = {
        .fec_required = true,
        .include_name = true,
        .include_uuid = true, /* SDK generates UUIDs from registered profiles. */
        .include_txpower = false,
        .flag = ESP_BT_EIR_FLAG_GEN_DISC,
    };
    err = esp_bt_gap_config_eir_data(&eir);
    ESP_LOGI(TAG, "config_eir name+registered_UUIDs request=%s", esp_err_to_name(err));
#endif
#if BT_DISCOVERY_TEST >= 1
    /* Queue a local-name read after the identity requests. Its callback gives
     * a later point to observe local CoD; this is not a remote RF readback.
     * There is no ESP_BT_GAP_SET_COD_EVT in this SDK despite an outdated comment. */
    err = esp_bt_gap_get_device_name();
    ESP_LOGI(TAG, "identity readback request=%s", esp_err_to_name(err));
#endif
}

void bt_discovery_diag_eir_event(const esp_bt_gap_cb_param_t *param)
{
    ESP_LOGI(TAG, "CONFIG_EIR status=%d", param->config_eir_data.stat);
    if (param->config_eir_data.stat != ESP_BT_STATUS_SUCCESS) {
        return;
    }
    ESP_LOGI(TAG, "EIR types=%u", (unsigned)param->config_eir_data.eir_type_num);
    bool name = false, uuid16 = false;
    size_t capacity = sizeof(param->config_eir_data.eir_type) /
                      sizeof(param->config_eir_data.eir_type[0]);
    size_t count = param->config_eir_data.eir_type_num;
    if (count > capacity) {
        count = capacity;
    }
    for (size_t i = 0; i < count; ++i) {
        unsigned type = param->config_eir_data.eir_type[i];
        ESP_LOGI(TAG, "EIR type=0x%02X", type);
        name |= type == ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME || type == ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME;
        uuid16 |= type == ESP_BT_EIR_TYPE_CMPL_16BITS_UUID || type == ESP_BT_EIR_TYPE_INCMPL_16BITS_UUID;
    }
    ESP_LOGI(TAG, "EIR local summary: name_type=%d uuid16_type=%d; remote reception unverified",
             name, uuid16);
}
