/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_evdev.h"
#include "otx2_tim_evdev.h"

int
otx2_tim_caps_get(const struct rte_eventdev *evdev, uint64_t flags,
		  uint32_t *caps,
		  const struct rte_event_timer_adapter_ops **ops)
{
	struct otx2_tim_evdev *dev = tim_priv_get();

	RTE_SET_USED(flags);
	RTE_SET_USED(ops);
	if (dev == NULL)
		return -ENODEV;

	/* Store evdev pointer for later use. */
	dev->event_dev = (struct rte_eventdev *)(uintptr_t)evdev;
	*caps = RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT;

	return 0;
}

void
otx2_tim_init(struct rte_pci_device *pci_dev, struct otx2_dev *cmn_dev)
{
	struct rsrc_attach_req *atch_req;
	struct free_rsrcs_rsp *rsrc_cnt;
	const struct rte_memzone *mz;
	struct otx2_tim_evdev *dev;
	int rc;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	mz = rte_memzone_reserve(RTE_STR(OTX2_TIM_EVDEV_NAME),
				 sizeof(struct otx2_tim_evdev),
				 rte_socket_id(), 0);
	if (mz == NULL) {
		otx2_tim_dbg("Unable to allocate memory for TIM Event device");
		return;
	}

	dev = mz->addr;
	dev->pci_dev = pci_dev;
	dev->mbox = cmn_dev->mbox;
	dev->bar2 = cmn_dev->bar2;

	otx2_mbox_alloc_msg_free_rsrc_cnt(dev->mbox);
	rc = otx2_mbox_process_msg(dev->mbox, (void *)&rsrc_cnt);
	if (rc < 0) {
		otx2_err("Unable to get free rsrc count.");
		goto mz_free;
	}

	dev->nb_rings = rsrc_cnt->tim;

	if (!dev->nb_rings) {
		otx2_tim_dbg("No TIM Logical functions provisioned.");
		goto mz_free;
	}

	atch_req = otx2_mbox_alloc_msg_attach_resources(dev->mbox);
	atch_req->modify = true;
	atch_req->timlfs = dev->nb_rings;

	rc = otx2_mbox_process(dev->mbox);
	if (rc < 0) {
		otx2_err("Unable to attach TIM rings.");
		goto mz_free;
	}

	return;

mz_free:
	rte_memzone_free(mz);
}

void
otx2_tim_fini(void)
{
	struct otx2_tim_evdev *dev = tim_priv_get();
	struct rsrc_detach_req *dtch_req;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	dtch_req = otx2_mbox_alloc_msg_detach_resources(dev->mbox);
	dtch_req->partial = true;
	dtch_req->timlfs = true;

	otx2_mbox_process(dev->mbox);
	rte_memzone_free(rte_memzone_lookup(RTE_STR(OTX2_TIM_EVDEV_NAME)));
}
