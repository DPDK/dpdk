/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024, NVIDIA CORPORATION & AFFILIATES.
 */

#include <stdint.h>

#include <rte_log.h>
#include <virtio_ha.h>
#include <rte_vf_rpc.h>

#include "vdpa_ha.h"

#define RTE_LOGTYPE_HA RTE_LOGTYPE_USER1

static struct virtio_ha_vf_restore_queue rq;
static pthread_mutex_t vf_restore_lock = PTHREAD_MUTEX_INITIALIZER;

static void
virtio_ha_client_dma_map(uint64_t iova, uint64_t len, bool map)
{
	struct virtio_ha_global_dma_map dma_map = {.iova = iova, .size = len};
	int ret;

	if (map) {
		ret = virtio_ha_global_dma_map_store(&dma_map);
		if (ret < 0)
			RTE_LOG(ERR, HA, "Failed to store dma map\n");
	} else {
		ret = virtio_ha_global_dma_map_remove(&dma_map);
		if (ret < 0)
			RTE_LOG(ERR, HA, "Failed to remove dma map\n");	
	}

	return;
}

static void
virtio_ha_client_cfd_store(int container_fd)
{
	int ret;

	ret = virtio_ha_global_cfd_store(container_fd);
	if (ret < 0)
		RTE_LOG(ERR, HA, "Failed to store container fd\n");

	return;
}

int
virtio_ha_client_start(ver_time_set set_ver)
{
	int ret, vfio_container_fd = -1; 

	ret = virtio_ha_ipc_client_init(set_ver);
	if (ret) {
		RTE_LOG(ERR, HA, "Failed to init ha ipc client\n");
		return -1;		
	}

	rte_vfio_register_dma_cb(virtio_ha_client_dma_map, virtio_ha_client_cfd_store);

	ret = virtio_ha_global_cfd_query(&vfio_container_fd);
	if (ret < 0) {
		RTE_LOG(ERR, HA, "Failed to query global container fd\n");
		return -1;
	} else {
		RTE_LOG(INFO, HA, "Query success: global container fd(%d)\n", vfio_container_fd);
	}

	rte_vfio_restore_default_cfd(vfio_container_fd);

	return 0;
}

static void
cleanup_restore_queue(void)
{
	struct virtio_ha_vf_to_restore *vf_dev, *next;

	if (!TAILQ_EMPTY(&rq.prio_q)) {
		for (vf_dev = TAILQ_FIRST(&rq.prio_q);
				vf_dev != NULL; vf_dev = next) {
			next = TAILQ_NEXT(vf_dev, next);
			TAILQ_REMOVE(&rq.prio_q, vf_dev, next);
			free(vf_dev);
		}
	}
	if (!TAILQ_EMPTY(&rq.non_prio_q)) {
		for (vf_dev = TAILQ_FIRST(&rq.non_prio_q);
				vf_dev != NULL; vf_dev = next) {
			next = TAILQ_NEXT(vf_dev, next);
			TAILQ_REMOVE(&rq.non_prio_q, vf_dev, next);
			free(vf_dev);
		}
	}
}

