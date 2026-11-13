/*
 * ESP32 RetroTerminal v0.1.7 - ESP-IDF 6.0.1, ESP32-WROVER (4 MB mapped PSRAM)
 * Bluetooth Classic SPP <-> UART2, no Arduino, no busy polling.
 *
 * UART2: GPIO25 = TX, GPIO26 = RX, GND common, TTL 3.3 V ONLY.
 * GPIO16 and GPIO17 are reserved by the WROVER module PSRAM: DO NOT USE.
 * USB/UART0: ESP-IDF diagnostics, 115200 baud.
 * LCD controller and wiring are unconfirmed; LCD/touch remain uninitialized.
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
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bt_tx_state.h"
#include "debug_led.h"
#include "bt_discovery_diag.h"

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
#ifndef SPP_TEST_ECHO
#define SPP_TEST_ECHO      1     /* Set to 0 for normal use with a UART target. */
#endif
#define UART_EVENT_QUEUE_LENGTH 32
#define SPP_WRITE_TIMEOUT_MS    3000
#define SPP_RECOVERY_TIMEOUT_MS 5000
#define IO_DIAGNOSTICS_MS       5000

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
    bt_session_t session;
    uint16_t len;
    uint8_t data[PACKET_SIZE];
} packet_t;

static QueueHandle_t s_to_uart;
static QueueHandle_t s_to_bt;
static QueueHandle_t s_uart_events;
static SemaphoreHandle_t s_spp_uninit_done;
static SemaphoreHandle_t s_spp_start_done;
static TaskHandle_t s_bt_task;
static portMUX_TYPE s_bt_lock = portMUX_INITIALIZER_UNLOCKED;
static bt_tx_state_t s_bt;
static esp_spp_status_t s_uninit_status;
typedef enum { SPP_STOPPED, SPP_STARTING, SPP_RUNNING, SPP_STOPPING } spp_phase_t;
static spp_phase_t s_spp_phase;

static debug_led_mode_t debug_led_bt_mode(void)
{
    portENTER_CRITICAL(&s_bt_lock);
    debug_led_mode_t mode = s_bt.fault ? DEBUG_LED_ERROR :
        (s_bt.ready && s_bt.session.handle ? DEBUG_LED_CONNECTED : DEBUG_LED_DISCONNECTED);
    portEXIT_CRITICAL(&s_bt_lock);
    return mode;
}

typedef struct {
    uint32_t uart_rx_bytes, bt_rx_events, bt_write_events, bt_cong_events;
    uint32_t bt_dropped_bytes, uart_dropped_bytes, stale_bytes;
    uint32_t uart_offline_bytes, uart_boundary_bytes;
    uint32_t uart_fifo_overflows, uart_buffer_full, uart_frame_errors;
    uint32_t uart_parity_errors, uart_read_errors, spp_recoveries;
} io_stats_t;

static io_stats_t s_stats;
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;

static void stats_add(uint32_t *counter, uint32_t count)
{
    portENTER_CRITICAL(&s_stats_lock);
    *counter += count;
    portEXIT_CRITICAL(&s_stats_lock);
}

static bt_session_t bt_session_get(void)
{
    bt_session_t session;
    portENTER_CRITICAL(&s_bt_lock);
    session = s_bt.session;
    if (!bt_tx_is_current(&s_bt, session)) {
        session.handle = 0;
    }
    portEXIT_CRITICAL(&s_bt_lock);
    return session;
}

static bool bt_session_current(bt_session_t session)
{
    bool current;
    portENTER_CRITICAL(&s_bt_lock);
    current = bt_tx_is_current(&s_bt, session);
    portEXIT_CRITICAL(&s_bt_lock);
    return current;
}

static void uart_boundary_wake(void)
{
    /* A control marker wakes RX immediately; a full queue already wakes it. */
    uart_event_t marker = { .type = UART_EVENT_MAX };
    if (s_uart_events) {
        (void)xQueueSend(s_uart_events, &marker, 0);
    }
}

static void bt_notify(void)
{
    if (s_bt_task) {
        xTaskNotifyGive(s_bt_task);
    }
}

