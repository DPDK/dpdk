/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#include "axgbe_ethdev.h"
#include "axgbe_common.h"

#define AXGBE_ABORT_COUNT	500
#define AXGBE_DISABLE_COUNT	1000

#define AXGBE_STD_SPEED		1

#define AXGBE_INTR_RX_FULL	BIT(IC_RAW_INTR_STAT_RX_FULL_INDEX)
#define AXGBE_INTR_TX_EMPTY	BIT(IC_RAW_INTR_STAT_TX_EMPTY_INDEX)
#define AXGBE_INTR_TX_ABRT	BIT(IC_RAW_INTR_STAT_TX_ABRT_INDEX)
#define AXGBE_INTR_STOP_DET	BIT(IC_RAW_INTR_STAT_STOP_DET_INDEX)
#define AXGBE_DEFAULT_INT_MASK	(AXGBE_INTR_RX_FULL  |	\
				 AXGBE_INTR_TX_EMPTY |	\
				 AXGBE_INTR_TX_ABRT  |	\
				 AXGBE_INTR_STOP_DET)

#define AXGBE_I2C_READ		BIT(8)
#define AXGBE_I2C_STOP		BIT(9)

static int axgbe_i2c_abort(struct axgbe_port *pdata)
{
	unsigned int wait = AXGBE_ABORT_COUNT;

	/* Must be enabled to recognize the abort request */
	XI2C_IOWRITE_BITS(pdata, IC_ENABLE, EN, 1);

	/* Issue the abort */
	XI2C_IOWRITE_BITS(pdata, IC_ENABLE, ABORT, 1);

	while (wait--) {
		if (!XI2C_IOREAD_BITS(pdata, IC_ENABLE, ABORT))
			return 0;
		rte_delay_us(500);
	}

	return -EBUSY;
}

static int axgbe_i2c_set_enable(struct axgbe_port *pdata, bool enable)
{
	unsigned int wait = AXGBE_DISABLE_COUNT;
	unsigned int mode = enable ? 1 : 0;

	while (wait--) {
		XI2C_IOWRITE_BITS(pdata, IC_ENABLE, EN, mode);
		if (XI2C_IOREAD_BITS(pdata, IC_ENABLE_STATUS, EN) == mode)
			return 0;

		rte_delay_us(100);
	}

	return -EBUSY;
}

static int axgbe_i2c_disable(struct axgbe_port *pdata)
{
	unsigned int ret;

	ret = axgbe_i2c_set_enable(pdata, false);
	if (ret) {
		/* Disable failed, try an abort */
		ret = axgbe_i2c_abort(pdata);
		if (ret)
			return ret;

		/* Abort succeeded, try to disable again */
		ret = axgbe_i2c_set_enable(pdata, false);
	}

	return ret;
}

static int axgbe_i2c_enable(struct axgbe_port *pdata)
{
	return axgbe_i2c_set_enable(pdata, true);
}

static void axgbe_i2c_clear_all_interrupts(struct axgbe_port *pdata)
{
	XI2C_IOREAD(pdata, IC_CLR_INTR);
}

static void axgbe_i2c_disable_interrupts(struct axgbe_port *pdata)
{
	XI2C_IOWRITE(pdata, IC_INTR_MASK, 0);
}

static void axgbe_i2c_enable_interrupts(struct axgbe_port *pdata)
{
	XI2C_IOWRITE(pdata, IC_INTR_MASK, AXGBE_DEFAULT_INT_MASK);
}

