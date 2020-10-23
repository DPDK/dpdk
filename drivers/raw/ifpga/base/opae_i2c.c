
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#include "opae_osdep.h"
#include "opae_i2c.h"

static int i2c_transfer(struct altera_i2c_dev *dev,
		struct i2c_msg *msg, int num)
{
	int ret, try;

	for (ret = 0, try = 0; try < I2C_XFER_RETRY; try++) {
		ret = dev->xfer(dev, msg, num);
		if (ret != -EAGAIN)
			break;
	}

	return ret;
}

/**
 * i2c read function
 */
int i2c_read(struct altera_i2c_dev *dev, int flags, unsigned int slave_addr,
		u32 offset, u8 *buf, u32 count)
{
	u8 msgbuf[2];
	int i = 0;
	int ret;

	pthread_mutex_lock(dev->mutex);

	if (flags & I2C_FLAG_ADDR16)
		msgbuf[i++] = offset >> 8;

	msgbuf[i++] = offset;

	struct i2c_msg msg[2] = {
		{
			.addr = slave_addr,
			.flags = 0,
			.len = i,
			.buf = msgbuf,
		},
		{
			.addr = slave_addr,
			.flags = I2C_M_RD,
			.len = count,
			.buf = buf,
		},
	};

	if (!dev->xfer) {
		ret = -ENODEV;
		goto exit;
	}

	ret = i2c_transfer(dev, msg, 2);

exit:
	pthread_mutex_unlock(dev->mutex);
	return ret;
}

int i2c_write(struct altera_i2c_dev *dev, int flags, unsigned int slave_addr,
		u32 offset, u8 *buffer, int len)
{
	struct i2c_msg msg;
	u8 *buf;
	int ret;
	int i = 0;

	pthread_mutex_lock(dev->mutex);

	if (!dev->xfer) {
		ret = -ENODEV;
		goto exit;
	}

	buf = opae_malloc(I2C_MAX_OFFSET_LEN + len);
	if (!buf) {
		ret = -ENOMEM;
		goto exit;
	}

	msg.addr = slave_addr;
	msg.flags = 0;
	msg.buf = buf;

	if (flags & I2C_FLAG_ADDR16)
		msg.buf[i++] = offset >> 8;

	msg.buf[i++] = offset;
	opae_memcpy(&msg.buf[i], buffer, len);
	msg.len = i + len;

	ret = i2c_transfer(dev, &msg, 1);

	opae_free(buf);
exit:
	pthread_mutex_unlock(dev->mutex);
	return ret;
}

int i2c_read8(struct altera_i2c_dev *dev, unsigned int slave_addr, u32 offset,
		u8 *buf, u32 count)
{
	return i2c_read(dev, 0, slave_addr, offset, buf, count);
}

int i2c_read16(struct altera_i2c_dev *dev, unsigned int slave_addr, u32 offset,
		u8 *buf, u32 count)
{
	return i2c_read(dev, I2C_FLAG_ADDR16, slave_addr, offset,
			buf, count);
}

int i2c_write8(struct altera_i2c_dev *dev, unsigned int slave_addr, u32 offset,
		u8 *buf, u32 count)
{
	return i2c_write(dev, 0, slave_addr, offset, buf, count);
}

int i2c_write16(struct altera_i2c_dev *dev, unsigned int slave_addr, u32 offset,
		u8 *buf, u32 count)
{
	return i2c_write(dev, I2C_FLAG_ADDR16, slave_addr, offset,
			buf, count);
}

static void i2c_indirect_write(struct altera_i2c_dev *dev, u32 reg,
		u32 value)
{
	u64 ctrl;

	ctrl = I2C_CTRL_W | (reg >> 2);

	opae_writeq(value & I2C_WRITE_DATA_MASK, dev->base + I2C_WRITE);
	opae_writeq(ctrl, dev->base + I2C_CTRL);
}

static u32 i2c_indirect_read(struct altera_i2c_dev *dev, u32 reg)
{
	u64 tmp;
	u64 ctrl;
	u32 value;

	ctrl = I2C_CTRL_R | (reg >> 2);
	opae_writeq(ctrl, dev->base + I2C_CTRL);

	/* FIXME: Read one more time to avoid HW timing issue. */
	tmp = opae_readq(dev->base + I2C_READ);
	tmp = opae_readq(dev->base + I2C_READ);

	value = tmp & I2C_READ_DATA_MASK;

	return value;
}

