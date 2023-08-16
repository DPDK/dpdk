/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_cycles.h>

#include "test.h"

/*
 * rte_delay_us_callback test
 *
 * - check if callback is correctly registered/unregistered
 *
 */

static unsigned int pattern;
static void my_rte_delay_us(unsigned int us)
{
	pattern += us;
}

static int
test_user_delay_us(void)
{
	pattern = 0;

	rte_delay_us(2);
	if (pattern != 0)
		return -1;

	/* register custom delay function */
	rte_delay_us_callback_register(my_rte_delay_us);

	rte_delay_us(2);
	if (pattern != 2)
		return -1;

	rte_delay_us(3);
	if (pattern != 5)
		return -1;

	/* restore original delay function */
	rte_delay_us_callback_register(rte_delay_us_block);

	rte_delay_us(3);
	if (pattern != 5)
		return -1;

	return 0;
}

REGISTER_FAST_TEST(user_delay_us, true, true, test_user_delay_us);
