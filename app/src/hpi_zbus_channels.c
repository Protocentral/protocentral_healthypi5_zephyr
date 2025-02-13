#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/rtc.h>

#include "hw_module.h"
#include "hpi_common_types.h"

ZBUS_CHAN_DEFINE(batt_chan,                     /* Name */
                 struct hpi_batt_status_t,      /* Message type */
                 NULL,                          /* Validator */
                 NULL,                          /* User Data */
                 ZBUS_OBSERVERS(disp_batt_lis, bt_batt_lis), 
                 ZBUS_MSG_INIT(0)               /* Initial value {0} */
);

ZBUS_CHAN_DEFINE(hr_chan,         /* Name */
                 struct hpi_hr_t, /* Message type */
                 NULL,            /* Validator */
                 NULL,            /* User Data */
                 ZBUS_OBSERVERS(disp_hr_lis, bt_hr_lis),
                 ZBUS_MSG_INIT(0) /* Initial value {0} */
);

ZBUS_CHAN_DEFINE(temp_chan, /* Name */
                 struct hpi_temp_t,
                 NULL, /* Validator */
                 NULL, /* User Data */
                 ZBUS_OBSERVERS(disp_temp_lis, bt_temp_lis),
                 ZBUS_MSG_INIT(0) /* Initial value {0} */
);

ZBUS_CHAN_DEFINE(spo2_chan, 
                 struct hpi_spo2_t,
                 NULL, 
                 NULL, 
                 ZBUS_OBSERVERS(disp_spo2_lis, bt_spo2_lis),
                 ZBUS_MSG_INIT(0) 
);

ZBUS_CHAN_DEFINE(resp_rate_chan, 
                 struct hpi_resp_rate_t,
                 NULL, 
                 NULL, 
                 ZBUS_OBSERVERS(disp_resp_rate_lis, bt_resp_rate_lis),
                 ZBUS_MSG_INIT(0) 
);