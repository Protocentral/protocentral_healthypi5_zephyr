#include <zephyr/drivers/sensor.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MAX30001_ASYNC, CONFIG_SENSOR_LOG_LEVEL);

#include "max30001.h"

static int max30001_async_sample_fetch(const struct device *dev,
                                       uint32_t *num_samples_ecg, uint32_t *num_samples_bioz, int32_t ecg_samples[32],
                                       int32_t bioz_samples[32], uint16_t *rri, uint16_t *hr,
                                       uint8_t *ecg_lead_off, uint8_t *bioz_lead_off)
{
    struct max30001_data *data = dev->data;
    const struct max30001_config *config = dev->config;

    uint32_t max30001_status;
    uint32_t e_fifo_num_bytes, b_fifo_num_bytes;

    uint32_t e_fifo_num_samples = 0;
    uint32_t b_fifo_num_samples = 0;

    // Buffer sized for actual max reads: 8 ECG samples × 3 bytes = 24 bytes
    // and 4 BioZ samples × 3 bytes = 12 bytes. Use 32 bytes each for alignment.
    uint8_t buf_ecg[32];
    uint8_t buf_bioz[16];

    uint8_t cmd_tx_ecg_fifo_burst = ((ECG_FIFO_BURST << 1) | RREG);
    const struct spi_buf tx_buf_ecg[1] = {{.buf = &cmd_tx_ecg_fifo_burst, .len = 1}};
    const struct spi_buf_set tx_ecg = {.buffers = tx_buf_ecg, .count = 1};

    uint8_t cmd_tx_bioz_fifo_burst = ((BIOZ_FIFO_BURST << 1) | RREG);
    const struct spi_buf tx_buf_bioz[1] = {{.buf = &cmd_tx_bioz_fifo_burst, .len = 1}};
    const struct spi_buf_set tx_bioz = {.buffers = tx_buf_bioz, .count = 1};

    uint32_t max30001_rtor = 0;

    max30001_status = max30001_read_status(dev);

    if ((max30001_status & MAX30001_STATUS_MASK_DCLOFF) == MAX30001_STATUS_MASK_DCLOFF)
    {
        data->ecg_lead_off = 1;
        *ecg_lead_off = 1;
    }
    else
    {
        data->ecg_lead_off = 0;
        *ecg_lead_off = 0;
    }

    // BioZ lead-off detection
    if ((max30001_status & BIOZ_LEAD_MASK) != 0)
    {
        data->bioz_lead_off = 1;
        *bioz_lead_off = 1;
    }
    else
    {
        data->bioz_lead_off = 0;
        *bioz_lead_off = 0;
    }

    // Initialize sample counts to 0 (no data available)
    *num_samples_ecg = 0;
    *num_samples_bioz = 0;

    // Handle ECG FIFO when EINT is set (ECG data ready)
    if ((max30001_status & MAX30001_STATUS_MASK_EINT) == MAX30001_STATUS_MASK_EINT)
    {
        // Read up to 8 ECG samples to provide headroom for system delays
        // ETAG validation determines actual valid sample count
        e_fifo_num_samples = 8;
        e_fifo_num_bytes = e_fifo_num_samples * 3;

        struct spi_buf rx_ecg_buf[2] = {{.buf = NULL, .len = 1}, {.buf = &buf_ecg, .len = e_fifo_num_bytes}};
        const struct spi_buf_set rx_ecg = {.buffers = rx_ecg_buf, .count = 2};
        spi_transceive_dt(&config->spi, &tx_ecg, &rx_ecg);

        // Read up to 4 BioZ samples (BioZ runs at half the ECG rate)
        b_fifo_num_samples = 4;
        b_fifo_num_bytes = b_fifo_num_samples * 3;

        struct spi_buf rx_bioz_buf[2] = {{.buf = NULL, .len = 1}, {.buf = &buf_bioz, .len = b_fifo_num_bytes}};
        const struct spi_buf_set rx_bioz = {.buffers = rx_bioz_buf, .count = 2};
        spi_transceive_dt(&config->spi, &tx_bioz, &rx_bioz);

        // Process ECG samples - validate each sample by checking ETAG
        uint32_t valid_ecg_samples = 0;
        for (int i = 0; i < e_fifo_num_samples; i++)
        {
            uint32_t etag = ((((uint8_t)buf_ecg[i * 3 + 2]) & 0x38) >> 3);

            if ((etag == 0x00) || (etag == 0x01) || (etag == 0x02)) // Valid sample (0x01 = fast recovery)
            {
                uint32_t uecgtemp = (uint32_t)(((uint32_t)buf_ecg[i * 3] << 16 | (uint32_t)buf_ecg[i * 3 + 1] << 8) | (uint32_t)(buf_ecg[i * 3 + 2] & 0xC0));
                uecgtemp = (uint32_t)(uecgtemp << 8);

                int32_t secgtemp = (int32_t)uecgtemp;
                secgtemp = (int32_t)secgtemp >> 6;

                ecg_samples[valid_ecg_samples] = (int32_t)(secgtemp);
                valid_ecg_samples++;
            }
            else if (etag == 0x06) // FIFO empty
            {
                break;
            }
            else if (etag == 0x07) // FIFO Overflow
            {
                max30001_fifo_reset(dev);
                break;
            }
            else
            {
                break;  // Invalid ETAG - stop reading
            }
        }
        *num_samples_ecg = valid_ecg_samples;

        // Process BioZ samples - validate each sample by checking BTAG
        uint32_t valid_bioz_samples = 0;
        for (int i = 0; i < b_fifo_num_samples; i++)
        {
            uint32_t btag = ((((uint8_t)buf_bioz[i * 3 + 2]) & 0x07));

            if ((btag == 0x00) || (btag == 0x02)) // Valid sample
            {
                uint32_t word = ((uint32_t)buf_bioz[i * 3] << 16) | ((uint32_t)buf_bioz[i * 3 + 1] << 8) | (uint32_t)buf_bioz[i * 3 + 2];
                word &= 0xFFFFF0u;
                int32_t s_bioz_temp = (int32_t)(word << 8);
                s_bioz_temp = (int32_t)(s_bioz_temp >> 12);
                bioz_samples[valid_bioz_samples] = s_bioz_temp;
                valid_bioz_samples++;
            }
            else if (btag == 0x06) // FIFO empty
            {
                break;
            }
            else if (btag == 0x07) // FIFO Overflow
            {
                max30001_fifo_reset(dev);
                break;
            }
            else
            {
                break;  // Invalid BTAG - stop reading
            }
        }
        *num_samples_bioz = valid_bioz_samples;
    }
    // Handle BioZ-only FIFO when BINT is set but EINT is not
    else if ((max30001_status & MAX30001_STATUS_MASK_BINT) == MAX30001_STATUS_MASK_BINT)
    {
        // Read up to 4 BioZ samples
        b_fifo_num_samples = 4;
        b_fifo_num_bytes = b_fifo_num_samples * 3;

        struct spi_buf rx_bioz_buf[2] = {{.buf = NULL, .len = 1}, {.buf = &buf_bioz, .len = b_fifo_num_bytes}};
        const struct spi_buf_set rx_bioz = {.buffers = rx_bioz_buf, .count = 2};
        spi_transceive_dt(&config->spi, &tx_bioz, &rx_bioz);

        // Process BioZ samples
        uint32_t valid_bioz_samples = 0;
        for (int i = 0; i < b_fifo_num_samples; i++)
        {
            uint32_t btag = ((((uint8_t)buf_bioz[i * 3 + 2]) & 0x07));

            if ((btag == 0x00) || (btag == 0x02)) // Valid sample
            {
                uint32_t word = ((uint32_t)buf_bioz[i * 3] << 16) | ((uint32_t)buf_bioz[i * 3 + 1] << 8) | (uint32_t)buf_bioz[i * 3 + 2];
                word &= 0xFFFFF0u;
                int32_t s_bioz_temp = (int32_t)(word << 8);
                s_bioz_temp = (int32_t)(s_bioz_temp >> 12);
                bioz_samples[valid_bioz_samples] = s_bioz_temp;
                valid_bioz_samples++;
            }
            else if (btag == 0x06) // FIFO empty
            {
                break;
            }
            else if (btag == 0x07) // FIFO Overflow
            {
                max30001_fifo_reset(dev);
                break;
            }
        }
        *num_samples_bioz = valid_bioz_samples;
    }
    // No data available (neither EINT nor BINT set)

    // Check for FIFO overflow conditions and reset if needed
    if (((max30001_status & MAX30001_STATUS_MASK_EOVF) == MAX30001_STATUS_MASK_EOVF) ||
        ((max30001_status & MAX30001_STATUS_MASK_BOVF) == MAX30001_STATUS_MASK_BOVF))
    {
        static uint32_t overflow_count = 0;
        overflow_count++;
        if (overflow_count <= 3 || (overflow_count % 100) == 0) {
            LOG_WRN("FIFO overflow #%u", overflow_count);
        }
        max30001_fifo_reset(dev);
    }

    // Read R-to-R if available
    if ((max30001_status & MAX30001_STATUS_MASK_RRINT) == MAX30001_STATUS_MASK_RRINT)
    {
        max30001_rtor = max30001_read_reg(dev, RTOR);
        if (max30001_rtor > 0)
        {
            data->lastRRI = (uint16_t)((max30001_rtor >> 10) * 7.8125);
            data->lastHR = (uint16_t)(60 * 1000 / data->lastRRI);
        }
    }

    // Always output the last known good HR and RRI values
    *hr = data->lastHR;
    *rri = data->lastRRI;

    return 0;
}

