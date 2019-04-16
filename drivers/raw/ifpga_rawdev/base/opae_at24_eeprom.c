/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#include "opae_osdep.h"
#include "opae_i2c.h"
#include "opae_at24_eeprom.h"

#define AT24_READ_RETRY 10

static int at24_eeprom_read_and_try(struct altera_i2c_dev *dev,
		unsigned int slave_addr,
		u32 offset, u8 *buf, u32 len)
{
	int i;
	int ret = 0;

	for (i = 0; i < AT24_READ_RETRY; i++) {
		ret = i2c_read16(dev, slave_addr, offset,
				buf, len);
		if (ret == 0)
			break;

		opae_udelay(100);
	}

	return ret;
}

int at24_eeprom_read(struct altera_i2c_dev *dev, unsigned int slave_addr,
		u32 offset, u8 *buf, int count)
{
	int len;
	int status;
	int read_count = 0;

	if (!count)
		return count;

	if (count > AT24C512_IO_LIMIT)
		len = AT24C512_IO_LIMIT;
	else
		len = count;

	while (count) {
		status = at24_eeprom_read_and_try(dev, slave_addr, offset,
				buf, len);
		if (status)
			break;

		buf += len;
		offset += len;
		count -= len;
		read_count += len;
	}

	return read_count;
}

int at24_eeprom_write(struct altera_i2c_dev *dev, unsigned int slave_addr,
		u32 offset, u8 *buf, int count)
{
	int len;
	int status;
	int write_count = 0;

	if (!count)
		return count;

	if (count > AT24C512_PAGE_SIZE)
		len = AT24C512_PAGE_SIZE;
	else
		len = count;

	while (count) {
		status = i2c_write16(dev, slave_addr, offset, buf, len);
		if (status)
			break;

		buf += len;
		offset += len;
		count -= len;
		write_count += len;
	}

	return write_count;
}

