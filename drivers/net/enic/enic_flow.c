/*
 * Copyright (c) 2017, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <errno.h>
#include <rte_log.h>
#include <rte_ethdev.h>
#include <rte_flow_driver.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include "enic_compat.h"
#include "enic.h"
#include "vnic_dev.h"
#include "vnic_nic.h"

#ifdef RTE_LIBRTE_ENIC_DEBUG_FLOW
#define FLOW_TRACE() \
	RTE_LOG(DEBUG, PMD, "%s()\n", __func__)
#define FLOW_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, fmt, ## args)
#else
#define FLOW_TRACE() do { } while (0)
#define FLOW_LOG(level, fmt, args...) do { } while (0)
#endif

/*
 * The following functions are callbacks for Generic flow API.
 */

/**
 * Validate a flow supported by the NIC.
 *
 * @see rte_flow_validate()
 * @see rte_flow_ops
 */
static int
enic_flow_validate(struct rte_eth_dev *dev, const struct rte_flow_attr *attrs,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	(void)dev;
	(void)attrs;
	(void)pattern;
	(void)actions;
	(void)error;

	FLOW_TRACE();

	return 0;
}

/**
 * Create a flow supported by the NIC.
 *
 * @see rte_flow_create()
 * @see rte_flow_ops
 */
static struct rte_flow *
enic_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attrs,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	(void)dev;
	(void)attrs;
	(void)pattern;
	(void)actions;
	(void)error;

	FLOW_TRACE();

	return NULL;
}

/**
 * Destroy a flow supported by the NIC.
 *
 * @see rte_flow_destroy()
 * @see rte_flow_ops
 */
static int
enic_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		  __rte_unused struct rte_flow_error *error)
{
	(void)dev;
	(void)flow;

	FLOW_TRACE();

	return 0;
}

/**
 * Flush all flows on the device.
 *
 * @see rte_flow_flush()
 * @see rte_flow_ops
 */
static int
enic_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	(void)dev;
	(void)error;

	FLOW_TRACE();

	return 0;
}

/**
 * Flow callback registration.
 *
 * @see rte_flow_ops
 */
const struct rte_flow_ops enic_flow_ops = {
	.validate = enic_flow_validate,
	.create = enic_flow_create,
	.destroy = enic_flow_destroy,
	.flush = enic_flow_flush,
};
