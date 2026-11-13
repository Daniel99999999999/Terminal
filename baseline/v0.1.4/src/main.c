/*
 * ESP32 RetroTerminal v0.1.4 - ESP-IDF 6.0.1, ESP32-WROVER (4 MB PSRAM)
 * Bluetooth Classic SPP <-> UART2, no Arduino, no busy polling.
 *
 * UART2: GPIO25 = TX, GPIO26 = RX, GND common, TTL 3.3 V ONLY.
 * GPIO16 and GPIO17 are reserved by the WROVER module PSRAM: DO NOT USE.
 * USB/UART0: ESP-IDF diagnostics, 115200 baud.
 * LCD ILI9341 8-bit I80: intentionally not initialized until wiring is defined.
 *
 * SPP_TEST_ECHO=1: Bluetooth loopback for testing without a UART device.
 * SPP_TEST_ECHO=0: transparent Bluetooth <-> UART bridge.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs_flash.h"

#if !CONFIG_BT_ENABLED || !CONFIG_BT_BLUEDROID_ENABLED || !CONFIG_BT_CLASSIC_ENABLED || !CONFIG_BT_SPP_ENABLED
#error "Enable Bluetooth / Bluedroid / Classic Bluetooth / SPP in sdkconfig."
#endif

#define BT_DEVICE_NAME     "ESP32-RetroTerminal"
#define BT_SERVICE_NAME    "RetroTerminal-SPP"
#define UART_TARGET        UART_NUM_2
#define UART_TARGET_TX_PIN 25
#define UART_TARGET_RX_PIN 26
#define UART_TARGET_BAUD   115200
#define PACKET_SIZE        128
#define QUEUE_LENGTH       32
#define SPP_TEST_ECHO      1     /* Set to 0 for normal use with a UART target. */

static const char *TAG = "RetroTerminal";

/* Log heap statistics; safe to call only from a function at run time. */
static void print_memory_info(const char *stage)
{
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

    ESP_LOGI("MEM", "=== %s ===", stage);
    ESP_LOGI("MEM", "Internal heap total: %u B",
             (unsigned)heap_caps_get_total_size(caps));
    ESP_LOGI("MEM", "Internal heap free:  %u B",
             (unsigned)heap_caps_get_free_size(caps));
    ESP_LOGI("MEM", "Largest internal block: %u B",
             (unsigned)heap_caps_get_largest_free_block(caps));

    /* PSRAM is registered with the heap allocator when CONFIG_SPIRAM is enabled.
     * A result of 0 means the PSRAM is not enabled/available in this build. */
    const uint32_t psram_caps = MALLOC_CAP_SPIRAM;
    ESP_LOGI("MEM", "External PSRAM heap total: %u B",
             (unsigned)heap_caps_get_total_size(psram_caps));
    ESP_LOGI("MEM", "External PSRAM heap free:  %u B",
             (unsigned)heap_caps_get_free_size(psram_caps));
    ESP_LOGI("MEM", "Largest PSRAM block:       %u B",
             (unsigned)heap_caps_get_largest_free_block(psram_caps));
}

typedef struct {
    uint16_t len;
    uint8_t data[PACKET_SIZE];
} packet_t;

static QueueHandle_t s_to_uart;
static QueueHandle_t s_to_bt;
static SemaphoreHandle_t s_bt_write_finished;
static portMUX_TYPE s_bt_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_bt_handle;  /* 0 means disconnected. */

static uint32_t bt_handle_get(void)
{
    uint32_t handle;
    portENTER_CRITICAL(&s_bt_lock);
    handle = s_bt_handle;
    portEXIT_CRITICAL(&s_bt_lock);
    return handle;
}

static void bt_handle_set(uint32_t handle)
{
    portENTER_CRITICAL(&s_bt_lock);
    s_bt_handle = handle;
    portEXIT_CRITICAL(&s_bt_lock);
}

/* Copies the caller's data immediately. Never blocks the Bluetooth callback. */
static bool queue_bytes(QueueHandle_t queue, const uint8_t *data, size_t len)
{
    packet_t packet;
    while (len > 0) {
        size_t count = (len > PACKET_SIZE) ? PACKET_SIZE : len;
        packet.len = (uint16_t)count;
        memcpy(packet.data, data, count);
        if (xQueueSend(queue, &packet, 0) != pdTRUE) {
            return false; /* Queue full: cannot keep up with sender. */
        }
        data += count;
        len -= count;
    }
    return true;
}