static void bt_fail(void)
{
    portENTER_CRITICAL(&s_bt_lock);
    bt_tx_fail(&s_bt);
    portEXIT_CRITICAL(&s_bt_lock);
    uart_boundary_wake();
    /* The writer must not wake its own recovery backoff on an API failure. */
    if (xTaskGetCurrentTaskHandle() != s_bt_task) {
        bt_notify();
    }
}

/* Called by uart_rx only. Aggregate output avoids logging every BT packet. */
static void print_io_stats(void)
{
    io_stats_t stats;
    portENTER_CRITICAL(&s_stats_lock);
    stats = s_stats;
    portEXIT_CRITICAL(&s_stats_lock);
    ESP_LOGI("IO", "UART RX=%lu offline=%lu boundary_discard=%lu; dropped BT=%lu UART=%lu stale=%lu bytes",
             (unsigned long)stats.uart_rx_bytes, (unsigned long)stats.uart_offline_bytes,
             (unsigned long)stats.uart_boundary_bytes, (unsigned long)stats.bt_dropped_bytes,
             (unsigned long)stats.uart_dropped_bytes, (unsigned long)stats.stale_bytes);
    ESP_LOGI("IO", "UART errors FIFO_OVF=%lu BUFFER_FULL=%lu FRAME=%lu PARITY=%lu READ=%lu; SPP DATA_IND=%lu WRITE=%lu CONG=%lu recovery=%lu",
             (unsigned long)stats.uart_fifo_overflows, (unsigned long)stats.uart_buffer_full,
             (unsigned long)stats.uart_frame_errors, (unsigned long)stats.uart_parity_errors,
             (unsigned long)stats.uart_read_errors, (unsigned long)stats.bt_rx_events,
             (unsigned long)stats.bt_write_events, (unsigned long)stats.bt_cong_events,
             (unsigned long)stats.spp_recoveries);
}

/* Copies the caller's data immediately. Never blocks the Bluetooth callback. */
static bool queue_bytes(QueueHandle_t queue, bt_session_t session,
                        const uint8_t *data, size_t len)
{
    packet_t packet = { .session = session };
    while (len > 0) {
        size_t count = (len > PACKET_SIZE) ? PACKET_SIZE : len;
        packet.len = (uint16_t)count;
        memcpy(packet.data, data, count);
        if (xQueueSend(queue, &packet, 0) != pdTRUE) {
            stats_add(queue == s_to_bt ? &s_stats.bt_dropped_bytes
                                      : &s_stats.uart_dropped_bytes, (uint32_t)len);
            return false; /* Queue full: cannot keep up with sender. */
        }
        if (queue == s_to_bt) {
            bt_notify();
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
        debug_led_heartbeat(DEBUG_LED_UART_TX);
        if (xQueueReceive(s_to_uart, &packet, pdMS_TO_TICKS(250)) == pdTRUE) {
            if (!bt_session_current(packet.session)) {
                stats_add(&s_stats.stale_bytes, packet.len);
                continue;
            }
            int written = uart_write_bytes(UART_TARGET, packet.data, packet.len);
            if (written != packet.len) {
                debug_led_fault();
                stats_add(&s_stats.uart_dropped_bytes,
                          packet.len - (written > 0 ? (uint32_t)written : 0));
                ESP_LOGW(TAG, "UART TX error: %d/%u", written, (unsigned)packet.len);
            }
        }
    }
}

static void uart_input_task(void *arg)
{
    (void)arg;
    packet_t packet;
    uart_event_t event;
    uint64_t generation = 0;
    TickType_t last_report = xTaskGetTickCount();
    for (;;) {
        debug_led_heartbeat(DEBUG_LED_UART_RX);
        /* A finite wait also drains RX if a UART_DATA event was lost because
         * the driver event queue filled up. No busy polling when idle. */
        if (xQueueReceive(s_uart_events, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (event.type) {
            case UART_FIFO_OVF: stats_add(&s_stats.uart_fifo_overflows, 1); break;
            case UART_BUFFER_FULL: stats_add(&s_stats.uart_buffer_full, 1); break;
            case UART_FRAME_ERR: stats_add(&s_stats.uart_frame_errors, 1); break;
            case UART_PARITY_ERR: stats_add(&s_stats.uart_parity_errors, 1); break;
            default: break;
            }
        }
        bt_session_t session = bt_session_get();
        if (generation != session.generation) {
            /* Deliberate session boundary only: discard pre-connection data.
             * Do not reset the event queue: retain pending error reports. */
            size_t buffered = 0;
            ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_TARGET, &buffered));
            ESP_ERROR_CHECK(uart_flush_input(UART_TARGET));
            stats_add(&s_stats.uart_boundary_bytes, (uint32_t)buffered);
            generation = session.generation;
            ESP_LOGI(TAG, "UART RX session boundary: handle=%lu buffered_discard=%u",
                     (unsigned long)session.handle, (unsigned)buffered);
        }
        /* At most one RX buffer per pass, so errors and diagnostics get time. */
        for (unsigned chunk = 0; chunk < 2048 / PACKET_SIZE; ++chunk) {
            session = bt_session_get();
            if (session.generation != generation) {
                break; /* Apply the new boundary policy on the next pass. */
            }
            int count = uart_read_bytes(UART_TARGET, packet.data, PACKET_SIZE, 0);
            if (count <= 0) {
                if (count < 0) {
                    debug_led_fault();
                    stats_add(&s_stats.uart_read_errors, 1);
                }
                break;
            }
            stats_add(&s_stats.uart_rx_bytes, (uint32_t)count);
            if (!session.handle) {
                stats_add(&s_stats.uart_offline_bytes, (uint32_t)count);
                continue;
            }
            if (!bt_session_current(session)) {
                stats_add(&s_stats.stale_bytes, (uint32_t)count);
                continue;
            }
            packet.session = session;
            packet.len = (uint16_t)count;
            if (xQueueSend(s_to_bt, &packet, 0) != pdTRUE) {
                stats_add(&s_stats.bt_dropped_bytes, (uint32_t)count);
            } else {
                bt_notify();
            }
            /* Later: also feed bytes into the terminal parser / verified LCD. */
        }
        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - last_report) >= pdMS_TO_TICKS(IO_DIAGNOSTICS_MS)) {
            print_io_stats();
            last_report = now;
        }
    }
}

