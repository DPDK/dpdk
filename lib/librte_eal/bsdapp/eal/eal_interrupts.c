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

#include <rte_common.h>
#include <rte_interrupts.h>
#include "eal_private.h"

int
rte_intr_callback_register(struct rte_intr_handle *intr_handle __rte_unused,
			rte_intr_callback_fn cb __rte_unused,
			void *cb_arg __rte_unused)
{
	return -ENOTSUP;
}

int
rte_intr_callback_unregister(struct rte_intr_handle *intr_handle __rte_unused,
			rte_intr_callback_fn cb_fn __rte_unused,
			void *cb_arg __rte_unused)
{
	return -ENOTSUP;
}

int
rte_intr_enable(struct rte_intr_handle *intr_handle __rte_unused)
{
	return -ENOTSUP;
}

int
rte_intr_disable(struct rte_intr_handle *intr_handle __rte_unused)
{
	return -ENOTSUP;
}

int
rte_eal_intr_init(void)
{
	return 0;
}

int
rte_intr_rx_ctl(struct rte_intr_handle *intr_handle,
		int epfd, int op, unsigned int vec, void *data)
{
	RTE_SET_USED(intr_handle);
	RTE_SET_USED(epfd);
	RTE_SET_USED(op);
	RTE_SET_USED(vec);
	RTE_SET_USED(data);

	return -ENOTSUP;
}

int
rte_intr_efd_enable(struct rte_intr_handle *intr_handle, uint32_t nb_efd)
{
	RTE_SET_USED(intr_handle);
	RTE_SET_USED(nb_efd);

	return 0;
}

void
rte_intr_efd_disable(struct rte_intr_handle *intr_handle)
{
	RTE_SET_USED(intr_handle);
}

int
rte_intr_dp_is_en(struct rte_intr_handle *intr_handle)
{
	RTE_SET_USED(intr_handle);
	return 0;
}

int
rte_intr_allow_others(struct rte_intr_handle *intr_handle)
{
	RTE_SET_USED(intr_handle);
	return 1;
}

int
rte_intr_cap_multiple(struct rte_intr_handle *intr_handle)
{
	RTE_SET_USED(intr_handle);
	return 0;
}