int max30001_submit(const struct device *dev, struct rtio_iodev_sqe *iodev_sqe)
{
    uint32_t m_min_buf_len = sizeof(struct max30001_encoded_data);

    uint8_t *buf;
    uint32_t buf_len;

    struct max30001_encoded_data *m_edata;

    int ret = 0;

    ret = rtio_sqe_rx_buf(iodev_sqe, m_min_buf_len, m_min_buf_len, &buf, &buf_len);
    if (ret != 0)
    {
        LOG_ERR("Failed to get a read buffer of size %u bytes", m_min_buf_len);
        rtio_iodev_sqe_err(iodev_sqe, ret);
        return ret;
    }

    m_edata = (struct max30001_encoded_data *)buf;
    m_edata->header.timestamp = k_ticks_to_ns_floor64(k_uptime_ticks());
    ret = max30001_async_sample_fetch(dev, &m_edata->num_samples_ecg, &m_edata->num_samples_bioz,
                                      m_edata->ecg_samples, m_edata->bioz_samples, &m_edata->rri, &m_edata->hr, &m_edata->ecg_lead_off, &m_edata->bioz_lead_off);

    if (ret != 0)
    {
        rtio_iodev_sqe_err(iodev_sqe, ret);
        return ret;
    }

    rtio_iodev_sqe_ok(iodev_sqe, 0);

    return 0;
}