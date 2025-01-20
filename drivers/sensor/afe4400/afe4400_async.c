#define DT_DRV_COMPAT ti_afe4400

#include <zephyr/drivers/sensor.h>

#include "afe4400.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(SENSOR_AFE4400_ASYNC, CONFIG_SENSOR_LOG_LEVEL);

#define AFE4400_READ_BLOCK_SIZE 1

static int afe4400_async_sample_fetch(const struct device *dev, int32_t *raw_ir_sample, int32_t *raw_red_sample, uint8_t *ppg_lead_off)
{
    // struct afe4400_data *drv_data = dev->data;

    //for (int i = 0; i < AFE4400_READ_BLOCK_SIZE; i++)
//{
    _afe4400_reg_write(dev, CONTROL0, 0x000004); //DIAG_EN enabled
    k_sleep(K_MSEC(16)); //wait for 16msec because of it takes 16msec to complete diagnostics

    _afe4400_reg_write(dev, CONTROL0, 0x000001); //SPI_READ enabled


    while ((_afe4400_read_reg(dev, CONTROL0) & DIAGNOSTIC_MASK_BIT) != DIAGNOSTIC_MASK_BIT)
    {
        _afe4400_reg_write(dev, CONTROL0, 0x000001);
    }

    _afe4400_reg_write(dev, CONTROL0, 0x000001);
    uint32_t diagnostic = _afe4400_read_reg(dev, DIAG);
    
    printk("diagnostic %d\n",diagnostic);

    //_afe4400_reg_write(dev, CONTROL0, 0x000001);
    //printk("Alaram %d\n",_afe4400_read_reg(dev, ALARM));



    if (diagnostic > 0 & diagnostic < 8191)
    {
        //printk("PPG Lead off\n");
        *raw_ir_sample = 0;
        *raw_red_sample = 0;
        *ppg_lead_off = 1;
    }
    else
    {
        //printk("PPG Lead on\n");
        _afe4400_reg_write(dev, CONTROL0, 0x000001);
        uint32_t led1val = _afe4400_read_reg(dev, LED1VAL);
        led1val = (uint32_t)(led1val << 10);
        int32_t led1val_signed = (int32_t)led1val;
        *raw_ir_sample = (int32_t)led1val_signed >> 18;

        _afe4400_reg_write(dev, CONTROL0, 0x000001);
        uint32_t led2val = _afe4400_read_reg(dev, LED2VAL);
        led2val = (uint32_t)(led2val << 10);
        int32_t led2val_signed = (int32_t)led2val;
        *raw_red_sample = (int32_t)led2val_signed >> 18;

        *ppg_lead_off = 0;
    }

        //if(AFE4400_READ_BLOCK_SIZE>1)
        //k_sleep(K_MSEC(7));

        // printk("IR: %d, RED: %d\n", raw_ir_sample[i], raw_red_sample[i]);
    //}

    //*num_samples = AFE4400_READ_BLOCK_SIZE;

    return 0;
}

int afe4400_submit(const struct device *dev, struct rtio_iodev_sqe *iodev_sqe)
{
    uint32_t m_min_buf_len = sizeof(struct afe4400_encoded_data);

    uint8_t *buf;
    uint32_t buf_len;

    struct afe4400_encoded_data *m_edata;

    int ret = 0;

    ret = rtio_sqe_rx_buf(iodev_sqe, m_min_buf_len, m_min_buf_len, &buf, &buf_len);
    if (ret != 0)
    {
        LOG_ERR("Failed to get a read buffer of size %u bytes", m_min_buf_len);
        rtio_iodev_sqe_err(iodev_sqe, ret);
        return ret;
    }

    m_edata = (struct afe4400_encoded_data *)buf;
    m_edata->header.timestamp = k_ticks_to_ns_floor64(k_uptime_ticks());
    ret = afe4400_async_sample_fetch(dev, &m_edata->raw_sample_ir, &m_edata->raw_sample_red,&m_edata->ppg_lead_off);

    if (ret != 0)
    {
        rtio_iodev_sqe_err(iodev_sqe, ret);
        return ret;
    }

    rtio_iodev_sqe_ok(iodev_sqe, 0);

    return 0;
}