/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */
#include <rte_cryptodev.h>

#include "otx2_cryptodev.h"
#include "otx2_cryptodev_mbox.h"
#include "otx2_dev.h"
#include "otx2_mbox.h"

int
otx2_cpt_available_queues_get(const struct rte_cryptodev *dev,
			      uint16_t *nb_queues)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	struct otx2_dev *otx2_dev = &vf->otx2_dev;
	struct free_rsrcs_rsp *rsp;
	int ret;

	otx2_mbox_alloc_msg_free_rsrc_cnt(otx2_dev->mbox);

	ret = otx2_mbox_process_msg(otx2_dev->mbox, (void *)&rsp);
	if (ret)
		return -EIO;

	*nb_queues = rsp->cpt;
	return 0;
}
