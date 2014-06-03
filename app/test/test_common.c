/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <rte_common.h>
#include <rte_hexdump.h>

#include "test.h"

#define MAX_NUM 1 << 20

#define FAIL(x)\
	{printf(x "() test failed!\n");\
	return -1;}

/* this is really a sanity check */
static int
test_macros(int __rte_unused unused_parm)
{
#define SMALLER 0x1000U
#define BIGGER 0x2000U
#define PTR_DIFF BIGGER - SMALLER
#define FAIL_MACRO(x)\
	{printf(#x "() test failed!\n");\
	return -1;}

	uintptr_t unused = 0;

	RTE_SET_USED(unused);

	if ((uintptr_t)RTE_PTR_ADD(SMALLER, PTR_DIFF) != BIGGER)
		FAIL_MACRO(RTE_PTR_ADD);
	if ((uintptr_t)RTE_PTR_SUB(BIGGER, PTR_DIFF) != SMALLER)
		FAIL_MACRO(RTE_PTR_SUB);
	if (RTE_PTR_DIFF(BIGGER, SMALLER) != PTR_DIFF)
		FAIL_MACRO(RTE_PTR_DIFF);
	if (RTE_MAX(SMALLER, BIGGER) != BIGGER)
		FAIL_MACRO(RTE_MAX);
	if (RTE_MIN(SMALLER, BIGGER) != SMALLER)
		FAIL_MACRO(RTE_MIN);

	if (strncmp(RTE_STR(test), "test", sizeof("test")))
		FAIL_MACRO(RTE_STR);

	return 0;
}

static int
test_misc(void)
{
	char memdump[] = "memdump_test";
	if (rte_bsf32(129))
		FAIL("rte_bsf32");

	rte_memdump(stdout, "test", memdump, sizeof(memdump));
	rte_hexdump(stdout, "test", memdump, sizeof(memdump));

	rte_pause();

	return 0;
}

static int
test_align(void)
{
#define FAIL_ALIGN(x, i, p)\
	{printf(x "() test failed: %u %u\n", i, p);\
	return -1;}
#define ERROR_FLOOR(res, i, pow) \
		(res % pow) || 						/* check if not aligned */ \
		((res / pow) != (i / pow))  		/* check if correct alignment */
#define ERROR_CEIL(res, i, pow) \
		(res % pow) ||						/* check if not aligned */ \
			((i % pow) == 0 ?				/* check if ceiling is invoked */ \
			val / pow != i / pow :			/* if aligned */ \
			val / pow != (i / pow) + 1)		/* if not aligned, hence +1 */

	uint32_t i, p, val;

	for (i = 1, p = 1; i <= MAX_NUM; i ++) {
		if (rte_align32pow2(i) != p)
			FAIL_ALIGN("rte_align32pow2", i, p);
		if (i == p)
			p <<= 1;
	}

	for (p = 2; p <= MAX_NUM; p <<= 1) {

		if (!rte_is_power_of_2(p))
			FAIL("rte_is_power_of_2");

		for (i = 1; i <= MAX_NUM; i++) {
			/* align floor */
			if (rte_align_floor_int((uintptr_t)i, p) % p)
				FAIL_ALIGN("rte_align_floor_int", i, p);

			val = RTE_PTR_ALIGN_FLOOR((uintptr_t) i, p);
			if (ERROR_FLOOR(val, i, p))
				FAIL_ALIGN("RTE_PTR_ALIGN_FLOOR", i, p);

			val = RTE_ALIGN_FLOOR(i, p);
			if (ERROR_FLOOR(val, i, p))
				FAIL_ALIGN("RTE_ALIGN_FLOOR", i, p);

			/* align ceiling */
			val = RTE_PTR_ALIGN((uintptr_t) i, p);
			if (ERROR_CEIL(val, i, p))
				FAIL_ALIGN("RTE_PTR_ALIGN", i, p);

			val = RTE_ALIGN(i, p);
			if (ERROR_CEIL(val, i, p))
				FAIL_ALIGN("RTE_ALIGN", i, p);

			val = RTE_ALIGN_CEIL(i, p);
			if (ERROR_CEIL(val, i, p))
				FAIL_ALIGN("RTE_ALIGN_CEIL", i, p);

			val = RTE_PTR_ALIGN_CEIL((uintptr_t)i, p);
			if (ERROR_CEIL(val, i, p))
				FAIL_ALIGN("RTE_PTR_ALIGN_CEIL", i, p);

			/* by this point we know that val is aligned to p */
			if (!rte_is_aligned((void*)(uintptr_t) val, p))
				FAIL("rte_is_aligned");
		}
	}
	return 0;
}

int
test_common(void)
{
	int ret = 0;
	ret |= test_align();
	ret |= test_macros(0);
	ret |= test_misc();

	return ret;
}