static void uart_output_task(void *arg)
{
    (void)arg;
    packet_t packet;
    for (;;) {
        if (xQueueReceive(s_to_uart, &packet, portMAX_DELAY) == pdTRUE) {
            int written = uart_write_bytes(UART_TARGET, packet.data, packet.len);
            if (written != packet.len) {
                ESP_LOGW(TAG, "UART TX error: %d/%u", written, (unsigned)packet.len);
            }
        }
    }
}

static void uart_input_task(void *arg)
{
    (void)arg;
    packet_t packet;
    for (;;) {
        int count = uart_read_bytes(UART_TARGET, packet.data, PACKET_SIZE, portMAX_DELAY);
        if (count > 0 && bt_handle_get() != 0) {
            packet.len = (uint16_t)count;
            if (xQueueSend(s_to_bt, &packet, 0) != pdTRUE) {
                ESP_LOGW(TAG, "BT TX queue full: UART data dropped");
            }
            /* Later: also feed these bytes into the ANSI parser / KS0108 screen. */
        }
    }
}

/* The sole caller of esp_spp_write(). The callback only signals completion. */
static void bt_output_task(void *arg)
{
    (void)arg;
    packet_t packet;
    for (;;) {
        if (xQueueReceive(s_to_bt, &packet, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        uint32_t handle = bt_handle_get();
        if (handle == 0) {
            continue;
        }
        /* Remove any completion token left by a previous disconnect. */
        (void)xSemaphoreTake(s_bt_write_finished, 0);
        esp_err_t err = esp_spp_write(handle, packet.len, packet.data);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SPP write failed: %s", esp_err_to_name(err));
            continue;
        }
        /* Do not change packet.data until WRITE_EVT / CONG_EVT or disconnect. */
        (void)xSemaphoreTake(s_bt_write_finished, portMAX_DELAY);
    }
}

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    if (event == ESP_BT_GAP_AUTH_CMPL_EVT) {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Bluetooth pairing OK");
        } else {
            ESP_LOGW(TAG, "Bluetooth pairing failed: %d", param->auth_cmpl.stat);
        }
    }
}

static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event) {
    case ESP_SPP_INIT_EVT:
        if (param->init.status != ESP_SPP_SUCCESS) {
            ESP_LOGE(TAG, "SPP init failed: %d", param->init.status);
            break;
        }
        ESP_ERROR_CHECK(esp_bt_gap_set_device_name(BT_DEVICE_NAME));
        ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,
                                                 ESP_BT_GENERAL_DISCOVERABLE));
        ESP_ERROR_CHECK(esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE,
                                          ESP_SPP_ROLE_SLAVE, 0, BT_SERVICE_NAME));
        break;

    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(TAG, "SPP listening as '%s'", BT_DEVICE_NAME);
        } else {
            ESP_LOGE(TAG, "SPP server failed: %d", param->start.status);
        }
        break;

    case ESP_SPP_SRV_OPEN_EVT:
        if (param->srv_open.status != ESP_SPP_SUCCESS) {
            break;
        }
        if (bt_handle_get() != 0) {
            (void)esp_spp_disconnect(param->srv_open.handle); /* One user at a time. */
            break;
        }
        xQueueReset(s_to_bt); /* Do not leak stale traffic into a new session. */
        bt_handle_set(param->srv_open.handle);
        ESP_LOGI(TAG, "Phone connected (SPP handle %lu)",
                 (unsigned long)param->srv_open.handle);
        {
#if SPP_TEST_ECHO
            static const char banner[] = "\r\nESP32 RetroTerminal v0.1.4 - BT echo test ON\r\n";
#else
            static const char banner[] = "\r\nESP32 RetroTerminal v0.1.4 - UART bridge ready\r\n";
#endif
            (void)queue_bytes(s_to_bt, (const uint8_t *)banner, sizeof(banner) - 1);
        }
        break;

    case ESP_SPP_CLOSE_EVT:
        if (param->close.handle == bt_handle_get()) {
            bt_handle_set(0);
            xQueueReset(s_to_bt);
            (void)xSemaphoreGive(s_bt_write_finished); /* Release pending writer. */
            ESP_LOGI(TAG, "Phone disconnected");
        }
        break;

    case ESP_SPP_DATA_IND_EVT:
        if (param->data_ind.handle == bt_handle_get() && param->data_ind.len > 0) {
#if SPP_TEST_ECHO
            if (!queue_bytes(s_to_bt, param->data_ind.data, param->data_ind.len)) {
                ESP_LOGW(TAG, "BT TX queue full: echo dropped");
            }
#else
            if (!queue_bytes(s_to_uart, param->data_ind.data, param->data_ind.len)) {
                ESP_LOGW(TAG, "UART TX queue full: BT data dropped");
            }
#endif
        }
        break;

    case ESP_SPP_WRITE_EVT:
        if (param->write.handle == bt_handle_get()) {
            if (param->write.status != ESP_SPP_SUCCESS) {
                ESP_LOGW(TAG, "SPP write completion status: %d", param->write.status);
            }
            if (!param->write.cong) {
                (void)xSemaphoreGive(s_bt_write_finished);
            }
        }
        break;

    case ESP_SPP_CONG_EVT:
        if (param->cong.handle == bt_handle_get() && !param->cong.cong) {
            (void)xSemaphoreGive(s_bt_write_finished);
        }
        break;

    default:
        break;
    }
}