int
virtio_ha_client_dev_restore_pf(int *total_vf)
{
	struct virtio_dev_name *pf_list = NULL;
	struct vdpa_vf_with_devargs *vf_list = NULL;
	struct virtio_pf_ctx pf_ctx;
	struct virtio_ha_vf_to_restore *vf_dev, *v1, *v2, *tmp;
	int ret, i, j, nr_pf, nr_vf;

	ret = virtio_ha_pf_list_query(&pf_list);
	if (ret < 0) {
		RTE_LOG(ERR, HA, "Failed to query pf list\n");
		return -1;
	} else if (ret == 0) {
		RTE_LOG(INFO, HA, "Query success: 0 pf to restore\n");
		return 0;
	} else {
		RTE_LOG(INFO, HA, "Query success: %d pf to restore\n", ret);
	}

	nr_pf = ret;

	TAILQ_INIT(&rq.non_prio_q);
	TAILQ_INIT(&rq.prio_q);
	pthread_mutex_init(&rq.lock, NULL);

	for (i = 0; i < nr_pf; i++) {
		ret = virtio_ha_pf_ctx_query(pf_list + i, &pf_ctx);
		if (ret < 0) {
			RTE_LOG(ERR, HA, "Failed to query pf ctx\n");
			ret = -1;
			goto err;
		}

		ret = virtio_ha_pf_ctx_set(pf_list + i, &pf_ctx, 0);
		if (ret < 0) {
			RTE_LOG(ERR, HA, "Failed to set pf ctx in pf driver\n");
			ret = -1;
			goto err;
		}

		ret = rte_vdpa_pf_dev_add(pf_list[i].dev_bdf);
		if (ret < 0) {
			RTE_LOG(ERR, HA, "Failed to restore pf\n");
			ret = -1;
			goto err;
		}

		ret = virtio_ha_pf_ctx_unset(pf_list + i);
		if (ret < 0) {
			RTE_LOG(ERR, HA, "Failed to unset pf ctx in pf driver\n");
			ret = -1;
			goto err;
		}
	}

	for (i = 0; i < nr_pf; i++) {
		ret = virtio_ha_vf_list_query(pf_list + i, &vf_list);
		if (ret < 0) {
			RTE_LOG(ERR, HA, "Failed to query vf list of %s\n", pf_list[i].dev_bdf);
			ret = -1;
			goto err;		
		} else {
			RTE_LOG(INFO, HA, "Query success: %d vf of pf %s to restore\n",
				ret, pf_list[i].dev_bdf);
		}

		nr_vf = ret;
		*total_vf += nr_vf;

		/* No need to take priority queue lock as the priority channel is not inited yet */
		for (j = 0; j < nr_vf; j++) {
			vf_dev = malloc(sizeof(struct virtio_ha_vf_to_restore));
			if (!vf_dev) {
				RTE_LOG(ERR, HA, "Failed to alloc restore vf\n");
				free(vf_list);
				ret = -1;
				goto err;
			}
			memset(vf_dev, 0, sizeof(struct virtio_ha_vf_to_restore));
			memcpy(&vf_dev->vf_devargs, vf_list + j, sizeof(struct vdpa_vf_with_devargs));
			memcpy(&vf_dev->pf_name, pf_list + i, sizeof(struct virtio_dev_name));
			vf_dev->vf_cnt_vm = 1;
			TAILQ_INSERT_TAIL(&rq.non_prio_q, vf_dev, next);
		}

		free(vf_list);
		vf_list = NULL;
	}

	TAILQ_FOREACH(v1, &rq.non_prio_q, next) {
		tmp = TAILQ_NEXT(v1, next);
		while (tmp != NULL) {
			v2 = tmp;
			if (strncmp(v1->vf_devargs.vm_uuid, v2->vf_devargs.vm_uuid, RTE_UUID_STRLEN) == 0 &&
					v1->vf_devargs.mem_tbl_set && v2->vf_devargs.mem_tbl_set) {
				v1->vf_cnt_vm++;
				v2->vf_cnt_vm++;
			}
			tmp = TAILQ_NEXT(tmp, next);
		}
		if (TAILQ_NEXT(v1, next) == NULL)
			break;
	}

	ret = virtio_ha_prio_chnl_init(&rq);
	if (ret) {
		RTE_LOG(ERR, HA, "Failed to init priority channel\n");
		cleanup_restore_queue();
	}

err:
	free(pf_list);
	return ret;
}

extern int stage1;

