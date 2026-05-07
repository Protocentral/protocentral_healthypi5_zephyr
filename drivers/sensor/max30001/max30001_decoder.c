
/*
 * MAX30001 device async decoder
 * Protocentral Electronics Pvt Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "max30001.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MAX30001_DECODER, CONFIG_SENSOR_LOG_LEVEL);

#define DT_DRV_COMPAT maxim_max30001

static int max30001_decoder_get_frame_count(const uint8_t *buffer, enum sensor_channel channel,
											size_t channel_idx, uint16_t *frame_count)
{
	ARG_UNUSED(buffer);
	ARG_UNUSED(channel);
	ARG_UNUSED(channel_idx);

	/* This sensor lacks a FIFO; there will always only be one frame at a time. */
	*frame_count = 1;
	return 0;
}

static int max30001_decoder_get_size_info(enum sensor_channel channel, size_t *base_size,
										  size_t *frame_size)
{
	switch (channel)
	{
	case SENSOR_CHAN_ECG_UV:
	case SENSOR_CHAN_BIOZ_UV:
		*base_size = sizeof(int32_t);
		*frame_size = sizeof(int32_t);
		return 0;
	default:
		return -ENOTSUP;
	}
}

static int max30001_decoder_decode(const uint8_t *buffer, enum sensor_channel channel,
								   size_t channel_idx, uint32_t *fit,
								   uint16_t max_count, void *data_out)
{
	const struct max30001_encoded_data *edata = (const struct max30001_encoded_data *)buffer;
	int32_t *out = data_out;

	ARG_UNUSED(channel_idx);

	if (*fit != 0 || max_count == 0)
	{
		return 0;
	}

	switch (channel)
	{
	case SENSOR_CHAN_ECG_UV:
		out[0] = (edata->num_samples_ecg > 0) ? edata->ecg_samples[0] : 0;
		*fit = 1;
		return 0;
	case SENSOR_CHAN_BIOZ_UV:
		out[0] = (edata->num_samples_bioz > 0) ? edata->bioz_samples[0] : 0;
		*fit = 1;
		return 0;
	default:
		return -ENOTSUP;
	}
}

SENSOR_DECODER_API_DT_DEFINE() = {
	.get_frame_count = max30001_decoder_get_frame_count,
	.get_size_info = max30001_decoder_get_size_info,
	.decode = max30001_decoder_decode,
};

int max30001_get_decoder(const struct device *dev, const struct sensor_decoder_api **decoder)
{
	ARG_UNUSED(dev);
	*decoder = &SENSOR_DECODER_NAME();

	return 0;
}