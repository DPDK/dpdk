/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#include <unistd.h>

#include "ark_logs.h"
#include "ark_mpu.h"

uint16_t
ark_api_num_queues(struct ark_mpu_t *mpu)
{
	return mpu->hw.num_queues;
}

uint16_t
ark_api_num_queues_per_port(struct ark_mpu_t *mpu, uint16_t ark_ports)
{
	return mpu->hw.num_queues / ark_ports;
}

int
ark_mpu_verify(struct ark_mpu_t *mpu, uint32_t obj_size)
{
	uint32_t version;

	version = mpu->id.vernum;
	if (mpu->id.idnum != ARK_MPU_MODID ||
	    version != ARK_MPU_MODVER ||
	    mpu->hw.obj_size != obj_size) {
		ARK_PMD_LOG(ERR,
			    "   MPU module not found as expected %08x"
			    " \"%c%c%c%c %c%c%c%c\"\n",
			    mpu->id.idnum,
			    mpu->id.id[0], mpu->id.id[1],
			    mpu->id.id[2], mpu->id.id[3],
			    mpu->id.ver[0], mpu->id.ver[1],
			    mpu->id.ver[2], mpu->id.ver[3]);
		ARK_PMD_LOG(ERR,
			    "   MPU HW num_queues: %u hw_depth %u,"
			    " obj_size: %u, obj_per_mrr: %u"
			    " Expected size %u\n",
			    mpu->hw.num_queues,
			    mpu->hw.hw_depth,
			    mpu->hw.obj_size,
			    mpu->hw.obj_per_mrr,
			    obj_size);
		return -1;
	}
	return 0;
}

void
ark_mpu_stop(struct ark_mpu_t *mpu)
{
	mpu->cfg.command = MPU_CMD_STOP;
}

void
ark_mpu_start(struct ark_mpu_t *mpu)
{
	mpu->cfg.command = MPU_CMD_RUN;
}

int
ark_mpu_reset(struct ark_mpu_t *mpu)
{
	int cnt = 0;

	mpu->cfg.command = MPU_CMD_RESET;
	rte_wmb();

	while (mpu->cfg.command != MPU_CMD_IDLE) {
		if (cnt++ > 1000)
			break;
		usleep(10);
	}
	if (mpu->cfg.command != MPU_CMD_IDLE) {
		mpu->cfg.command = MPU_CMD_FORCE_RESET;
		usleep(10);
	}
	return mpu->cfg.command != MPU_CMD_IDLE;
}

int
ark_mpu_configure(struct ark_mpu_t *mpu, rte_iova_t ring, uint32_t ring_size,
		  int is_tx)
{
	ark_mpu_reset(mpu);

	if (!rte_is_power_of_2(ring_size)) {
		ARK_PMD_LOG(ERR, "Invalid ring size for MPU %d\n",
			    ring_size);
		return -1;
	}

	mpu->cfg.ring_base = ring;
	mpu->cfg.ring_size = ring_size;
	mpu->cfg.ring_mask = ring_size - 1;
	mpu->cfg.min_host_move = is_tx ? 1 : mpu->hw.obj_per_mrr;
	mpu->cfg.min_hw_move = mpu->hw.obj_per_mrr;
	mpu->cfg.sw_prod_index = 0;
	mpu->cfg.hw_cons_index = 0;
	return 0;
}

void
ark_mpu_dump(struct ark_mpu_t *mpu, const char *code, uint16_t qid)
{
	/* DUMP to see that we have started */
	ARK_PMD_LOG(DEBUG, "MPU: %s Q: %3u sw_prod %u, hw_cons: %u\n",
		      code, qid,
		      mpu->cfg.sw_prod_index, mpu->cfg.hw_cons_index);
}

void
ark_mpu_dump_setup(struct ark_mpu_t *mpu, uint16_t q_id)
{
	ARK_PMD_LOG(DEBUG, "MPU Setup Q: %u"
		      ARK_SU64X "\n",
		      q_id,
		      "ring_base", mpu->cfg.ring_base
		      );
}