static void altera_i2c_transfer(struct altera_i2c_dev *dev, u32 data)
{
	/*send STOP on last byte*/
	if (dev->msg_len == 1)
		data |= ALTERA_I2C_TFR_CMD_STO;
	if (dev->msg_len > 0)
		i2c_indirect_write(dev, ALTERA_I2C_TFR_CMD, data);
}

static void altera_i2c_disable(struct altera_i2c_dev *dev)
{
	u32 val = i2c_indirect_read(dev, ALTERA_I2C_CTRL);

	i2c_indirect_write(dev, ALTERA_I2C_CTRL, val&~ALTERA_I2C_CTRL_EN);
}

static void altera_i2c_enable(struct altera_i2c_dev *dev)
{
	u32 val = i2c_indirect_read(dev, ALTERA_I2C_CTRL);

	i2c_indirect_write(dev, ALTERA_I2C_CTRL, val | ALTERA_I2C_CTRL_EN);
}

static void altera_i2c_reset(struct altera_i2c_dev *dev)
{
	altera_i2c_disable(dev);
	altera_i2c_enable(dev);
}

static int altera_i2c_wait_core_idle(struct altera_i2c_dev *dev)
{
	int retry = 0;

	while (i2c_indirect_read(dev, ALTERA_I2C_STATUS)
			& ALTERA_I2C_STAT_CORE) {
		if (retry++ > ALTERA_I2C_TIMEOUT_US) {
			dev_err(dev, "timeout: Core Status not IDLE...\n");
			return -EBUSY;
		}
		udelay(1);
	}

	return 0;
}

static void altera_i2c_enable_interrupt(struct altera_i2c_dev *dev,
		u32 mask, bool enable)
{
	u32 status;

	status = i2c_indirect_read(dev, ALTERA_I2C_ISER);
	if (enable)
		dev->isr_mask = status | mask;
	else
		dev->isr_mask = status&~mask;

	i2c_indirect_write(dev, ALTERA_I2C_ISER, dev->isr_mask);
}

static void altera_i2c_interrupt_clear(struct altera_i2c_dev *dev, u32 mask)
{
	u32 int_en;

	int_en = i2c_indirect_read(dev, ALTERA_I2C_ISR);

	i2c_indirect_write(dev, ALTERA_I2C_ISR, int_en | mask);
}

static void altera_i2c_read_rx_fifo(struct altera_i2c_dev *dev)
{
	size_t rx_avail;
	size_t bytes;

	rx_avail = i2c_indirect_read(dev, ALTERA_I2C_RX_FIFO_LVL);
	bytes = min(rx_avail, dev->msg_len);

	while (bytes-- > 0) {
		*dev->buf++ = i2c_indirect_read(dev, ALTERA_I2C_RX_DATA);
		dev->msg_len--;
		altera_i2c_transfer(dev, 0);
	}
}

static void altera_i2c_stop(struct altera_i2c_dev *dev)
{
	i2c_indirect_write(dev, ALTERA_I2C_TFR_CMD, ALTERA_I2C_TFR_CMD_STO);
}

static int altera_i2c_fill_tx_fifo(struct altera_i2c_dev *dev)
{
	size_t tx_avail;
	int bytes;
	int ret;

	tx_avail = dev->fifo_size -
		i2c_indirect_read(dev, ALTERA_I2C_TC_FIFO_LVL);
	bytes = min(tx_avail, dev->msg_len);
	ret = dev->msg_len - bytes;

	while (bytes-- > 0) {
		altera_i2c_transfer(dev, *dev->buf++);
		dev->msg_len--;
	}

	return ret;
}

static u8 i2c_8bit_addr_from_msg(const struct i2c_msg *msg)
{
	return (msg->addr << 1) | (msg->flags & I2C_M_RD ? 1 : 0);
}

static int altera_i2c_wait_complete(struct altera_i2c_dev *dev,
		u32 *status)
{
	int retry = 0;

	while (!((*status = i2c_indirect_read(dev, ALTERA_I2C_ISR))
				& dev->isr_mask)) {
		if (retry++ > ALTERA_I2C_TIMEOUT_US)
			return -EBUSY;

		udelay(1000);
	}

	return 0;
}