static void axgbe_i2c_write(struct axgbe_port *pdata)
{
	struct axgbe_i2c_op_state *state = &pdata->i2c.op_state;
	unsigned int tx_slots;
	unsigned int cmd;

	/* Configured to never receive Rx overflows, so fill up Tx fifo */
	tx_slots = pdata->i2c.tx_fifo_size - XI2C_IOREAD(pdata, IC_TXFLR);
	while (tx_slots && state->tx_len) {
		if (state->op->cmd == AXGBE_I2C_CMD_READ)
			cmd = AXGBE_I2C_READ;
		else
			cmd = *state->tx_buf++;

		if (state->tx_len == 1)
			XI2C_SET_BITS(cmd, IC_DATA_CMD, STOP, 1);

		XI2C_IOWRITE(pdata, IC_DATA_CMD, cmd);

		tx_slots--;
		state->tx_len--;
	}

	/* No more Tx operations, so ignore TX_EMPTY and return */
	if (!state->tx_len)
		XI2C_IOWRITE_BITS(pdata, IC_INTR_MASK, TX_EMPTY, 0);
}

static void axgbe_i2c_read(struct axgbe_port *pdata)
{
	struct axgbe_i2c_op_state *state = &pdata->i2c.op_state;
	unsigned int rx_slots;

	/* Anything to be read? */
	if (state->op->cmd != AXGBE_I2C_CMD_READ)
		return;

	rx_slots = XI2C_IOREAD(pdata, IC_RXFLR);
	while (rx_slots && state->rx_len) {
		*state->rx_buf++ = XI2C_IOREAD(pdata, IC_DATA_CMD);
		state->rx_len--;
		rx_slots--;
	}
}

static void axgbe_i2c_clear_isr_interrupts(struct axgbe_port *pdata,
					  unsigned int isr)
{
	struct axgbe_i2c_op_state *state = &pdata->i2c.op_state;

	if (isr & AXGBE_INTR_TX_ABRT) {
		state->tx_abort_source = XI2C_IOREAD(pdata, IC_TX_ABRT_SOURCE);
		XI2C_IOREAD(pdata, IC_CLR_TX_ABRT);
	}

	if (isr & AXGBE_INTR_STOP_DET)
		XI2C_IOREAD(pdata, IC_CLR_STOP_DET);
}

static int axgbe_i2c_isr(struct axgbe_port *pdata)
{
	struct axgbe_i2c_op_state *state = &pdata->i2c.op_state;
	unsigned int isr;

	isr = XI2C_IOREAD(pdata, IC_RAW_INTR_STAT);

	axgbe_i2c_clear_isr_interrupts(pdata, isr);

	if (isr & AXGBE_INTR_TX_ABRT) {
		axgbe_i2c_disable_interrupts(pdata);

		state->ret = -EIO;
		goto out;
	}

	/* Check for data in the Rx fifo */
	axgbe_i2c_read(pdata);

	/* Fill up the Tx fifo next */
	axgbe_i2c_write(pdata);

out:
	/* Complete on an error or STOP condition */
	if (state->ret || XI2C_GET_BITS(isr, IC_RAW_INTR_STAT, STOP_DET))
		return 1;

	return 0;
}

static void axgbe_i2c_set_mode(struct axgbe_port *pdata)
{
	unsigned int reg;

	reg = XI2C_IOREAD(pdata, IC_CON);
	XI2C_SET_BITS(reg, IC_CON, MASTER_MODE, 1);
	XI2C_SET_BITS(reg, IC_CON, SLAVE_DISABLE, 1);
	XI2C_SET_BITS(reg, IC_CON, RESTART_EN, 1);
	XI2C_SET_BITS(reg, IC_CON, SPEED, AXGBE_STD_SPEED);
	XI2C_SET_BITS(reg, IC_CON, RX_FIFO_FULL_HOLD, 1);
	XI2C_IOWRITE(pdata, IC_CON, reg);
}

static void axgbe_i2c_get_features(struct axgbe_port *pdata)
{
	struct axgbe_i2c *i2c = &pdata->i2c;
	unsigned int reg;

	reg = XI2C_IOREAD(pdata, IC_COMP_PARAM_1);
	i2c->max_speed_mode = XI2C_GET_BITS(reg, IC_COMP_PARAM_1,
					    MAX_SPEED_MODE);
	i2c->rx_fifo_size = XI2C_GET_BITS(reg, IC_COMP_PARAM_1,
					  RX_BUFFER_DEPTH);
	i2c->tx_fifo_size = XI2C_GET_BITS(reg, IC_COMP_PARAM_1,
					  TX_BUFFER_DEPTH);
}