static void uart_init_target(void)
{
    const uart_config_t config = {
        .baud_rate = UART_TARGET_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_TARGET, 2048, 2048, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_TARGET, &config));
    ESP_ERROR_CHECK(uart_set_pin(UART_TARGET, UART_TARGET_TX_PIN,
                                 UART_TARGET_RX_PIN, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART2 ready: TX=GPIO%d RX=GPIO%d, %d 8N1",
             UART_TARGET_TX_PIN, UART_TARGET_RX_PIN, UART_TARGET_BAUD);
}

static void bluetooth_init(void)
{
    /* Keys for bonded devices are stored in NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    const esp_bt_mode_t controller_mode = (esp_bt_mode_t)config.mode;
    ESP_LOGI(TAG, "BT controller configured mode: %d (Classic=2, Dual=3)",
             (int)controller_mode);

    /* ESP-IDF 6.x requires enable() mode to match init() configuration.
     * On a Dual-mode build (mode=3) enable Dual even if we only use SPP.
     * Do not release BLE memory unless controller runs in Classic-only mode.
     */
    if (controller_mode != ESP_BT_MODE_CLASSIC_BT &&
        controller_mode != ESP_BT_MODE_BTDM) {
        ESP_LOGE(TAG, "Invalid BT controller mode for SPP: %d. Configure BR/EDR or Dual in menuconfig.",
                 (int)controller_mode);
        abort();
    }
    if (controller_mode == ESP_BT_MODE_CLASSIC_BT) {
        ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
    }
    ESP_ERROR_CHECK(esp_bt_controller_init(&config));
    ESP_ERROR_CHECK(esp_bt_controller_enable(controller_mode));

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    esp_bluedroid_config_t bd_config = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bd_config));
#else
    ESP_ERROR_CHECK(esp_bluedroid_init());
#endif
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_callback));
    /* No keyboard or physical confirmation button: SSP 'Just Works'. */
    esp_bt_io_cap_t io_cap = ESP_BT_IO_CAP_NONE;
    ESP_ERROR_CHECK(esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE,
                                                   &io_cap, sizeof(io_cap)));
    ESP_ERROR_CHECK(esp_spp_register_callback(spp_callback));
    /* ESP-IDF 6.x: esp_spp_init() is no longer part of the public API. */
    const esp_spp_cfg_t spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0, /* Only used by ESP_SPP_MODE_VFS */
    };
    ESP_ERROR_CHECK(esp_spp_enhanced_init(&spp_cfg));
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32 RetroTerminal v0.1.4 WROVER (ESP-IDF 6.x)");
    print_memory_info("Before initialization");

    s_to_uart = xQueueCreate(QUEUE_LENGTH, sizeof(packet_t));
    s_to_bt = xQueueCreate(QUEUE_LENGTH, sizeof(packet_t));
    s_bt_write_finished = xSemaphoreCreateBinary();
    if (!s_to_uart || !s_to_bt || !s_bt_write_finished) {
        ESP_LOGE(TAG, "FreeRTOS allocation failed");
        abort();
    }

    uart_init_target();
    if (xTaskCreate(uart_output_task, "uart_tx", 3072, NULL, 8, NULL) != pdPASS ||
        xTaskCreate(uart_input_task, "uart_rx", 3072, NULL, 8, NULL) != pdPASS ||
        xTaskCreate(bt_output_task, "bt_tx", 4096, NULL, 8, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Cannot create FreeRTOS tasks");
        abort();
    }

    bluetooth_init();
    print_memory_info("After Bluetooth initialization");
    ESP_LOGI(TAG, "Ready. Pair with '%s' using Serial Bluetooth Terminal.", BT_DEVICE_NAME);
}