static esp_err_t spp_init_profile(void)
{
    const esp_spp_cfg_t config = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0,
    };
    return esp_spp_enhanced_init(&config);
}

static bool spp_recover(void)
{
    /* Stop accepting sessions before deinit. UNINIT is a barrier for late
     * callbacks, including errors whose WRITE event has handle=0. */
    bt_fail();
    portENTER_CRITICAL(&s_bt_lock);
    s_spp_phase = SPP_STOPPING;
    portEXIT_CRITICAL(&s_bt_lock);
    stats_add(&s_stats.spp_recoveries, 1);
    packet_t discarded;
    while (xQueueReceive(s_to_bt, &discarded, 0) == pdTRUE) {
        stats_add(&s_stats.stale_bytes, discarded.len);
    }
    while (xQueueReceive(s_to_uart, &discarded, 0) == pdTRUE) {
        stats_add(&s_stats.stale_bytes, discarded.len);
    }
    (void)xSemaphoreTake(s_spp_uninit_done, 0);
    debug_led_heartbeat(DEBUG_LED_BT_TX);
    esp_err_t err = esp_spp_deinit();
    ESP_LOGW(TAG, "SPP recovery: deinit request=%s", esp_err_to_name(err));
    if (err != ESP_OK || xSemaphoreTake(s_spp_uninit_done,
                                       pdMS_TO_TICKS(SPP_RECOVERY_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "SPP recovery: no UNINIT confirmation; TX remains disabled");
        return false;
    }
    portENTER_CRITICAL(&s_bt_lock);
    esp_spp_status_t status = s_uninit_status;
    portEXIT_CRITICAL(&s_bt_lock);
    if (status != ESP_SPP_SUCCESS && status != ESP_SPP_NEED_INIT) {
        ESP_LOGE(TAG, "SPP recovery: UNINIT status=%d", status);
        return false;
    }
    (void)xSemaphoreTake(s_spp_start_done, 0);
    portENTER_CRITICAL(&s_bt_lock);
    s_spp_phase = SPP_STARTING;
    portEXIT_CRITICAL(&s_bt_lock);
    err = spp_init_profile();
    ESP_LOGI(TAG, "SPP recovery: init request=%s", esp_err_to_name(err));
    debug_led_heartbeat(DEBUG_LED_BT_TX);
    if (err != ESP_OK || xSemaphoreTake(s_spp_start_done,
                                       pdMS_TO_TICKS(SPP_RECOVERY_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "SPP recovery: no server START; TX remains disabled");
        portENTER_CRITICAL(&s_bt_lock);
        s_spp_phase = SPP_STOPPING;
        bt_tx_fail(&s_bt);
        portEXIT_CRITICAL(&s_bt_lock);
        return false;
    }
    portENTER_CRITICAL(&s_bt_lock);
    bool ready = s_bt.ready && !s_bt.fault;
    portEXIT_CRITICAL(&s_bt_lock);
    return ready;
}

/* Sole writer; notifications are wake hints, never completion tokens.
 * Completion/congestion/failure are persistent state protected by s_bt_lock. */
static void bt_output_task(void *arg)
{
    (void)arg;
    packet_t packet;
    for (;;) {
        debug_led_heartbeat(DEBUG_LED_BT_TX);
        portENTER_CRITICAL(&s_bt_lock);
        bool fault = s_bt.fault;
        portEXIT_CRITICAL(&s_bt_lock);
        if (fault) {
            bool recovered = spp_recover();
            debug_led_heartbeat(DEBUG_LED_BT_TX);
            if (!recovered) {
                /* Bounded calls, no traffic sent through an uncertain profile. */
                (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            }
            continue;
        }
        if (xQueueReceive(s_to_bt, &packet, 0) != pdTRUE) {
            (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
            continue;
        }
        TickType_t started = xTaskGetTickCount();
        bt_tx_begin_t begin;
        do {
            debug_led_heartbeat(DEBUG_LED_BT_TX);
            portENTER_CRITICAL(&s_bt_lock);
            begin = bt_tx_begin(&s_bt, packet.session, packet.len);
            portEXIT_CRITICAL(&s_bt_lock);
            if (begin == BT_TX_BLOCKED) {
                (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
                if ((TickType_t)(xTaskGetTickCount() - started) >=
                    pdMS_TO_TICKS(SPP_WRITE_TIMEOUT_MS)) {
                    ESP_LOGE(TAG, "SPP congestion timeout; restarting profile");
                    bt_fail();
                    break;
                }
            }
        } while (begin == BT_TX_BLOCKED);
        if (begin != BT_TX_STARTED) {
            stats_add(&s_stats.stale_bytes, packet.len);
            continue;
        }
        /* Use the packet's reserved handle; never select a new session here.
         * A close after reservation faults the profile and blocks new opens. */
        esp_err_t err = esp_spp_write(packet.session.handle, packet.len, packet.data);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SPP write request failed: %s", esp_err_to_name(err));
            bt_fail();
        }
        started = xTaskGetTickCount();
        for (;;) {
            debug_led_heartbeat(DEBUG_LED_BT_TX);
            portENTER_CRITICAL(&s_bt_lock);
            bt_tx_result_t result = bt_tx_result(&s_bt);
            if (result == BT_TX_DONE) {
                bt_tx_finish(&s_bt);
            }
            portEXIT_CRITICAL(&s_bt_lock);
            if (result != BT_TX_WAIT) {
                if (result == BT_TX_FAULT) {
                    stats_add(&s_stats.bt_dropped_bytes, packet.len);
                }
                break;
            }
            if ((TickType_t)(xTaskGetTickCount() - started) >=
                pdMS_TO_TICKS(SPP_WRITE_TIMEOUT_MS)) {
                ESP_LOGE(TAG, "SPP WRITE/congestion timeout; restarting profile");
                stats_add(&s_stats.bt_dropped_bytes, packet.len);
                bt_fail();
                break;
            }
            (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        }
    }
}

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        ESP_LOGI(TAG, "GAP AUTH status=%d", param->auth_cmpl.stat);
        break;
    case ESP_BT_GAP_GET_DEV_NAME_CMPL_EVT:
        ESP_LOGI(TAG, "GAP GET_DEV_NAME status=%d name='%.*s'",
                 param->get_dev_name_cmpl.status, ESP_BT_GAP_MAX_BDNAME_LEN,
                 param->get_dev_name_cmpl.name ? param->get_dev_name_cmpl.name : "");
        bt_discovery_diag_log_cod("name-callback");
        break;
    case ESP_BT_GAP_CONFIG_EIR_DATA_EVT:
        bt_discovery_diag_eir_event(param);
        break;
    case ESP_BT_GAP_ENC_CHG_EVT:
        ESP_LOGI(TAG, "GAP encryption mode=%u", (unsigned)param->enc_chg.enc_mode);
        break;
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
        ESP_LOGI(TAG, "GAP ACL connect status=%d handle=%u",
                 param->acl_conn_cmpl_stat.stat, param->acl_conn_cmpl_stat.handle);
        break;
    case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT: {
        unsigned reason = param->acl_disconn_cmpl_stat.reason;
        if (reason >= ESP_BT_STATUS_BASE_FOR_HCI_ERR &&
            reason < ESP_BT_STATUS_BASE_FOR_HCI_ERR + 0x100) {
            unsigned hci = reason - ESP_BT_STATUS_BASE_FOR_HCI_ERR;
            ESP_LOGI(TAG, "GAP ACL disconnect reason=%u (0x%03X) hci=0x%02X%s handle=%u",
                     reason, reason, hci, hci == 0x22 ? " LMP_RESPONSE_TIMEOUT" : "",
                     param->acl_disconn_cmpl_stat.handle);
        } else {
            ESP_LOGI(TAG, "GAP ACL disconnect reason=%u handle=%u",
                     reason, param->acl_disconn_cmpl_stat.handle);
        }
        break;
    }
    case ESP_BT_GAP_PIN_REQ_EVT: {
        /* No PIN entry UI. Reject legacy pairing explicitly; do not invent a PIN. */
        esp_bt_pin_code_t unused_pin = {0};
        esp_err_t err = esp_bt_gap_pin_reply(param->pin_req.bda, false, 0, unused_pin);
        ESP_LOGW(TAG, "GAP PIN_REQ: legacy pairing unsupported, reject=%s",
                 esp_err_to_name(err));
        break;
    }
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGW(TAG, "GAP CFM_REQ: no confirmation UI, reject=%s",
                 esp_err_to_name(esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, false)));
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGW(TAG, "GAP KEY_REQ: no passkey UI, reject=%s",
                 esp_err_to_name(esp_bt_gap_ssp_passkey_reply(param->key_req.bda, false, 0)));
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(TAG, "GAP KEY_NOTIF (passkey omitted)");
        break;
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        ESP_LOGI(TAG, "GAP local inquiry state=%d", param->disc_st_chg.state);
        break;
    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(TAG, "GAP link power mode=%d", param->mode_chg.mode);
        break;
    default:
        ESP_LOGD(TAG, "GAP event=%d", event);
        break;
    }
}

static void spp_start_failed(void)
{
    bt_fail();
    (void)xSemaphoreGive(s_spp_start_done);
}

static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event) {
    case ESP_SPP_INIT_EVT: {
        ESP_LOGI(TAG, "SPP INIT status=%d", param->init.status);
        portENTER_CRITICAL(&s_bt_lock);
        bool starting = s_spp_phase == SPP_STARTING;
        portEXIT_CRITICAL(&s_bt_lock);
        if (!starting) {
            ESP_LOGW(TAG, "SPP late INIT ignored during recovery");
            break;
        }
        if (param->init.status != ESP_SPP_SUCCESS) {
            spp_start_failed();
            break;
        }
        /* Keep v0.1.4 initialization order, now with explicit API diagnostics. */
        esp_err_t err = esp_bt_gap_set_device_name(BT_DEVICE_NAME);
        ESP_LOGI(TAG, "BT set_name requested='%s' result=%s", BT_DEVICE_NAME, esp_err_to_name(err));
        if (err != ESP_OK) {
            spp_start_failed();
            break;
        }
        err = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        ESP_LOGI(TAG, "BT scan_mode connectable/general result=%s (request accepted only)", esp_err_to_name(err));
        if (err != ESP_OK) {
            spp_start_failed();
            break;
        }
        err = esp_bt_gap_get_device_name();
        ESP_LOGI(TAG, "BT get_name request result=%s", esp_err_to_name(err));
        err = esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_SLAVE, 0, BT_SERVICE_NAME);
        ESP_LOGI(TAG, "SPP start_server request=%s", esp_err_to_name(err));
        if (err != ESP_OK) {
            spp_start_failed();
        }
        break;
    }

    case ESP_SPP_START_EVT: {
        ESP_LOGI(TAG, "SPP START status=%d handle=%lu scn=%u", param->start.status,
                 (unsigned long)param->start.handle, param->start.scn);
        portENTER_CRITICAL(&s_bt_lock);
        bool starting = s_spp_phase == SPP_STARTING;
        if (starting && param->start.status == ESP_SPP_SUCCESS) {
            bt_tx_server_started(&s_bt);
            s_spp_phase = SPP_RUNNING;
        }
        portEXIT_CRITICAL(&s_bt_lock);
        if (!starting) {
            ESP_LOGW(TAG, "SPP late START ignored during recovery");
        } else if (param->start.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(TAG, "SPP listening as '%s'; RF discoverability not verified", BT_DEVICE_NAME);
            bt_discovery_diag_after_server_start();
            (void)xSemaphoreGive(s_spp_start_done);
        } else {
            spp_start_failed();
        }
        bt_notify();
        break;
    }

    case ESP_SPP_UNINIT_EVT: {
        ESP_LOGI(TAG, "SPP UNINIT status=%d", param->uninit.status);
        portENTER_CRITICAL(&s_bt_lock);
        bool stopping = s_spp_phase == SPP_STOPPING;
        if (stopping) {
            s_uninit_status = param->uninit.status;
            s_spp_phase = SPP_STOPPED;
        }
        portEXIT_CRITICAL(&s_bt_lock);
        if (stopping) {
            (void)xSemaphoreGive(s_spp_uninit_done);
        }
        break;
    }

    case ESP_SPP_SRV_OPEN_EVT: {
        ESP_LOGI(TAG, "SPP SRV_OPEN status=%d handle=%lu", param->srv_open.status,
                 (unsigned long)param->srv_open.handle);
        if (param->srv_open.status != ESP_SPP_SUCCESS) {
            break;
        }
        portENTER_CRITICAL(&s_bt_lock);
        bool accepted = bt_tx_open(&s_bt, param->srv_open.handle);
        bt_session_t session = s_bt.session;
        portEXIT_CRITICAL(&s_bt_lock);
        if (!accepted) {
            esp_err_t err = esp_spp_disconnect(param->srv_open.handle);
            ESP_LOGW(TAG, "SPP client rejected (busy/recovering), disconnect=%s", esp_err_to_name(err));
            break;
        }
        uart_boundary_wake();
#if SPP_TEST_ECHO
        static const char banner[] = "\r\nESP32 RetroTerminal v0.1.7 - BT echo test ON\r\n";
        (void)queue_bytes(s_to_bt, session, (const uint8_t *)banner, sizeof(banner) - 1);
#endif
        /* No injected banner in transparent bridge mode. */
        (void)session;
        bt_notify();
        break;
    }

    case ESP_SPP_CLOSE_EVT: {
        ESP_LOGI(TAG, "SPP CLOSE status=%d handle=%lu remote=%d", param->close.status,
                 (unsigned long)param->close.handle, param->close.async);
        portENTER_CRITICAL(&s_bt_lock);
        bool closed = bt_tx_close(&s_bt, param->close.handle);
        portEXIT_CRITICAL(&s_bt_lock);
        if (closed) {
            uart_boundary_wake();
        }
        bt_notify();
        break;
    }

    case ESP_SPP_DATA_IND_EVT: {
        stats_add(&s_stats.bt_rx_events, 1);
        bt_session_t session = bt_session_get();
        if (session.handle && param->data_ind.status == ESP_SPP_SUCCESS &&
            param->data_ind.handle == session.handle && param->data_ind.len > 0) {
#if SPP_TEST_ECHO
            (void)queue_bytes(s_to_bt, session, param->data_ind.data, param->data_ind.len);
#else
            (void)queue_bytes(s_to_uart, session, param->data_ind.data, param->data_ind.len);
#endif
        }
        break;
    }

    case ESP_SPP_WRITE_EVT:
        stats_add(&s_stats.bt_write_events, 1);
        if (param->write.status != ESP_SPP_SUCCESS) {
            ESP_LOGW(TAG, "SPP WRITE status=%d handle=%lu len=%d", param->write.status,
                     (unsigned long)param->write.handle, param->write.len);
        }
        portENTER_CRITICAL(&s_bt_lock);
        bt_tx_written(&s_bt, param->write.handle, param->write.status == ESP_SPP_SUCCESS,
                       param->write.len, param->write.cong);
        portEXIT_CRITICAL(&s_bt_lock);
        bt_notify();
        break;

    case ESP_SPP_CONG_EVT:
        stats_add(&s_stats.bt_cong_events, 1);
        portENTER_CRITICAL(&s_bt_lock);
        if (param->cong.status != ESP_SPP_SUCCESS &&
            param->cong.handle == s_bt.session.handle && param->cong.handle) {
            bt_tx_fail(&s_bt);
        } else {
            bt_tx_congestion(&s_bt, param->cong.handle, param->cong.cong);
        }
        portEXIT_CRITICAL(&s_bt_lock);
        bt_notify();
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
    ESP_ERROR_CHECK(uart_driver_install(UART_TARGET, 2048, 2048,
                                        UART_EVENT_QUEUE_LENGTH, &s_uart_events, 0));
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
    ESP_LOGI(TAG, "Bluedroid config: SSP=%d SC=%d", bd_config.ssp_en, bd_config.sc_en);
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bd_config));
#else
    ESP_ERROR_CHECK(esp_bluedroid_init());
