/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#include <rte_malloc.h>

#include "failsafe_private.h"

static int
fs_bus_init(struct rte_eth_dev *dev)
{
	struct sub_device *sdev;
	struct rte_devargs *da;
	uint8_t i;
	int ret;

	FOREACH_SUBDEV(sdev, i, dev) {
		if (sdev->state != DEV_PARSED)
			continue;
		da = &sdev->devargs;
		ret = rte_eal_hotplug_add(da->bus->name,
					  da->name,
					  da->args);
		if (ret) {
			ERROR("sub_device %d probe failed %s%s%s", i,
			      rte_errno ? "(" : "",
			      rte_errno ? strerror(rte_errno) : "",
			      rte_errno ? ")" : "");
			continue;
		}
		ETH(sdev) = rte_eth_dev_allocated(da->name);
		if (ETH(sdev) == NULL) {
			ERROR("sub_device %d init went wrong", i);
			return -ENODEV;
		}
		SUB_ID(sdev) = i;
		sdev->fs_dev = dev;
		sdev->dev = ETH(sdev)->device;
		ETH(sdev)->state = RTE_ETH_DEV_DEFERRED;
		sdev->state = DEV_PROBED;
	}
	return 0;
}

int
failsafe_eal_init(struct rte_eth_dev *dev)
{
	int ret;

	ret = fs_bus_init(dev);
	if (ret)
		return ret;
	if (PRIV(dev)->state < DEV_PROBED)
		PRIV(dev)->state = DEV_PROBED;
	fs_switch_dev(dev, NULL);
	return 0;
}

static int
fs_bus_uninit(struct rte_eth_dev *dev)
{
	struct sub_device *sdev = NULL;
	uint8_t i;
	int ret;

	FOREACH_SUBDEV_STATE(sdev, i, dev, DEV_PROBED) {
		ret = rte_eal_hotplug_remove(sdev->bus->name,
					     sdev->dev->name);
		if (ret) {
			ERROR("Failed to remove requested device %s",
			      sdev->dev->name);
			continue;
		}
		sdev->state = DEV_PROBED - 1;
	}
	return 0;
}

int
failsafe_eal_uninit(struct rte_eth_dev *dev)
{
	int ret;

	ret = fs_bus_uninit(dev);
	if (ret)
		return ret;
	PRIV(dev)->state = DEV_PROBED - 1;
	return 0;
}
