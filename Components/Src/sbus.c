#include "sbus.h"
#include "uart.h"
#include "os.h"
#include "daemon.h"
#include "math_utils.h"

#include <stddef.h>

#define SBUS_FRAME_SIZE 25U

static bool registered;
static sbus_t snapshot;

void sbus_init(void);
void sbus_receiveFrame(const uart_frame_t *frame);

static bool decode_frame(const uint8_t *data, sbus_t *out)
{
    size_t channel;
    if (data[0] != 0x0FU || (data[23] & 0xF0U) != 0U ||
        (data[24] != 0U && (data[24] & 0x0FU) != 0x04U)) return false;

    out->raw[0] = 0U;
    out->channel[0] = 0.0f;
    for (channel = 1U; channel <= SBUS_ANALOG_CHANNEL_COUNT; ++channel) {
        size_t bit;
        uint16_t raw = 0U;
        float value;
        for (bit = 0U; bit < 11U; ++bit) {
            size_t position = (channel - 1U) * 11U + bit;
            raw |= (uint16_t)(((data[1U + position / 8U] >>
                              (position % 8U)) & 1U) << bit);
        }
        out->raw[channel] = raw;
        value = ((float)raw - 991.5f) / 819.5f;
        out->channel[channel] = clampf(deadzone(value, SBUS_DEADZONE),
                                      -1.0f, 1.0f);
    }
    for (channel = SBUS_ANALOG_CHANNEL_COUNT + 1U;
         channel <= SBUS_CHANNEL_COUNT; ++channel) {
        out->raw[channel] = (uint16_t)((data[23] >>
                            (channel - SBUS_ANALOG_CHANNEL_COUNT - 1U)) & 1U);
        out->channel[channel] = (float)out->raw[channel];
    }
    out->frame_lost = (data[23] & 0x04U) != 0U;
    out->failsafe = (data[23] & 0x08U) != 0U;
    out->valid = true;
    out->online = !out->frame_lost && !out->failsafe;
    return true;
}

void sbus_receiveFrame(const uart_frame_t *frame)
{
    size_t offset = 0U;
    if (frame->error_flags != 0U) return;
    /* Scan complete packets within one idle-delimited UART burst. */
    while (offset + SBUS_FRAME_SIZE <= frame->length) {
        sbus_t decoded = {0};
        if (!decode_frame(&frame->data[offset], &decoded)) {
            ++offset;
            continue;
        }
        decoded.timestamp_ms = frame->timestamp_ms;
        taskENTER_CRITICAL();
        decoded.sequence = snapshot.sequence + 1U;
        snapshot = decoded;
        taskEXIT_CRITICAL();
        offset += SBUS_FRAME_SIZE;
    }
}

void sbus_init(void)
{
    registered = true;
}

int sbus_get(sbus_t *out)
{
    if (out == NULL || !registered) return -1;
    taskENTER_CRITICAL();
    *out = snapshot;
    taskEXIT_CRITICAL();
    if (!out->valid || (uint32_t)(os_getTime() - out->timestamp_ms) >= SBUS_TIMEOUT_MS)
        out->online = false;
    if (out->valid && out->online) daemon_reload();
    return 0;
}

uint32_t sbus_waitData(void)
{
    uint32_t sequence, start;
    if (!registered || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return UINT32_MAX;
    taskENTER_CRITICAL();
    sequence = snapshot.sequence;
    start = os_getTime();
    taskEXIT_CRITICAL();
    for (;;) {
        bool changed;
        taskENTER_CRITICAL();
        changed = snapshot.sequence != sequence;
        taskEXIT_CRITICAL();
        if (changed) return os_getTime() - start;
        vTaskDelay(1);
    }
}
