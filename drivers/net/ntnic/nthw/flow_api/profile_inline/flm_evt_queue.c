/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include <rte_ring.h>
#include <rte_errno.h>

#include "ntlog.h"
#include "flm_evt_queue.h"

/* Local queues for flm statistic events */
static struct rte_ring *info_q_local[MAX_INFO_LCL_QUEUES];

/* Remote queues for flm statistic events */
static struct rte_ring *info_q_remote[MAX_INFO_RMT_QUEUES];

/* Local queues for flm status records */
static struct rte_ring *stat_q_local[MAX_STAT_LCL_QUEUES];

/* Remote queues for flm status records */
static struct rte_ring *stat_q_remote[MAX_STAT_RMT_QUEUES];

static void flm_inf_sta_queue_free(uint8_t port, uint8_t caller)
{
	struct rte_ring *q = NULL;

	/* If queues is not created, then ignore and return */
	switch (caller) {
	case FLM_INFO_LOCAL:
		if (port < MAX_INFO_LCL_QUEUES && info_q_local[port] != NULL) {
			q = info_q_local[port];
			info_q_local[port] = NULL;
		}

		break;

	case FLM_INFO_REMOTE:
		if (port < MAX_INFO_RMT_QUEUES && info_q_remote[port] != NULL) {
			q = info_q_remote[port];
			info_q_remote[port] = NULL;
		}

		break;

	case FLM_STAT_LOCAL:
		if (port < MAX_STAT_LCL_QUEUES && stat_q_local[port] != NULL) {
			q = stat_q_local[port];
			stat_q_local[port] = NULL;
		}

		break;

	case FLM_STAT_REMOTE:
		if (port < MAX_STAT_RMT_QUEUES && stat_q_remote[port] != NULL) {
			q = stat_q_remote[port];
			stat_q_remote[port] = NULL;
		}

		break;

	default:
		NT_LOG(ERR, FILTER, "FLM queue free illegal caller: %u", caller);
		break;
	}

	rte_ring_free(q);
}

void nthw_flm_inf_sta_queue_free_all(uint8_t caller)
{
	int count = 0;

	switch (caller) {
	case FLM_INFO_LOCAL:
		count = MAX_INFO_LCL_QUEUES;
		break;

	case FLM_INFO_REMOTE:
		count = MAX_INFO_RMT_QUEUES;
		break;

	case FLM_STAT_LOCAL:
		count = MAX_STAT_LCL_QUEUES;
		break;

	case FLM_STAT_REMOTE:
		count = MAX_STAT_RMT_QUEUES;
		break;

	default:
		NT_LOG(ERR, FILTER, "FLM queue free illegal caller: %u", caller);
		return;
	}

	for (int i = 0; i < count; i++)
		flm_inf_sta_queue_free(i, caller);
}

static struct rte_ring *flm_evt_queue_create(uint8_t port, uint8_t caller)
{
	static_assert((FLM_EVT_ELEM_SIZE & ~(size_t)3) == FLM_EVT_ELEM_SIZE,
		"FLM EVENT struct size");
	static_assert((FLM_STAT_ELEM_SIZE & ~(size_t)3) == FLM_STAT_ELEM_SIZE,
		"FLM STAT struct size");
	char name[20] = "NONE";
	struct rte_ring *q;
	uint32_t elem_size = 0;
	uint32_t queue_size = 0;

	switch (caller) {
	case FLM_INFO_LOCAL:
		if (port >= MAX_INFO_LCL_QUEUES) {
			NT_LOG(WRN,
				FILTER,
				"FLM statistic event queue cannot be created for port %u. Max supported port is %u",
				port,
				MAX_INFO_LCL_QUEUES - 1);
			return NULL;
		}

		snprintf(name, 20, "LOCAL_INFO%u", port);
		elem_size = FLM_EVT_ELEM_SIZE;
		queue_size = FLM_EVT_QUEUE_SIZE;
		break;

	case FLM_INFO_REMOTE:
		if (port >= MAX_INFO_RMT_QUEUES) {
			NT_LOG(WRN,
				FILTER,
				"FLM statistic event queue cannot be created for vport %u. Max supported vport is %u",
				port,
				MAX_INFO_RMT_QUEUES - 1);
			return NULL;
		}

		snprintf(name, 20, "REMOTE_INFO%u", port);
		elem_size = FLM_EVT_ELEM_SIZE;
		queue_size = FLM_EVT_QUEUE_SIZE;
		break;

	case FLM_STAT_LOCAL:
		if (port >= MAX_STAT_LCL_QUEUES) {
			NT_LOG(WRN,
				FILTER,
				"FLM status queue cannot be created for port %u. Max supported port is %u",
				port,
				MAX_STAT_LCL_QUEUES - 1);
			return NULL;
		}

		snprintf(name, 20, "LOCAL_STAT%u", port);
		elem_size = FLM_STAT_ELEM_SIZE;
		queue_size = FLM_STAT_QUEUE_SIZE;
		break;

	case FLM_STAT_REMOTE:
		if (port >= MAX_STAT_RMT_QUEUES) {
			NT_LOG(WRN,
				FILTER,
				"FLM status queue cannot be created for vport %u. Max supported vport is %u",
				port,
				MAX_STAT_RMT_QUEUES - 1);
			return NULL;
		}

		snprintf(name, 20, "REMOTE_STAT%u", port);
		elem_size = FLM_STAT_ELEM_SIZE;
		queue_size = FLM_STAT_QUEUE_SIZE;
		break;

	default:
		NT_LOG(ERR, FILTER, "FLM queue create illegal caller: %u", caller);
		return NULL;
	}

