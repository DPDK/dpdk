/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#include "axgbe_ethdev.h"
#include "axgbe_common.h"
#include "axgbe_phy.h"

static int __axgbe_exit(struct axgbe_port *pdata)
{
	unsigned int count = 2000;

	/* Issue a software reset */
	AXGMAC_IOWRITE_BITS(pdata, DMA_MR, SWR, 1);
	rte_delay_us(10);

	/* Poll Until Poll Condition */
	while (--count && AXGMAC_IOREAD_BITS(pdata, DMA_MR, SWR))
		rte_delay_us(500);

	if (!count)
		return -EBUSY;

	return 0;
}

static int axgbe_exit(struct axgbe_port *pdata)
{
	int ret;

	/* To guard against possible incorrectly generated interrupts,
	 * issue the software reset twice.
	 */
	ret = __axgbe_exit(pdata);
	if (ret)
		return ret;

	return __axgbe_exit(pdata);
}

void axgbe_init_function_ptrs_dev(struct axgbe_hw_if *hw_if)
{
	hw_if->exit = axgbe_exit;
}