static bool altera_handle_i2c_status(struct altera_i2c_dev *dev, u32 status)
{
	bool read, finish = false;
	int ret;

	read = (dev->msg->flags & I2C_M_RD) != 0;

	if (status & ALTERA_I2C_ISR_ARB) {
		altera_i2c_interrupt_clear(dev, ALTERA_I2C_ISR_ARB);
		dev->msg_err = -EAGAIN;
		finish = true;
	} else if (status & ALTERA_I2C_ISR_NACK) {
		dev_debug(dev, "could not get ACK\n");
		dev->msg_err = -ENXIO;
		altera_i2c_interrupt_clear(dev, ALTERA_I2C_ISR_NACK);
		altera_i2c_stop(dev);
		finish = true;
	} else if (read && (status & ALTERA_I2C_ISR_RXOF)) {
		/* RX FIFO Overflow */
		altera_i2c_read_rx_fifo(dev);
		altera_i2c_interrupt_clear(dev, ALTERA_I2C_ISER_RXOF_EN);
		altera_i2c_stop(dev);
		dev_err(dev, "error: RX FIFO overflow\n");
		finish = true;
	} else if (read && (status & ALTERA_I2C_ISR_RXRDY)) {
		altera_i2c_read_rx_fifo(dev);
		altera_i2c_interrupt_clear(dev, ALTERA_I2C_ISR_RXRDY);
		if (!dev->msg_len)
			finish = true;
	} else if (!read && (status & ALTERA_I2C_ISR_TXRDY)) {
		altera_i2c_interrupt_clear(dev, ALTERA_I2C_ISR_TXRDY);
		if (dev->msg_len > 0)
			altera_i2c_fill_tx_fifo(dev);
		else
			finish = true;
	} else {
		dev_err(dev, "unexpected status:0x%x\n", status);
		altera_i2c_interrupt_clear(dev, ALTERA_I2C_ALL_IRQ);
	}

	if (finish) {
		ret = altera_i2c_wait_core_idle(dev);
		if (ret)
			dev_err(dev, "message timeout\n");

		altera_i2c_enable_interrupt(dev, ALTERA_I2C_ALL_IRQ, false);
		altera_i2c_interrupt_clear(dev, ALTERA_I2C_ALL_IRQ);
		dev_debug(dev, "message done\n");
	}

	return finish;
}

static bool altera_i2c_poll_status(struct altera_i2c_dev *dev)
{
	u32 status;
	bool finish = false;
	int i = 0;

	do {
		if (altera_i2c_wait_complete(dev, &status)) {
			dev_err(dev, "altera i2c wait complete timeout, status=0x%x\n",
					status);
			return -EBUSY;
		}

		finish = altera_handle_i2c_status(dev, status);

		if (i++ > I2C_XFER_RETRY)
			break;

	} while (!finish);

	return finish;
}

static int altera_i2c_xfer_msg(struct altera_i2c_dev *dev,
		struct i2c_msg *msg)
{
	u32 int_mask = ALTERA_I2C_ISR_RXOF |
		ALTERA_I2C_ISR_ARB | ALTERA_I2C_ISR_NACK;
	u8 addr = i2c_8bit_addr_from_msg(msg);
	bool finish;

	dev->msg = msg;
	dev->msg_len = msg->len;
	dev->buf = msg->buf;
	dev->msg_err = 0;
	altera_i2c_enable(dev);

	/*make sure RX FIFO is emtry*/
	do {
		i2c_indirect_read(dev, ALTERA_I2C_RX_DATA);
	} while (i2c_indirect_read(dev, ALTERA_I2C_RX_FIFO_LVL));

	i2c_indirect_write(dev, ALTERA_I2C_TFR_CMD_RW_D,
			ALTERA_I2C_TFR_CMD_STA | addr);

	/*enable irq*/
	if (msg->flags & I2C_M_RD) {
		int_mask |= ALTERA_I2C_ISR_RXOF | ALTERA_I2C_ISR_RXRDY;
		/* in polling mode, we should set this ISR register? */
		altera_i2c_enable_interrupt(dev, int_mask, true);
		altera_i2c_transfer(dev, 0);
	} else {
		int_mask |= ALTERA_I2C_ISR_TXRDY;
		altera_i2c_enable_interrupt(dev, int_mask, true);
		altera_i2c_fill_tx_fifo(dev);
	}

