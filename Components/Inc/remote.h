#ifndef RM2027AT_REMOTE_H
#define RM2027AT_REMOTE_H

/* Initialize shared USART6 reception after bsp_init(), before scheduler start.
 * Dispatches 18-byte DBUS and 25-byte SBUS packets.
 * Returns 0 on success, -1 if UART6 cannot be registered. Idempotent. */
int remote_init(void);

#endif
