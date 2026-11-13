/* Behavioral test of the production adapter with deterministic RTOS/API stubs.
 * This checks forwarding and loss handling, not controller/FreeRTOS concurrency. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "bt_hci_diag.h"
#include "bt_hci_scan.h"
#include "esp_bt.h"
#include "freertos/queue.h"

void __wrap_esp_vhci_host_send_packet(uint8_t *, uint16_t);
esp_err_t __wrap_esp_vhci_host_register_callback(const esp_vhci_host_callback_t *);

static bt_hci_scan_event_t queue_data[32];
static unsigned queue_head, queue_count, queue_creates, sends, received, available, logs;
static bool full, in_transport, locked;
static const esp_vhci_host_callback_t *registered, *last_registration;
static esp_err_t register_result = ESP_OK;
static uint8_t *expected_packet;
static uint16_t expected_len;
static char last_log[256];

void test_hci_enter(void) { assert(!locked); locked = true; }
void test_hci_exit(void) { assert(locked); locked = false; }

QueueHandle_t xQueueCreate(unsigned count, size_t size)
{
    assert(count == 32 && size == sizeof(queue_data[0]));
    queue_creates++;
    return queue_data;
}

int xQueueSend(QueueHandle_t queue, const void *item, uint32_t timeout)
{
    assert(queue == queue_data && timeout == 0 && !locked);
    if (full || queue_count == 32) { return 0; }
    queue_data[(queue_head + queue_count++) % 32] = *(const bt_hci_scan_event_t *)item;
    return 1;
}

int xQueueReceive(QueueHandle_t queue, void *item, uint32_t timeout)
{
    assert(queue == queue_data && timeout == 0 && !locked);
    if (!queue_count) { return 0; }
    *(bt_hci_scan_event_t *)item = queue_data[queue_head];
    queue_head = (queue_head + 1) % 32;
    queue_count--;
    return 1;
}

void test_hci_log(const char *tag, const char *format, ...)
{
    assert(!in_transport && !locked && strcmp(tag, "HCI_SCAN") == 0);
    va_list args;
    va_start(args, format);
    vsnprintf(last_log, sizeof(last_log), format, args);
    va_end(args);
    logs++;
}

void __real_esp_vhci_host_send_packet(uint8_t *data, uint16_t len)
{
    assert(!locked && data == expected_packet && len == expected_len);
    sends++;
}

esp_err_t __real_esp_vhci_host_register_callback(const esp_vhci_host_callback_t *callback)
{
    assert(!locked);
    last_registration = callback;
    if (register_result == ESP_OK) { registered = callback; }
    return register_result;
}

static void host_available(void) { available++; }
static int host_receive(uint8_t *data, uint16_t len)
{
    assert(data == expected_packet && len == expected_len && !locked);
    received++;
    return 37;
}
#if BT_HCI_SCAN_DIAG
static int different_host(uint8_t *data, uint16_t len)
{
    (void)data; (void)len;
    assert(0 && "Failed registration must preserve the original host");
    return -1;
}
#endif

int main(void)
{
    const esp_vhci_host_callback_t host = {host_available, host_receive};
    assert(bt_hci_diag_init() == ESP_OK);
#if BT_HCI_SCAN_DIAG
    assert(queue_creates == 1 && bt_hci_diag_init() == ESP_ERR_INVALID_STATE);
#else
    assert(queue_creates == 0);
#endif
    assert(__wrap_esp_vhci_host_register_callback(&host) == ESP_OK);
#if BT_HCI_SCAN_DIAG
    assert(registered != &host);
#else
    assert(registered == &host);
#endif
    in_transport = true;
    registered->notify_host_send_available();
    assert(available == 1);
    uint8_t write[] = {1, 0x1A, 0x0C, 1, 3};
    expected_packet = write; expected_len = sizeof(write);
    __wrap_esp_vhci_host_send_packet(write, sizeof(write));
    assert(sends == 1 && write[4] == 3);
    uint8_t complete[] = {4, 0x0E, 4, 1, 0x1A, 0x0C, 0};
    expected_packet = complete; expected_len = sizeof(complete);
    assert(registered->notify_host_recv(complete, sizeof(complete)) == 37);
    assert(received == 1 && complete[6] == 0);
    in_transport = false;
    bt_hci_diag_poll();
#if BT_HCI_SCAN_DIAG
    assert(strstr(last_log, "attached=1 drops=0 trace_complete=1 pending=0 confirmed=1 scan=0x03"));
    full = true;
    in_transport = true;
    expected_packet = write; expected_len = sizeof(write);
    __wrap_esp_vhci_host_send_packet(write, sizeof(write));
    assert(sends == 2); /* Full trace queue must never suppress real traffic. */
    expected_packet = complete; expected_len = sizeof(complete);
    assert(registered->notify_host_recv(complete, sizeof(complete)) == 37);
    assert(received == 2);
    in_transport = false;
    full = false;
    bt_hci_diag_poll();
    assert(strstr(last_log, "drops=2 trace_complete=0 pending=0 confirmed=0"));
    const esp_vhci_host_callback_t replacement = {host_available, different_host};
    register_result = ESP_FAIL;
    assert(__wrap_esp_vhci_host_register_callback(&replacement) == ESP_FAIL);
    assert(registered->notify_host_recv(complete, sizeof(complete)) == 37);
#else
    assert(logs == 0);
#endif
    register_result = ESP_FAIL;
    assert(__wrap_esp_vhci_host_register_callback(NULL) == ESP_FAIL);
    assert(last_registration == NULL);
    const esp_vhci_host_callback_t partial = {host_available, NULL};
    assert(__wrap_esp_vhci_host_register_callback(&partial) == ESP_FAIL);
    assert(last_registration == &partial);
    puts("PASS: HCI adapter forwarding, callback/return preservation, nonblocking observation and disable path");
    return 0;
}
