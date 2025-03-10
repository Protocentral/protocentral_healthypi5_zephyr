
/*
 * AFE4400 device async decoder
 * Protocentral Electronics Pvt Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "afe4400.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(AFE4400_DECODER, CONFIG_SENSOR_LOG_LEVEL);

#define DT_DRV_COMPAT ti_afe4400

static int afe4400_decoder_get_frame_count(const uint8_t *buffer, enum sensor_channel channel,
											size_t channel_idx, uint16_t *frame_count)
{
	ARG_UNUSED(buffer);
	ARG_UNUSED(channel);
	ARG_UNUSED(channel_idx);

	/* This sensor lacks a FIFO; there will always only be one frame at a time. */
	*frame_count = 1;
	return 0;
}

static int afe4400_decoder_get_size_info(enum sensor_channel channel, size_t *base_size,
										  size_t *frame_size)
{
	switch (channel)
	{
	case SENSOR_CHAN_MAGN_X:
	case SENSOR_CHAN_MAGN_Y:
	case SENSOR_CHAN_MAGN_Z:
	case SENSOR_CHAN_MAGN_XYZ:
		//*base_size = sizeof(struct sensor_three_axis_data);
		//*frame_size = sizeof(struct sensor_three_axis_sample_data);
		return 0;
	default:
		return -ENOTSUP;
	}
}

static int afe4400_decoder_decode(const uint8_t *buffer, enum sensor_channel channel,
								   size_t channel_idx, uint32_t *fit,
								   uint16_t max_count, void *data_out)
{
	//const struct afe4400_encoded_data *edata = (const struct afe4400_encoded_data *)buffer;
	
	return 0;
}

SENSOR_DECODER_API_DT_DEFINE() = {
	.get_frame_count = afe4400_decoder_get_frame_count,
	.get_size_info = afe4400_decoder_get_size_info,
	.decode = afe4400_decoder_decode,
};

int afe4400_get_decoder(const struct device *dev, const struct sensor_decoder_api **decoder)
{
	ARG_UNUSED(dev);
	*decoder = &SENSOR_DECODER_NAME();

	return 0;
}