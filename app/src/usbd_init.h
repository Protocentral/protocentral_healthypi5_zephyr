/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
 */

#ifndef HEALTHYPI_USBD_INIT_H
#define HEALTHYPI_USBD_INIT_H

#include <zephyr/usb/usbd.h>

/*
 * Configure, initialize, and enable the USBD-next stack for HealthyPi 5.
 *
 * Returns the configured USB device context on success, or NULL on failure.
 * The caller does not own the returned pointer (storage is static).
 *
 * `msg_cb` may be NULL. If supplied, it receives USBD lifecycle events
 * (VBUS up/down, CDC-ACM DTR/line-coding) via `usbd_msg_register_cb`.
 */
struct usbd_context *hpi_usbd_init(usbd_msg_cb_t msg_cb);

#endif /* HEALTHYPI_USBD_INIT_H */
