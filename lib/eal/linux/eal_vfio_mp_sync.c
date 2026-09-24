/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <rte_errno.h>
#include <rte_log.h>
#include <dev_vfio.h>
#include <rte_eal.h>

#include "eal_private.h"
#include "eal_vfio.h"

static int
vfio_mp_primary(const struct rte_mp_msg *msg, const void *peer)
{
	int fd = -1;
	struct rte_mp_msg reply;
	struct vfio_mp_param *r = (struct vfio_mp_param *)reply.param;
	const struct vfio_mp_param *m =
		(const struct vfio_mp_param *)msg->param;

	if (msg->len_param != sizeof(*m)) {
		EAL_LOG(ERR, "vfio received invalid message!");
		return -1;
	}

	memset(&reply, 0, sizeof(reply));

	switch (m->req) {
	case VFIO_SOCKET_REQ_GROUP:
	{
		struct vfio_container *cfg = vfio_global_cfg.default_cfg;
		struct vfio_group *grp;

		if (vfio_global_cfg.mode != DEV_VFIO_MODE_GROUP) {
			EAL_LOG(ERR, "VFIO not initialized in group mode");
			r->result = VFIO_SOCKET_ERR;
			break;
		}

		r->req = VFIO_SOCKET_REQ_GROUP;
		r->group_num = m->group_num;
		grp = vfio_group_get_by_num(cfg, m->group_num);
		if (grp == NULL) {
			/* group doesn't exist in primary */
			r->result = VFIO_SOCKET_NO_FD;
		} else {
			/* group exists and is bound to VFIO driver */
			fd = grp->fd;
			r->result = VFIO_SOCKET_OK;
			reply.num_fds = 1;
			reply.fds[0] = fd;
		}
		break;
	}
	case VFIO_SOCKET_REQ_CONTAINER:
		r->req = VFIO_SOCKET_REQ_CONTAINER;
		fd = dev_vfio_get_container_fd();
		if (fd < 0)
			r->result = VFIO_SOCKET_ERR;
		else {
			r->result = VFIO_SOCKET_OK;
			r->mode = vfio_global_cfg.mode;
			reply.num_fds = 1;
			reply.fds[0] = fd;
		}
		break;
	case VFIO_SOCKET_REQ_IOMMU_TYPE:
	{
		int iommu_type_id;

		if (vfio_global_cfg.mode != DEV_VFIO_MODE_GROUP) {
			EAL_LOG(ERR, "VFIO not initialized in group mode");
			r->result = VFIO_SOCKET_ERR;
			break;
		}

		r->req = VFIO_SOCKET_REQ_IOMMU_TYPE;

		iommu_type_id = vfio_get_iommu_type();

		if (iommu_type_id < 0)
			r->result = VFIO_SOCKET_ERR;
		else {
			r->iommu_type_id = iommu_type_id;
			r->result = VFIO_SOCKET_OK;
		}
		break;
	}
	case VFIO_SOCKET_REQ_IOVA_MODE:
		if (vfio_global_cfg.mode == DEV_VFIO_MODE_NONE ||
				vfio_global_cfg.iova_mode == DEV_VFIO_IOVA_MODE_UNKNOWN) {
			EAL_LOG(ERR, "VFIO IOVA mode is not initialized");
			r->result = VFIO_SOCKET_ERR;
			break;
		}

		r->req = VFIO_SOCKET_REQ_IOVA_MODE;
		r->iova_mode = vfio_global_cfg.iova_mode;
		r->result = VFIO_SOCKET_OK;
		break;
	default:
		EAL_LOG(ERR, "vfio received invalid message!");
		return -1;
	}

	strcpy(reply.name, EAL_VFIO_MP);
	reply.len_param = sizeof(*r);

	return rte_mp_reply(&reply, peer);
}

int
vfio_mp_sync_setup(void)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		int ret = rte_mp_action_register(EAL_VFIO_MP, vfio_mp_primary);
		if (ret && rte_errno != ENOTSUP) {
			EAL_LOG(ERR, "Multiprocess sync setup failed: %d (%s)",
					rte_errno, rte_strerror(rte_errno));
			return -1;
		}
	}

	return 0;
}

void
vfio_mp_sync_cleanup(void)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	rte_mp_action_unregister(EAL_VFIO_MP);
}