	q = rte_ring_create_elem(name,
			elem_size,
			queue_size,
			SOCKET_ID_ANY,
			RING_F_SP_ENQ | RING_F_SC_DEQ);

	if (q == NULL) {
		NT_LOG(WRN, FILTER, "FLM queues cannot be created due to error %02X", rte_errno);
		return NULL;
	}

	switch (caller) {
	case FLM_INFO_LOCAL:
		info_q_local[port] = q;
		break;

	case FLM_INFO_REMOTE:
		info_q_remote[port] = q;
		break;

	case FLM_STAT_LOCAL:
		stat_q_local[port] = q;
		break;

	case FLM_STAT_REMOTE:
		stat_q_remote[port] = q;
		break;

	default:
		break;
	}

	return q;
}

int nthw_flm_sta_queue_put(uint8_t port, bool remote, struct flm_status_event_s *obj)
{
	struct rte_ring **stat_q = remote ? stat_q_remote : stat_q_local;

	if (port >= (remote ? MAX_STAT_RMT_QUEUES : MAX_STAT_LCL_QUEUES))
		return -1;

	if (stat_q[port] == NULL) {
		if (flm_evt_queue_create(port, remote ? FLM_STAT_REMOTE : FLM_STAT_LOCAL) == NULL)
			return -1;
	}

	if (rte_ring_sp_enqueue_elem(stat_q[port], obj, FLM_STAT_ELEM_SIZE) != 0) {
		NT_LOG(DBG, FILTER, "FLM local status queue full");
		return -1;
	}

	return 0;
}

void nthw_flm_inf_queue_put(uint8_t port, bool remote, struct flm_info_event_s *obj)
{
	int ret;

	/* If queues is not created, then ignore and return */
	if (!remote) {
		if (port < MAX_INFO_LCL_QUEUES && info_q_local[port] != NULL) {
			ret = rte_ring_sp_enqueue_elem(info_q_local[port], obj, FLM_EVT_ELEM_SIZE);

			if (ret != 0)
				NT_LOG(DBG, FILTER, "FLM local info queue full");
		}

	} else if (port < MAX_INFO_RMT_QUEUES && info_q_remote[port] != NULL) {
		ret = rte_ring_sp_enqueue_elem(info_q_remote[port], obj, FLM_EVT_ELEM_SIZE);

		if (ret != 0)
			NT_LOG(DBG, FILTER, "FLM remote info queue full");
	}
}

int nthw_flm_inf_queue_get(uint8_t port, bool remote, struct flm_info_event_s *obj)
{
	int ret;

	/* If queues is not created, then ignore and return */
	if (!remote) {
		if (port < MAX_INFO_LCL_QUEUES) {
			if (info_q_local[port] != NULL) {
				ret = rte_ring_sc_dequeue_elem(info_q_local[port],
						obj,
						FLM_EVT_ELEM_SIZE);
				return ret;
			}

			if (flm_evt_queue_create(port, FLM_INFO_LOCAL) != NULL) {
				/* Recursive call to get data */
				return nthw_flm_inf_queue_get(port, remote, obj);
			}
		}

	} else if (port < MAX_INFO_RMT_QUEUES) {
		if (info_q_remote[port] != NULL) {
			ret = rte_ring_sc_dequeue_elem(info_q_remote[port],
					obj,
					FLM_EVT_ELEM_SIZE);
			return ret;
		}

		if (flm_evt_queue_create(port, FLM_INFO_REMOTE) != NULL) {
			/* Recursive call to get data */
			return nthw_flm_inf_queue_get(port, remote, obj);
		}
	}

	return -ENOENT;
}
