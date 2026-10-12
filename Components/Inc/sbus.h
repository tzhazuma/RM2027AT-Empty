#ifndef RM2027AT_SBUS_H
#define RM2027AT_SBUS_H

#include <stdbool.h>
#include <stdint.h>

#define SBUS_ANALOG_CHANNEL_COUNT 16U
#define SBUS_CHANNEL_COUNT 18U
#define SBUS_TIMEOUT_MS 100U
/* Normalized center deadzone, inclusive; set to 0.0f to disable. */
#define SBUS_DEADZONE 0.05f

typedef struct {
    /* Index 0 is reserved and always zero; indices 1..18 match CH1..CH18. */
    uint16_t raw[SBUS_CHANNEL_COUNT + 1U]; /* CH1..16: 0..2047; CH17..18: 0 or 1. */
    /* CH1..16: normalized with deadzone; CH17..18: 0.0f or 1.0f. */
    float channel[SBUS_CHANNEL_COUNT + 1U];
    bool frame_lost;
    bool failsafe;
    uint32_t timestamp_ms;
    uint32_t sequence;
    bool valid; /* At least one structurally valid frame received. */
    bool online; /* Fresh frame without frame_lost or failsafe. */
} sbus_t;

/* Call remote_init() before reading either protocol. */
int sbus_get(sbus_t *out);
/* Wait for the next structurally valid frame (including failsafe frames).
 * Task context only; no timeout. UINT32_MAX means unavailable. */
uint32_t sbus_waitData(void);

#endif