#endif
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    const uint8_t *address = esp_bt_dev_get_address();
    if (address) {
        ESP_LOGI(TAG, "BT local address: %02X:%02X:%02X:%02X:%02X:%02X",
                 address[0], address[1], address[2], address[3], address[4], address[5]);
    } else {
        ESP_LOGE(TAG, "BT local address unavailable");
    }

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_callback));
    ESP_LOGI(TAG, "BT saved bonds=%d (read only)", esp_bt_gap_get_bond_device_num());
    /* No keyboard or physical confirmation button: SSP 'Just Works'. */
    esp_bt_io_cap_t io_cap = ESP_BT_IO_CAP_NONE;
    err = esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &io_cap, sizeof(io_cap));
    ESP_LOGI(TAG, "BT SSP IO_CAP_NONE request=%s", esp_err_to_name(err));
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_spp_register_callback(spp_callback));
    /* ESP-IDF 6.x: esp_spp_init() is no longer part of the public API. */
    portENTER_CRITICAL(&s_bt_lock);
    s_spp_phase = SPP_STARTING;
    portEXIT_CRITICAL(&s_bt_lock);
    ESP_ERROR_CHECK(spp_init_profile());
}

void app_main(void)
{
    ESP_ERROR_CHECK(debug_led_init(debug_led_bt_mode));
    ESP_LOGI(TAG, "Starting ESP32 RetroTerminal v0.1.7 WROVER; SDK: %s; echo=%d identity_test=%d",
             esp_get_idf_version(), SPP_TEST_ECHO, BT_DISCOVERY_TEST);
    print_memory_info("Before initialization");

    s_to_uart = xQueueCreate(QUEUE_LENGTH, sizeof(packet_t));
    s_to_bt = xQueueCreate(QUEUE_LENGTH, sizeof(packet_t));
    s_spp_uninit_done = xSemaphoreCreateBinary();
    s_spp_start_done = xSemaphoreCreateBinary();
    if (!s_to_uart || !s_to_bt || !s_spp_uninit_done || !s_spp_start_done) {
        ESP_LOGE(TAG, "FreeRTOS allocation failed");
        abort();
    }

    uart_init_target();
    if (xTaskCreate(uart_output_task, "uart_tx", 3072, NULL, 8, NULL) != pdPASS ||
        xTaskCreate(uart_input_task, "uart_rx", 3072, NULL, 8, NULL) != pdPASS ||
        xTaskCreate(bt_output_task, "bt_tx", 4096, NULL, 8, &s_bt_task) != pdPASS) {
        ESP_LOGE(TAG, "Cannot create FreeRTOS tasks");
        abort();
    }

    bluetooth_init();
    print_memory_info("After Bluetooth init request");
    ESP_LOGI(TAG, "SPP initialization requested; await INIT/START for '%s'", BT_DEVICE_NAME);
}