static void axgbe_i2c_set_target(struct axgbe_port *pdata, unsigned int addr)
{
	XI2C_IOWRITE(pdata, IC_TAR, addr);
}

static int axgbe_i2c_xfer(struct axgbe_port *pdata, struct axgbe_i2c_op *op)
{
	struct axgbe_i2c_op_state *state = &pdata->i2c.op_state;
	int ret;
	uint64_t timeout;

	pthread_mutex_lock(&pdata->i2c_mutex);
	ret = axgbe_i2c_disable(pdata);
	if (ret) {
		PMD_DRV_LOG(ERR, "failed to disable i2c master\n");
		return ret;
	}

	axgbe_i2c_set_target(pdata, op->target);

	memset(state, 0, sizeof(*state));
	state->op = op;
	state->tx_len = op->len;
	state->tx_buf = (unsigned char *)op->buf;
	state->rx_len = op->len;
	state->rx_buf = (unsigned char *)op->buf;

	axgbe_i2c_clear_all_interrupts(pdata);
	ret = axgbe_i2c_enable(pdata);
	if (ret) {
		PMD_DRV_LOG(ERR, "failed to enable i2c master\n");
		return ret;
	}

	/* Enabling the interrupts will cause the TX FIFO empty interrupt to
	 * fire and begin to process the command via the ISR.
	 */
	axgbe_i2c_enable_interrupts(pdata);
	timeout = rte_get_timer_cycles() + rte_get_timer_hz();

	while (time_before(rte_get_timer_cycles(), timeout)) {
		rte_delay_us(100);
		if (XI2C_IOREAD(pdata, IC_RAW_INTR_STAT)) {
			if (axgbe_i2c_isr(pdata))
				goto success;
		}
	}

	PMD_DRV_LOG(ERR, "i2c operation timed out\n");
	axgbe_i2c_disable_interrupts(pdata);
	axgbe_i2c_disable(pdata);
	ret = -ETIMEDOUT;
	goto unlock;

success:
	ret = state->ret;
	if (ret) {
		if (state->tx_abort_source & IC_TX_ABRT_7B_ADDR_NOACK)
			ret = -ENOTCONN;
		else if (state->tx_abort_source & IC_TX_ABRT_ARB_LOST)
			ret = -EAGAIN;
	}

unlock:
	pthread_mutex_unlock(&pdata->i2c_mutex);
	return ret;
}

static void axgbe_i2c_stop(struct axgbe_port *pdata)
{
	if (!pdata->i2c.started)
		return;

	pdata->i2c.started = 0;
	axgbe_i2c_disable_interrupts(pdata);
	axgbe_i2c_disable(pdata);
	axgbe_i2c_clear_all_interrupts(pdata);
}

static int axgbe_i2c_start(struct axgbe_port *pdata)
{
	if (pdata->i2c.started)
		return 0;

	pdata->i2c.started = 1;

	return 0;
}

static int axgbe_i2c_init(struct axgbe_port *pdata)
{
	int ret;

	axgbe_i2c_disable_interrupts(pdata);

	ret = axgbe_i2c_disable(pdata);
	if (ret) {
		PMD_DRV_LOG(ERR, "failed to disable i2c master\n");
		return ret;
	}

	axgbe_i2c_get_features(pdata);

	axgbe_i2c_set_mode(pdata);

	axgbe_i2c_clear_all_interrupts(pdata);

	return 0;
}

void axgbe_init_function_ptrs_i2c(struct axgbe_i2c_if *i2c_if)
{
	i2c_if->i2c_init		= axgbe_i2c_init;
	i2c_if->i2c_start		= axgbe_i2c_start;
	i2c_if->i2c_stop		= axgbe_i2c_stop;
	i2c_if->i2c_xfer		= axgbe_i2c_xfer;
}