#define MAX_WAIT_ROUND 100 /* Max wait time = 100 * 50 = 5000 ms */
#define WAIT_INTERVAL 50000 /* 50000 us = 50 ms */
int
virtio_ha_client_dev_restore_vf(int total_vf)
{
	struct virtio_ha_vf_to_restore *vf_dev;
	struct vdpa_vf_ctx *vf_ctx = NULL;
	struct vdpa_vf_params vf_info = {0};
	int ret = 0, i;

	while (total_vf > 0 && ret != -1) {
		pthread_mutex_lock(&vf_restore_lock);
		pthread_mutex_lock(&rq.lock);

		if (TAILQ_EMPTY(&rq.prio_q)) {
			vf_dev = TAILQ_FIRST(&rq.non_prio_q);
			TAILQ_REMOVE(&rq.non_prio_q, vf_dev, next);
		} else {
			vf_dev = TAILQ_FIRST(&rq.prio_q);
			TAILQ_REMOVE(&rq.prio_q, vf_dev, next);
		}

		pthread_mutex_unlock(&rq.lock);

		ret = virtio_ha_vf_ctx_query(&vf_dev->vf_devargs.vf_name, &vf_dev->pf_name, &vf_ctx);
		if (ret < 0) {
			RTE_LOG(ERR, HA, "Failed to query vf ctx (ret %d)\n", ret);
			pthread_mutex_unlock(&vf_restore_lock);
			ret = -1;
			goto err;
		}

		ret = virtio_ha_vf_ctx_set(&vf_dev->vf_devargs.vf_name, vf_ctx, vf_dev->vf_cnt_vm);
		if (ret < 0) {
			RTE_LOG(ERR, HA, "Failed to set vf ctx in vf driver\n");
			pthread_mutex_unlock(&vf_restore_lock);
			ret = -1;
			goto err_vf_ctx;
		}

		ret = rte_vdpa_vf_dev_add(vf_dev->vf_devargs.vf_name.dev_bdf, vf_dev->vf_devargs.vm_uuid,
					NULL, stage1, vf_dev->vf_devargs.vhost_sock_addr);
		if (ret < 0) {
			RTE_LOG(ERR, HA, "Failed to restore vf\n");
			pthread_mutex_unlock(&vf_restore_lock);
			ret = -1;
			goto err_vf_ctx;
		}

		if (vf_ctx->ctt.vhost_fd_saved) {
			ret = virtio_ha_vf_vhost_fd_remove(&vf_dev->vf_devargs.vf_name, &vf_dev->pf_name);
			if (ret < 0) {
				RTE_LOG(ERR, HA, "Failed to close vhost fd\n");
				pthread_mutex_unlock(&vf_restore_lock);
				ret = -1;
				goto err_vf_ctx;
			}
		}

		ret = vdpa_with_socket_path_start(vf_dev->vf_devargs.vf_name.dev_bdf,
			vf_dev->vf_devargs.vhost_sock_addr);
		if (ret < 0) {
			RTE_LOG(ERR, HA, "Failed to start vdpa\n");
			pthread_mutex_unlock(&vf_restore_lock);
			ret = -1;
			goto err_vf_ctx;
		}

		ret = virtio_ha_vf_ctx_unset(&vf_dev->vf_devargs.vf_name);
		if (ret < 0) {
			RTE_LOG(ERR, HA, "Failed to unset vf ctx in vf driver\n");
			pthread_mutex_unlock(&vf_restore_lock);
			ret = -1;
			goto err_vf_ctx;
		}
		pthread_mutex_unlock(&vf_restore_lock);
		total_vf--;
		if (vf_ctx->ctt.vhost_fd_saved) {
			for (i = 0; i < MAX_WAIT_ROUND; i++) {
				ret = rte_vdpa_get_vf_info(vf_dev->vf_devargs.vf_name.dev_bdf, &vf_info);
				if (ret < 0) {
					RTE_LOG(WARNING, HA, "Failed to get VF info\n");
					continue;				
				}
				if (vf_info.configured)
					break;
				usleep(WAIT_INTERVAL);
			}
		}
err_vf_ctx:
		if (vf_ctx) {
			free(vf_ctx);
			vf_ctx = NULL;
		}
err:
		if (vf_dev) {
			free(vf_dev);
			vf_dev = NULL;
		}
		memset(&vf_info, 0, sizeof(struct vdpa_vf_params));
	}

	cleanup_restore_queue();
	virtio_ha_prio_chnl_destroy();
	return ret;
}

bool
virtio_ha_client_pf_has_restore_vf(const char *pf_name)
{
	struct virtio_ha_vf_to_restore *vf_dev;
	bool found = false;

	/* Take vf_restore_lock to make sure no VF is restoring */
	pthread_mutex_lock(&vf_restore_lock);
	pthread_mutex_lock(&rq.lock);
	TAILQ_FOREACH(vf_dev, &rq.non_prio_q, next) {
		if (!strcmp(vf_dev->pf_name.dev_bdf, pf_name)) {
			found = true;
			break;
		}
	}

	if (found)
		goto exit;

	TAILQ_FOREACH(vf_dev, &rq.prio_q, next) {
		if (!strcmp(vf_dev->pf_name.dev_bdf, pf_name)) {
			found = true;
			break;
		}
	}

exit:
	pthread_mutex_unlock(&rq.lock);
	pthread_mutex_unlock(&vf_restore_lock);
	return found;
}

