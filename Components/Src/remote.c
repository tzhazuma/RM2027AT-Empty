#include "remote.h"
#include "dbus.h"
#include "uart.h"

#include <stddef.h>

/* Private protocol lifecycle and parser entry points. */
void dbus_init(void);
void sbus_init(void);
void dbus_receiveFrame(const uart_frame_t *frame);
void sbus_receiveFrame(const uart_frame_t *frame);

static bool initialized;

static void remote_receive(const uart_frame_t *frame, void *context)
{
    (void)context;
    if (frame->error_flags != 0U) return;
    switch (frame->length) {
    case 18U:
        dbus_receiveFrame(frame);
        break;
    case 25U:
        sbus_receiveFrame(frame);
        break;
    default:
        break;
    }
}

int remote_init(void)
{
    if (initialized) return 0;
    if (uart_register(UART_6, remote_receive, NULL) != 0) return -1;
    dbus_useUartInstead(false);
    dbus_init();
    sbus_init();
    initialized = true;
    return 0;
}