	finish = altera_i2c_poll_status(dev);
	if (!finish) {
		dev->msg_err = -ETIMEDOUT;
		dev_err(dev, "%s: i2c transfer error\n", __func__);
	}

	altera_i2c_enable_interrupt(dev, int_mask, false);

	if (i2c_indirect_read(dev, ALTERA_I2C_STATUS) & ALTERA_I2C_STAT_CORE)
		dev_info(dev, "core not idle...\n");

	altera_i2c_disable(dev);

	return dev->msg_err;
}

static int altera_i2c_xfer(struct altera_i2c_dev *dev,
		struct i2c_msg *msg, int num)
{
	int ret = 0;
	int i;

	for (i = 0; i < num; i++, msg++) {
		ret = altera_i2c_xfer_msg(dev, msg);
		if (ret)
			break;
	}

	return ret;
}

static void altera_i2c_hardware_init(struct altera_i2c_dev *dev)
{
	u32 divisor = dev->i2c_clk / dev->bus_clk_rate;
	u32 clk_mhz = dev->i2c_clk / 1000000;
	u32 tmp = (ALTERA_I2C_THRESHOLD << ALTERA_I2C_CTRL_RXT_SHFT) |
		  (ALTERA_I2C_THRESHOLD << ALTERA_I2C_CTRL_TCT_SHFT);
	u32 t_high, t_low;

	if (dev->bus_clk_rate <= 100000) {
		tmp &= ~ALTERA_I2C_CTRL_BSPEED;
		/*standard mode SCL 50/50*/
		t_high = divisor*1/2;
		t_low = divisor*1/2;
	} else {
		tmp |= ALTERA_I2C_CTRL_BSPEED;
		/*Fast mode SCL 33/66*/
		t_high = divisor*1/3;
		t_low = divisor*2/3;
	}

	i2c_indirect_write(dev, ALTERA_I2C_CTRL, tmp);

	dev_info(dev, "%s: rate=%uHz per_clk=%uMHz -> ratio=1:%u\n",
		__func__, dev->bus_clk_rate, clk_mhz, divisor);

	/*reset the i2c*/
	altera_i2c_reset(dev);

	/*Set SCL high Time*/
	i2c_indirect_write(dev, ALTERA_I2C_SCL_HIGH, t_high);
	/*Set SCL low time*/
	i2c_indirect_write(dev, ALTERA_I2C_SCL_LOW, t_low);
	/*Set SDA Hold time, 300ms*/
	i2c_indirect_write(dev, ALTERA_I2C_SDA_HOLD, (300*clk_mhz)/1000);

	altera_i2c_enable_interrupt(dev, ALTERA_I2C_ALL_IRQ, false);
}

struct altera_i2c_dev *altera_i2c_probe(void *base)
{
	struct altera_i2c_dev *dev;

	dev = opae_malloc(sizeof(*dev));
	if (!dev)
		return NULL;

	dev->base = (u8 *)base;
	dev->i2c_param.info = opae_readq(dev->base + I2C_PARAM);

	if (dev->i2c_param.devid != 0xEE011) {
		dev_err(dev, "find a invalid i2c master\n");
		return NULL;
	}

	dev->fifo_size = dev->i2c_param.fifo_depth;

	if (dev->i2c_param.max_req == ALTERA_I2C_100KHZ)
		dev->bus_clk_rate = 100000;
	else if (dev->i2c_param.max_req == ALTERA_I2C_400KHZ)
		/* i2c bus clk 400KHz*/
		dev->bus_clk_rate = 400000;

	/* i2c input clock for vista creek is 100MHz */
	dev->i2c_clk = dev->i2c_param.ref_clk * 1000000;
	dev->xfer = altera_i2c_xfer;

	if (pthread_mutex_init(&dev->lock, NULL))
		return NULL;
	dev->mutex = &dev->lock;

	altera_i2c_hardware_init(dev);

	return dev;
}

void altera_i2c_remove(struct altera_i2c_dev *dev)
{
	if (dev) {
		pthread_mutex_destroy(&dev->lock);
		altera_i2c_disable(dev);
		opae_free(dev);
	}
}
