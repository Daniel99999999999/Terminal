#include "bt_hci_diag.h"
#include "bt_hci_scan.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/* Passive interposition on the existing Bluedroid -> internal controller path.
 * No second host, raw HCI injection, packet mutation, or controller API under lock.
 * Both original callbacks and their return values are preserved. */
void __real_esp_vhci_host_send_packet(uint8_t *data, uint16_t len);
esp_err_t __real_esp_vhci_host_register_callback(const esp_vhci_host_callback_t *callback);

#if BT_HCI_SCAN_DIAG
#define TRACE_QUEUE_LENGTH 32
static QueueHandle_t s_events;
static esp_vhci_host_callback_t s_host;
static uint32_t s_drops, s_attached;
static portMUX_TYPE s_trace_lock = portMUX_INITIALIZER_UNLOCKED;
static bt_hci_scan_state_t s_state;
static const char *TAG = "HCI_SCAN";

static void trace_status(uint32_t *drops, uint32_t *attached)
{
    portENTER_CRITICAL(&s_trace_lock);
    *drops = s_drops;
    *attached = s_attached;
    portEXIT_CRITICAL(&s_trace_lock);
}

static void observe(bool from_controller, const uint8_t *data, uint16_t len)
{
    bt_hci_scan_event_t event;
    if (s_events && bt_hci_scan_parse(from_controller, data, len, &event) &&
        xQueueSend(s_events, &event, 0) != pdTRUE) {
        portENTER_CRITICAL(&s_trace_lock);
        s_drops++;
        portEXIT_CRITICAL(&s_trace_lock);
    }
}

static void send_available(void)
{
    s_host.notify_host_send_available();
}

static int receive_packet(uint8_t *data, uint16_t len)
{
    observe(true, data, len);
    return s_host.notify_host_recv(data, len);
}

static const esp_vhci_host_callback_t s_proxy = {
    .notify_host_send_available = send_available,
    .notify_host_recv = receive_packet,
};
#endif

void __wrap_esp_vhci_host_send_packet(uint8_t *data, uint16_t len)
{
#if BT_HCI_SCAN_DIAG
    observe(false, data, len);
#endif
    __real_esp_vhci_host_send_packet(data, len);
}

esp_err_t __wrap_esp_vhci_host_register_callback(const esp_vhci_host_callback_t *callback)
{
#if BT_HCI_SCAN_DIAG
    /* SDK calls this during Bluedroid startup, before traffic is enabled.
     * Preserve invalid/partial registrations exactly instead of dereferencing them. */
    if (s_events && callback && callback->notify_host_recv &&
        callback->notify_host_send_available) {
        esp_vhci_host_callback_t previous = s_host;
        s_host = *callback;
        esp_err_t err = __real_esp_vhci_host_register_callback(&s_proxy);
        if (err != ESP_OK) {
            s_host = previous;
        } else {
            portENTER_CRITICAL(&s_trace_lock);
            s_attached = 1;
            portEXIT_CRITICAL(&s_trace_lock);
        }
        return err;
    }
#endif
    return __real_esp_vhci_host_register_callback(callback);
}

esp_err_t bt_hci_diag_init(void)
{
#if BT_HCI_SCAN_DIAG
    if (s_events) {
        return ESP_ERR_INVALID_STATE;
    }
    bt_hci_scan_init(&s_state);
    s_events = xQueueCreate(TRACE_QUEUE_LENGTH, sizeof(bt_hci_scan_event_t));
    if (!s_events) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "passive observer enabled; scan/reset metadata only; awaiting Bluedroid");
#endif
    return ESP_OK;
}

void bt_hci_diag_poll(void)
{
#if BT_HCI_SCAN_DIAG
    if (!s_events) {
        return;
    }
    uint32_t drops, attached;
    trace_status(&drops, &attached);
    if (drops) {
        bt_hci_scan_lost(&s_state);
    }
    bt_hci_scan_event_t event;
    for (unsigned i = 0; i < TRACE_QUEUE_LENGTH &&
         xQueueReceive(s_events, &event, 0) == pdTRUE; ++i) {
        uint8_t requested = 0;
        bool matched = bt_hci_scan_apply(&s_state, &event, &requested);
        if (event.kind == BT_HCI_TX) {
            if (event.has_value) {
                ESP_LOGI(TAG, "TX opcode=0x%04X WRITE_SCAN_ENABLE value=0x%02X",
                         (unsigned)event.opcode, (unsigned)event.value);
            } else {
                ESP_LOGI(TAG, "TX opcode=0x%04X %s", (unsigned)event.opcode,
                         event.opcode == BT_HCI_RESET ? "RESET" : "READ_SCAN_ENABLE");
            }
        } else if (matched) {
            ESP_LOGI(TAG, "RX COMMAND_COMPLETE opcode=0x%04X status=0x%02X requested=0x%02X",
                     (unsigned)event.opcode, (unsigned)event.status, (unsigned)requested);
        } else {
            ESP_LOGI(TAG, "RX %s opcode=0x%04X status=0x%02X value_present=%u value=0x%02X",
                     event.kind == BT_HCI_COMPLETE ? "COMMAND_COMPLETE" : "COMMAND_STATUS",
                     (unsigned)event.opcode, (unsigned)event.status,
                     (unsigned)event.has_value, (unsigned)event.value);
        }
    }
    trace_status(&drops, &attached);
    if (drops) {
        bt_hci_scan_lost(&s_state);
    }
    ESP_LOGI(TAG, "observer attached=%u drops=%lu trace_complete=%u pending=%u confirmed=%u scan=0x%02X (last observation; RF unverified)",
             (unsigned)attached, (unsigned long)drops,
             (unsigned)s_state.trace_complete, s_state.count,
             (unsigned)s_state.confirmed, (unsigned)s_state.value);
#endif
}
