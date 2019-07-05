/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _OPAE_I2C_H
#define _OPAE_I2C_H

#include "opae_osdep.h"

#define ALTERA_I2C_TFR_CMD	0x00	/* Transfer Command register */
#define ALTERA_I2C_TFR_CMD_STA	BIT(9)	/* send START before byte */
#define ALTERA_I2C_TFR_CMD_STO	BIT(8)	/* send STOP after byte */
#define ALTERA_I2C_TFR_CMD_RW_D	BIT(0)	/* Direction of transfer */
#define ALTERA_I2C_RX_DATA	0x04	/* RX data FIFO register */
#define ALTERA_I2C_CTRL		0x8	/* Control register */
#define ALTERA_I2C_CTRL_RXT_SHFT	4	/* RX FIFO Threshold */
#define ALTERA_I2C_CTRL_TCT_SHFT	2	/* TFER CMD FIFO Threshold */
#define ALTERA_I2C_CTRL_BSPEED	BIT(1)	/* Bus Speed */
#define ALTERA_I2C_CTRL_EN	BIT(0)	/* Enable Core */
#define ALTERA_I2C_ISER		0xc	/* Interrupt Status Enable register */
#define ALTERA_I2C_ISER_RXOF_EN	BIT(4)	/* Enable RX OVERFLOW IRQ */
#define ALTERA_I2C_ISER_ARB_EN	BIT(3)	/* Enable ARB LOST IRQ */
#define ALTERA_I2C_ISER_NACK_EN	BIT(2)	/* Enable NACK DET IRQ */
#define ALTERA_I2C_ISER_RXRDY_EN	BIT(1)	/* Enable RX Ready IRQ */
#define ALTERA_I2C_ISER_TXRDY_EN	BIT(0)	/* Enable TX Ready IRQ */
#define ALTERA_I2C_ISR		0x10	/* Interrupt Status register */
#define ALTERA_I2C_ISR_RXOF		BIT(4)	/* RX OVERFLOW */
#define ALTERA_I2C_ISR_ARB		BIT(3)	/* ARB LOST */
#define ALTERA_I2C_ISR_NACK		BIT(2)	/* NACK DET */
#define ALTERA_I2C_ISR_RXRDY		BIT(1)	/* RX Ready */
#define ALTERA_I2C_ISR_TXRDY		BIT(0)	/* TX Ready */
#define ALTERA_I2C_STATUS	0x14	/* Status register */
#define ALTERA_I2C_STAT_CORE		BIT(0)	/* Core Status */
#define ALTERA_I2C_TC_FIFO_LVL	0x18   /* Transfer FIFO LVL register */
#define ALTERA_I2C_RX_FIFO_LVL	0x1c	/* Receive FIFO LVL register */
#define ALTERA_I2C_SCL_LOW	0x20	/* SCL low count register */
#define ALTERA_I2C_SCL_HIGH	0x24	/* SCL high count register */
#define ALTERA_I2C_SDA_HOLD	0x28	/* SDA hold count register */

#define ALTERA_I2C_ALL_IRQ	(ALTERA_I2C_ISR_RXOF | ALTERA_I2C_ISR_ARB | \
				 ALTERA_I2C_ISR_NACK | ALTERA_I2C_ISR_RXRDY | \
				 ALTERA_I2C_ISR_TXRDY)

#define ALTERA_I2C_THRESHOLD	0
#define ALTERA_I2C_DFLT_FIFO_SZ	8
#define ALTERA_I2C_TIMEOUT_US  250000 /* 250ms */

#define I2C_PARAM 0x8
#define I2C_CTRL  0x10
#define I2C_CTRL_R    BIT_ULL(9)
#define I2C_CTRL_W    BIT_ULL(8)
#define I2C_CTRL_ADDR_MASK GENMASK_ULL(3, 0)
#define I2C_READ 0x18
#define I2C_READ_DATA_VALID BIT_ULL(32)
#define I2C_READ_DATA_MASK GENMASK_ULL(31, 0)
#define I2C_WRITE 0x20
#define I2C_WRITE_DATA_MASK GENMASK_ULL(31, 0)

#define ALTERA_I2C_100KHZ  0
#define ALTERA_I2C_400KHZ  1

/* i2c slave using 16bit address */
#define I2C_FLAG_ADDR16  1

#define I2C_XFER_RETRY 10

struct i2c_core_param {
	union {
		u64 info;
		struct {
			u16 fifo_depth:9;
			u8 interface:1;
			/*reference clock of I2C core in MHz*/
			u32 ref_clk:10;
			/*Max I2C interface freq*/
			u8 max_req:4;
			u64 devid:32;
			/* number of MAC address*/
			u8 nu_macs:8;
		};
	};
};

struct altera_i2c_dev {
	u8 *base;
	struct i2c_core_param i2c_param;
	u32 fifo_size;
	u32 bus_clk_rate; /* i2c bus clock */
	u32 i2c_clk; /* i2c input clock */
	struct i2c_msg *msg;
	size_t msg_len;
	int msg_err;
	u32 isr_mask;
	u8 *buf;
	int (*xfer)(struct altera_i2c_dev *dev, struct i2c_msg *msg, int num);
};

/**
 * struct i2c_msg: an I2C message
 */
struct i2c_msg {
	unsigned int addr;
	unsigned int flags;
	unsigned int len;
	u8 *buf;
};

#define I2C_MAX_OFFSET_LEN 4

enum i2c_msg_flags {
	I2C_M_TEN = 0x0010, /*ten-bit chip address*/
	I2C_M_RD  = 0x0001, /*read data*/
	I2C_M_STOP = 0x8000, /*send stop after this message*/
};

struct altera_i2c_dev *altera_i2c_probe(void *base);
int altera_i2c_remove(struct altera_i2c_dev *dev);
int i2c_read(struct altera_i2c_dev *dev, int flags, unsigned int slave_addr,
		u32 offset, u8 *buf, u32 count);
int i2c_write(struct altera_i2c_dev *dev, int flags, unsigned int slave_addr,
		u32 offset, u8 *buffer, int len);
int i2c_read8(struct altera_i2c_dev *dev, unsigned int slave_addr, u32 offset,
		u8 *buf, u32 count);
int i2c_read16(struct altera_i2c_dev *dev, unsigned int slave_addr, u32 offset,
		u8 *buf, u32 count);
int i2c_write8(struct altera_i2c_dev *dev, unsigned int slave_addr, u32 offset,
		u8 *buf, u32 count);
int i2c_write16(struct altera_i2c_dev *dev, unsigned int slave_addr, u32 offset,
		u8 *buf, u32 count);
#endif
