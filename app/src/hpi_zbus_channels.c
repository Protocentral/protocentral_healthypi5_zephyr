/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Ashwin Whitchurch, ProtoCentral Electronics
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/rtc.h>

#include "hw_module.h"
#include "hpi_common_types.h"

/* Helper macro: include BT listeners only when BLE is enabled. This avoids
 * referencing `bt_*_lis` symbols when `ble_module.c` is not compiled.
 */
#ifdef CONFIG_HEALTHYPI_BLE_ENABLED
#define HPI_OBSERVERS(disp, bt) ZBUS_OBSERVERS(disp, bt)
#else
#define HPI_OBSERVERS(disp, bt) ZBUS_OBSERVERS(disp)
#endif

ZBUS_CHAN_DEFINE(batt_chan,                     /* Name */
                 struct hpi_batt_status_t,      /* Message type */
                 NULL,                          /* Validator */
                 NULL,                          /* User Data */
                 HPI_OBSERVERS(disp_batt_lis, bt_batt_lis), 
                 ZBUS_MSG_INIT(0)               /* Initial value {0} */
);

ZBUS_CHAN_DEFINE(hr_chan,         /* Name */
                 struct hpi_hr_t, /* Message type */
                 NULL,            /* Validator */
                 NULL,            /* User Data */
                 HPI_OBSERVERS(disp_hr_lis, bt_hr_lis),
                 ZBUS_MSG_INIT(0) /* Initial value {0} */
);

ZBUS_CHAN_DEFINE(temp_chan, /* Name */
                 struct hpi_temp_t,
                 NULL, /* Validator */
                 NULL, /* User Data */
                 HPI_OBSERVERS(disp_temp_lis, bt_temp_lis),
                 ZBUS_MSG_INIT(0) /* Initial value {0} */
);

ZBUS_CHAN_DEFINE(spo2_chan, 
                 struct hpi_spo2_t,
                 NULL, 
                 NULL, 
                 HPI_OBSERVERS(disp_spo2_lis, bt_spo2_lis),
                 ZBUS_MSG_INIT(0) 
);

ZBUS_CHAN_DEFINE(resp_rate_chan, 
                 struct hpi_resp_rate_t,
                 NULL, 
                 NULL, 
                 HPI_OBSERVERS(disp_resp_rate_lis, bt_resp_rate_lis),
                 ZBUS_MSG_INIT(0) 
);