uint32_t
virtio_ha_client_pf_vf_list(const char *pf_name, struct vdpa_vf_with_devargs **vf_list)
{
	struct virtio_ha_vf_to_restore *vf_dev;
	struct vdpa_vf_with_devargs *vfs;
	uint32_t nr_vf = 0;

	pthread_mutex_lock(&rq.lock);
	TAILQ_FOREACH(vf_dev, &rq.non_prio_q, next) {
		if (!strcmp(vf_dev->pf_name.dev_bdf, pf_name))
			nr_vf++;
	}

	TAILQ_FOREACH(vf_dev, &rq.prio_q, next) {
		if (!strcmp(vf_dev->pf_name.dev_bdf, pf_name))
			nr_vf++;
	}

	if (nr_vf == 0)
		goto exit;

	vfs = malloc(nr_vf * sizeof(struct vdpa_vf_with_devargs));
	if (!vfs) {
		RTE_LOG(ERR, HA, "Failed to alloc vf list\n");
		nr_vf = 0;
		goto exit;
	}

	memset(vfs, 0, nr_vf * sizeof(struct vdpa_vf_with_devargs));
	nr_vf = 0;
	TAILQ_FOREACH(vf_dev, &rq.non_prio_q, next) {
		if (!strcmp(vf_dev->pf_name.dev_bdf, pf_name)) {
			memcpy(vfs + nr_vf, &vf_dev->vf_devargs, sizeof(struct vdpa_vf_with_devargs));
			nr_vf++;
		}
	}

	TAILQ_FOREACH(vf_dev, &rq.prio_q, next) {
		if (!strcmp(vf_dev->pf_name.dev_bdf, pf_name)) {
			memcpy(vfs + nr_vf, &vf_dev->vf_devargs, sizeof(struct vdpa_vf_with_devargs));
			nr_vf++;
		}
	}

	*vf_list = vfs;
exit:
	pthread_mutex_unlock(&rq.lock);
	return nr_vf;
}

static inline void
add_vf_to_priority_queue(struct virtio_ha_vf_to_restore *vf_dev)
{
	TAILQ_REMOVE(&rq.non_prio_q, vf_dev, next);
	TAILQ_INSERT_TAIL(&rq.prio_q, vf_dev, next);
}

bool
virtio_ha_client_vf_in_restore(const char *vf_name)
{
	struct virtio_ha_vf_to_restore *vf_dev;
	bool found = false;

	/* Take vf_restore_lock to make sure no VF is restoring */
	pthread_mutex_lock(&vf_restore_lock);
	pthread_mutex_lock(&rq.lock);
	TAILQ_FOREACH(vf_dev, &rq.non_prio_q, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf_name)) {
			add_vf_to_priority_queue(vf_dev);
			found = true;
			break;
		}
	}

	if (found)
		goto exit;

	TAILQ_FOREACH(vf_dev, &rq.prio_q, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf_name)) {
			found = true;
			break;
		}
	}

exit:
	pthread_mutex_unlock(&rq.lock);
	pthread_mutex_unlock(&vf_restore_lock);
	return found;
}

bool
virtio_ha_client_vf_restore_devargs(const char *vf_name, struct vdpa_vf_with_devargs *args)
{
	struct virtio_ha_vf_to_restore *vf_dev;
	bool found = false;

	/* Take vf_restore_lock to make sure no VF is restoring */
	pthread_mutex_lock(&vf_restore_lock);
	pthread_mutex_lock(&rq.lock);
	TAILQ_FOREACH(vf_dev, &rq.non_prio_q, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf_name)) {
			memcpy(args, &vf_dev->vf_devargs, sizeof(struct vdpa_vf_with_devargs));
			found = true;
			break;
		}
	}

	if (found)
		goto exit;

	TAILQ_FOREACH(vf_dev, &rq.prio_q, next) {
		if (!strcmp(vf_dev->vf_devargs.vf_name.dev_bdf, vf_name)) {
			memcpy(args, &vf_dev->vf_devargs, sizeof(struct vdpa_vf_with_devargs));
			found = true;
			break;
		}
	}

exit:
	pthread_mutex_unlock(&rq.lock);
	pthread_mutex_unlock(&vf_restore_lock);
	return found;
}

void
virtio_ha_dev_lock(void)
{
	pthread_mutex_lock(&vf_restore_lock);
};

void
virtio_ha_dev_unlock(void)
{
	pthread_mutex_unlock(&vf_restore_lock);
};
