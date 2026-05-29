/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
 *
 * USBD-next configuration for HealthyPi 5.
 *
 * This file is the project-local equivalent of Zephyr's
 * samples/subsys/usb/common/sample_usbd_init.c. It instantiates one USB
 * device context bound to the board's `zephyr_udc0` UDC controller, attaches
 * language / manufacturer / product / serial string descriptors, a single
 * Full-Speed configuration, and any USBD classes enabled in Kconfig
 * (CDC-ACM in our case). Initialization (`usbd_init` + `usbd_enable`) is
 * done here so callers only need to invoke `hpi_usbd_init()` once.
 */

#include "usbd_init.h"

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hpi_usbd, LOG_LEVEL_INF);

/* HealthyPi 5 USB identifiers. The VID/PID match the legacy stack values
 * configured previously via CONFIG_USB_DEVICE_VID/PID so existing host
 * tooling continues to enumerate the device unchanged.
 */
#define HPI_USB_VID          0x1158  /* 4440 decimal */
#define HPI_USB_PID          0x0001
#define HPI_USB_MANUFACTURER "ProtoCentral Electronics"
#define HPI_USB_PRODUCT      "HealthyPi 5"
#define HPI_USB_MAX_POWER    250   /* 500 mA in 2 mA units */

/* Don't register the USB DFU class' DFU mode instance — we don't ship DFU. */
static const char *const class_blocklist[] = {
	"dfu_dfu",
	NULL,
};

USBD_DEVICE_DEFINE(hpi_usbd_ctx,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   HPI_USB_VID, HPI_USB_PID);

USBD_DESC_LANG_DEFINE(hpi_usb_lang);
USBD_DESC_MANUFACTURER_DEFINE(hpi_usb_mfr, HPI_USB_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(hpi_usb_product, HPI_USB_PRODUCT);
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(hpi_usb_sn)));

USBD_DESC_CONFIG_DEFINE(hpi_usb_fs_cfg_desc, "HealthyPi 5 FS");

/* Bus-powered, no remote wakeup. Use a #define so the macro initializer
 * resolves at preprocessor time (a `static const` variable doesn't qualify
 * as a constant expression for file-scope struct initialization).
 */
#define HPI_USB_ATTRIBUTES 0

USBD_CONFIGURATION_DEFINE(hpi_usb_fs_config,
			  HPI_USB_ATTRIBUTES,
			  HPI_USB_MAX_POWER,
			  &hpi_usb_fs_cfg_desc);

/*
 * USB classes with multiple interfaces (CDC-ACM, CDC-ECM, audio, video, ...)
 * require Interface Association Descriptors. The corresponding class-code
 * triple on the device descriptor is "Miscellaneous / Common Class / IAD".
 */
static void hpi_usb_fix_code_triple(struct usbd_context *uds_ctx,
				    const enum usbd_speed speed)
{
	if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_CDC_NCM_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS) ||
	    IS_ENABLED(CONFIG_USBD_VIDEO_CLASS)) {
		usbd_device_set_code_triple(uds_ctx, speed,
					    USB_BCC_MISCELLANEOUS, 0x02, 0x01);
	} else {
		usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
	}
}

struct usbd_context *hpi_usbd_init(usbd_msg_cb_t msg_cb)
{
	int err;

	err = usbd_add_descriptor(&hpi_usbd_ctx, &hpi_usb_lang);
	if (err) {
		LOG_ERR("Failed to add language descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&hpi_usbd_ctx, &hpi_usb_mfr);
	if (err) {
		LOG_ERR("Failed to add manufacturer descriptor (%d)", err);
		return NULL;
	}

	err = usbd_add_descriptor(&hpi_usbd_ctx, &hpi_usb_product);
	if (err) {
		LOG_ERR("Failed to add product descriptor (%d)", err);
		return NULL;
	}

	IF_ENABLED(CONFIG_HWINFO, (
		err = usbd_add_descriptor(&hpi_usbd_ctx, &hpi_usb_sn);
		if (err) {
			LOG_ERR("Failed to add serial-number descriptor (%d)", err);
			return NULL;
		}
	))

	err = usbd_add_configuration(&hpi_usbd_ctx, USBD_SPEED_FS,
				     &hpi_usb_fs_config);
	if (err) {
		LOG_ERR("Failed to add Full-Speed configuration (%d)", err);
		return NULL;
	}

	err = usbd_register_all_classes(&hpi_usbd_ctx, USBD_SPEED_FS, 1,
					class_blocklist);
	if (err) {
		LOG_ERR("Failed to register USBD classes (%d)", err);
		return NULL;
	}

	hpi_usb_fix_code_triple(&hpi_usbd_ctx, USBD_SPEED_FS);
	usbd_self_powered(&hpi_usbd_ctx, HPI_USB_ATTRIBUTES & USB_SCD_SELF_POWERED);

	if (msg_cb != NULL) {
		err = usbd_msg_register_cb(&hpi_usbd_ctx, msg_cb);
		if (err) {
			LOG_ERR("Failed to register USBD message callback (%d)", err);
			return NULL;
		}
	}

	err = usbd_init(&hpi_usbd_ctx);
	if (err) {
		LOG_ERR("usbd_init failed (%d)", err);
		return NULL;
	}

	/* If the UDC driver can report VBUS state, defer enable until VBUS is
	 * present (the msg_cb handles VBUS_READY / VBUS_REMOVED). Otherwise
	 * enable unconditionally — the RP2040 UDC does not support VBUS
	 * detection, so this branch is what we actually take.
	 */
	if (!usbd_can_detect_vbus(&hpi_usbd_ctx)) {
		err = usbd_enable(&hpi_usbd_ctx);
		if (err) {
			LOG_ERR("usbd_enable failed (%d)", err);
			return NULL;
		}
	}

	LOG_INF("USBD-next stack initialized (VID=0x%04x PID=0x%04x)",
		HPI_USB_VID, HPI_USB_PID);

	return &hpi_usbd_ctx;
}
