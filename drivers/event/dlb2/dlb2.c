/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2022 Intel Corporation
 */

#include <assert.h>
#include <errno.h>
#include <nmmintrin.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <rte_common.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <dev_driver.h>
#include <rte_errno.h>
#include <rte_eventdev.h>
#include <eventdev_pmd.h>
#include <rte_io.h>
#include <rte_kvargs.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_power_intrinsics.h>
#include <rte_prefetch.h>
#include <rte_ring.h>
#include <rte_string_fns.h>

#include "dlb2_priv.h"
#include "dlb2_iface.h"
#include "dlb2_inline_fns.h"

/*
 * Bypass memory fencing instructions when port is of Producer type.
 * This should be enabled very carefully with understanding that producer
 * is not doing any writes which need fencing. The movdir64 instruction used to
 * enqueue events to DLB is a weakly-ordered instruction and movdir64 write
 * to DLB can go ahead of relevant application writes like updates to buffers
 * being sent with event
 */
#ifndef DLB2_BYPASS_FENCE_ON_PP
#define DLB2_BYPASS_FENCE_ON_PP 0  /* 1 == Bypass fence, 0 == do not bypass */
#endif

/*
 * Optimization switches for improving driver performance.
 * WARNING: Do not change any of the below switches without first
 * consulting with DLB2 software development team.
 *
 * HW credit checks can only be turned off for DLB2 device if following
 * is true for each created eventdev
 * LDB credits <= DIR credits + minimum CQ Depth
 * (CQ Depth is minimum of all ports configured within eventdev)
 * This needs to be true for all eventdevs created on any DLB2 device
 * managed by this driver.
 * DLB2.5 does not any such restriction as it has single credit pool
 */
#ifndef DLB_HW_CREDITS_CHECKS
#define DLB_HW_CREDITS_CHECKS 0
#endif

/*
 * SW credit checks can only be turned off if application has a way to
 * limit input events to the eventdev below assigned credit limit
 */
#ifndef DLB_SW_CREDITS_CHECKS
#define DLB_SW_CREDITS_CHECKS 1
#endif

/*
 * This check can only be disabled if application is not using
 * queues of RTE_EVENT_QUEUE_CFG_ALL_TYPES type.
 * Once such application is fully validated, type check can be turned off.
 * HW will continue checking for correct type and generate alarm on mismatch
 */
#ifndef DLB_TYPE_CHECK
#define DLB_TYPE_CHECK 1
#endif
#define DLB_TYPE_MACRO 0x010002

/*
 * To avoid deadlock situations, by default, per port new_event_threshold
 * check is disabled. nb_events_limit is still checked while allocating
 * new event credits.
 */
#define ENABLE_PORT_THRES_CHECK 1
/*
 * To avoid deadlock, ports holding to credits will release them after these
 * many consecutive zero dequeues
 */
#define DLB2_ZERO_DEQ_CREDIT_RETURN_THRES 16384

/*
 * To avoid deadlock, ports holding to credits will release them after these
 * many consecutive enqueue failures
 */
#define DLB2_ENQ_FAIL_CREDIT_RETURN_THRES 100

/*
 * Resources exposed to eventdev. Some values overridden at runtime using
 * values returned by the DLB kernel driver.
 */
#if (RTE_EVENT_MAX_QUEUES_PER_DEV > UINT8_MAX)
#error "RTE_EVENT_MAX_QUEUES_PER_DEV cannot fit in member max_event_queues"
#endif

static struct rte_event_dev_info evdev_dlb2_default_info = {
	.driver_name = "", /* probe will set */
	.min_dequeue_timeout_ns = DLB2_MIN_DEQUEUE_TIMEOUT_NS,
	.max_dequeue_timeout_ns = DLB2_MAX_DEQUEUE_TIMEOUT_NS,
#if (RTE_EVENT_MAX_QUEUES_PER_DEV < DLB2_MAX_NUM_LDB_QUEUES)
	.max_event_queues = RTE_EVENT_MAX_QUEUES_PER_DEV,
#else
	.max_event_queues = DLB2_MAX_NUM_LDB_QUEUES,
#endif
	.max_event_queue_flows = DLB2_MAX_NUM_FLOWS,
	.max_event_queue_priority_levels = DLB2_QID_PRIORITIES,
	.max_event_priority_levels = DLB2_QID_PRIORITIES,
	.max_event_ports = DLB2_MAX_NUM_LDB_PORTS,
	.max_event_port_dequeue_depth = DLB2_DEFAULT_CQ_DEPTH,
	.max_event_port_enqueue_depth = DLB2_MAX_ENQUEUE_DEPTH,
	.max_event_port_links = DLB2_MAX_NUM_QIDS_PER_LDB_CQ,
	.max_num_events = DLB2_MAX_NUM_LDB_CREDITS,
	.max_single_link_event_port_queue_pairs =
		DLB2_MAX_NUM_DIR_PORTS(DLB2_HW_V2),
	.event_dev_cap = (RTE_EVENT_DEV_CAP_ATOMIC |
			  RTE_EVENT_DEV_CAP_ORDERED |
			  RTE_EVENT_DEV_CAP_PARALLEL |
			  RTE_EVENT_DEV_CAP_EVENT_QOS |
			  RTE_EVENT_DEV_CAP_NONSEQ_MODE |
			  RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |
			  RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES |
			  RTE_EVENT_DEV_CAP_BURST_MODE |
			  RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE |
			  RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK |
			  RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT |
			  RTE_EVENT_DEV_CAP_INDEPENDENT_ENQ |
			  RTE_EVENT_DEV_CAP_MAINTENANCE_FREE),
	.max_profiles_per_port = 1,
};

struct process_local_port_data
dlb2_port[DLB2_MAX_NUM_PORTS_ALL][DLB2_NUM_PORT_TYPES];

static void
dlb2_free_qe_mem(struct dlb2_port *qm_port)
{
	if (qm_port == NULL)
		return;

	rte_free(qm_port->qe4);
	qm_port->qe4 = NULL;

	if (qm_port->order) {
		rte_free(qm_port->order);
		qm_port->order = NULL;
	}

	rte_free(qm_port->int_arm_qe);
	qm_port->int_arm_qe = NULL;

	rte_free(qm_port->consume_qe);
	qm_port->consume_qe = NULL;

	rte_memzone_free(dlb2_port[qm_port->id][PORT_TYPE(qm_port)].mz);
	dlb2_port[qm_port->id][PORT_TYPE(qm_port)].mz = NULL;
}

/* override defaults with value(s) provided on command line */
static void
dlb2_init_queue_depth_thresholds(struct dlb2_eventdev *dlb2,
				 int *qid_depth_thresholds)
{
	int q;

	for (q = 0; q < DLB2_MAX_NUM_QUEUES(dlb2->version); q++) {
		if (qid_depth_thresholds[q] != 0)
			dlb2->ev_queues[q].depth_threshold =
				qid_depth_thresholds[q];
	}
}

/* override defaults with value(s) provided on command line */
static void
dlb2_init_port_cos(struct dlb2_eventdev *dlb2, int *port_cos)
{
	int q;

	for (q = 0; q < DLB2_MAX_NUM_PORTS_ALL; q++) {
		dlb2->ev_ports[q].cos_id = port_cos[q];
		if (port_cos[q] != DLB2_COS_DEFAULT &&
		    dlb2->cos_ports[port_cos[q]] < DLB2_MAX_NUM_LDB_PORTS_PER_COS)
			dlb2->cos_ports[port_cos[q]]++;
	}
}

static void
dlb2_init_cos_bw(struct dlb2_eventdev *dlb2,
		 struct dlb2_cos_bw *cos_bw)
{
	int q;


	/* If cos_bw not set, then split evenly */
	if (cos_bw->val[0] == 0 && cos_bw->val[1] == 0 &&
		cos_bw->val[2] == 0 && cos_bw->val[3] == 0) {
		cos_bw->val[0] = 25;
		cos_bw->val[1] = 25;
		cos_bw->val[2] = 25;
		cos_bw->val[3] = 25;
	}

	for (q = 0; q < DLB2_COS_NUM_VALS; q++)
		dlb2->cos_bw[q] = cos_bw->val[q];

}

static int
dlb2_hw_query_resources(struct dlb2_eventdev *dlb2)
{
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	int num_ldb_ports;
	int ret;

	/* Query driver resources provisioned for this device */

	ret = dlb2_iface_get_num_resources(handle,
					   &dlb2->hw_rsrc_query_results);
	if (ret) {
		DLB2_LOG_ERR("ioctl get dlb2 num resources, err=%d", ret);
		return ret;
	}

	/* Complete filling in device resource info returned to evdev app,
	 * overriding any default values.
	 * The capabilities (CAPs) were set at compile time.
	 */

	num_ldb_ports = dlb2->hw_rsrc_query_results.num_ldb_ports;

	evdev_dlb2_default_info.max_event_queues =
		dlb2->hw_rsrc_query_results.num_ldb_queues;

	evdev_dlb2_default_info.max_event_ports = num_ldb_ports;

	evdev_dlb2_default_info.max_single_link_event_port_queue_pairs =
		dlb2->hw_rsrc_query_results.num_dir_ports;

	if (dlb2->version == DLB2_HW_V2_5) {
		evdev_dlb2_default_info.max_num_events =
			dlb2->hw_rsrc_query_results.num_credits;
	} else {
		evdev_dlb2_default_info.max_num_events =
			dlb2->hw_rsrc_query_results.num_ldb_credits;
	}
	/* Save off values used when creating the scheduling domain. */

	handle->info.num_sched_domains =
		dlb2->hw_rsrc_query_results.num_sched_domains;

	if (dlb2->version == DLB2_HW_V2_5) {
		handle->info.hw_rsrc_max.nb_events_limit =
			dlb2->hw_rsrc_query_results.num_credits;
	} else {
		handle->info.hw_rsrc_max.nb_events_limit =
			dlb2->hw_rsrc_query_results.num_ldb_credits;
	}
	handle->info.hw_rsrc_max.num_queues =
		dlb2->hw_rsrc_query_results.num_ldb_queues +
		dlb2->hw_rsrc_query_results.num_dir_ports;

	handle->info.hw_rsrc_max.num_ldb_queues =
		dlb2->hw_rsrc_query_results.num_ldb_queues;

	handle->info.hw_rsrc_max.num_ldb_ports = num_ldb_ports;

	handle->info.hw_rsrc_max.num_dir_ports =
		dlb2->hw_rsrc_query_results.num_dir_ports;

	handle->info.hw_rsrc_max.reorder_window_size =
		dlb2->hw_rsrc_query_results.num_hist_list_entries;

	return 0;
}

#define DLB2_BASE_10 10

static int
dlb2_string_to_int(int *result, const char *str)
{
	long ret;
	char *endptr;

	if (str == NULL || result == NULL)
		return -EINVAL;

	errno = 0;
	ret = strtol(str, &endptr, DLB2_BASE_10);
	if (errno)
		return -errno;

	/* long int and int may be different width for some architectures */
	if (ret < INT_MIN || ret > INT_MAX || endptr == str)
		return -EINVAL;

	*result = ret;
	return 0;
}

static int
set_producer_coremask(const char *key __rte_unused,
		      const char *value,
		      void *opaque)
{
	const char **mask_str = opaque;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	*mask_str = value;

	return 0;
}

static int
set_numa_node(const char *key __rte_unused, const char *value, void *opaque)
{
	int *socket_id = opaque;
	int ret;

	ret = dlb2_string_to_int(socket_id, value);
	if (ret < 0)
		return ret;

	if (*socket_id > RTE_MAX_NUMA_NODES)
		return -EINVAL;
	return 0;
}


static int
set_max_cq_depth(const char *key __rte_unused,
		 const char *value,
		 void *opaque)
{
	int *max_cq_depth = opaque;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(max_cq_depth, value);
	if (ret < 0)
		return ret;

	if (*max_cq_depth < DLB2_MIN_CQ_DEPTH_OVERRIDE ||
	    *max_cq_depth > DLB2_MAX_CQ_DEPTH_OVERRIDE ||
	    !rte_is_power_of_2(*max_cq_depth)) {
		DLB2_LOG_ERR("dlb2: Allowed max_cq_depth range %d - %d and should be power of 2",
			     DLB2_MIN_CQ_DEPTH_OVERRIDE,
			     DLB2_MAX_CQ_DEPTH_OVERRIDE);
		return -EINVAL;
	}

	return 0;
}

static int
set_max_enq_depth(const char *key __rte_unused,
		  const char *value,
		  void *opaque)
{
	int *max_enq_depth = opaque;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(max_enq_depth, value);
	if (ret < 0)
		return ret;

	if (*max_enq_depth < DLB2_MIN_ENQ_DEPTH_OVERRIDE ||
	    *max_enq_depth > DLB2_MAX_ENQ_DEPTH_OVERRIDE ||
	    !rte_is_power_of_2(*max_enq_depth)) {
		DLB2_LOG_ERR("dlb2: max_enq_depth %d and %d and a power of 2",
		DLB2_MIN_ENQ_DEPTH_OVERRIDE,
		DLB2_MAX_ENQ_DEPTH_OVERRIDE);
		return -EINVAL;
	}

	return 0;
}

static int
set_max_num_events(const char *key __rte_unused,
		   const char *value,
		   void *opaque)
{
	int *max_num_events = opaque;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(max_num_events, value);
	if (ret < 0)
		return ret;

	if (*max_num_events < 0 || *max_num_events >
			DLB2_MAX_NUM_LDB_CREDITS) {
		DLB2_LOG_ERR("dlb2: max_num_events must be between 0 and %d",
			     DLB2_MAX_NUM_LDB_CREDITS);
		return -EINVAL;
	}

	return 0;
}

static int
set_max_num_events_v2_5(const char *key __rte_unused,
			const char *value,
			void *opaque)
{
	int *max_num_events = opaque;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(max_num_events, value);
	if (ret < 0)
		return ret;

	if (*max_num_events < 0 || *max_num_events >
			DLB2_MAX_NUM_CREDITS(DLB2_HW_V2_5)) {
		DLB2_LOG_ERR("dlb2: max_num_events must be between 0 and %d",
			     DLB2_MAX_NUM_CREDITS(DLB2_HW_V2_5));
		return -EINVAL;
	}

	return 0;
}

static int
set_num_dir_credits(const char *key __rte_unused,
		    const char *value,
		    void *opaque)
{
	int *num_dir_credits = opaque;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(num_dir_credits, value);
	if (ret < 0)
		return ret;

	if (*num_dir_credits < 0 ||
	    *num_dir_credits > DLB2_MAX_NUM_DIR_CREDITS(DLB2_HW_V2)) {
		DLB2_LOG_ERR("dlb2: num_dir_credits must be between 0 and %d",
			     DLB2_MAX_NUM_DIR_CREDITS(DLB2_HW_V2));
		return -EINVAL;
	}

	return 0;
}

static int
set_dev_id(const char *key __rte_unused,
	   const char *value,
	   void *opaque)
{
	int *dev_id = opaque;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(dev_id, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int
set_poll_interval(const char *key __rte_unused,
	const char *value,
	void *opaque)
{
	int *poll_interval = opaque;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(poll_interval, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int
set_port_cos(const char *key __rte_unused,
	     const char *value,
	     void *opaque)
{
	int first, last, cos_id, i, ports_per_cos[DLB2_COS_NUM_VALS] = {0};
	struct dlb2_port_cos *port_cos = opaque;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	/* command line override may take one of the following 3 forms:
	 * port_cos=port-port:<cos_id> ... a range of ports
	 * port_cos=port:<cos_id> ... just one port
	 */
	if (sscanf(value, "%d-%d:%d", &first, &last, &cos_id) == 3) {
		/* we have everything we need */
	} else if (sscanf(value, "%d:%d", &first, &cos_id) == 2) {
		last = first;
	} else {
		DLB2_LOG_ERR("Error parsing ldb port port_cos devarg. Should be port-port:val, or port:val");
		return -EINVAL;
	}

	if (first > last || first < 0 ||
		last >= DLB2_MAX_NUM_LDB_PORTS) {
		DLB2_LOG_ERR("Error parsing ldb port cos_id arg, invalid port value");
		return -EINVAL;
	}

	if (cos_id < DLB2_COS_0 || cos_id > DLB2_COS_3) {
		DLB2_LOG_ERR("Error parsing ldb port cos_id devarg, must be between 0 and 4");
		return -EINVAL;
	}

	for (i = first; i <= last; i++)
		port_cos->cos_id[i] = cos_id; /* indexed by port */

	for (i = 0; i < DLB2_MAX_NUM_PORTS_ALL; i++)
		if (port_cos->cos_id[i] != DLB2_COS_DEFAULT &&
			++ports_per_cos[port_cos->cos_id[i]] > DLB2_MAX_NUM_LDB_PORTS_PER_COS) {
			DLB2_LOG_ERR("Error parsing ldb port cos_id devarg: More than 16 ports for "
				"cos_id %d.", port_cos->cos_id[i]);
			return -EINVAL;
		}

	return 0;
}

static int
set_cos_bw(const char *key __rte_unused,
	     const char *value,
	     void *opaque)
{
	struct dlb2_cos_bw *cos_bw = opaque;

	if (opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	/* format must be %d,%d,%d,%d */

	if (sscanf(value, "%d:%d:%d:%d", &cos_bw->val[0], &cos_bw->val[1],
		   &cos_bw->val[2], &cos_bw->val[3]) != 4) {
		DLB2_LOG_ERR("Error parsing cos bandwidth devarg. Should be bw0:bw1:bw2:bw3 where all values combined are <= 100");
		return -EINVAL;
	}
	if (cos_bw->val[0] + cos_bw->val[1] + cos_bw->val[2] + cos_bw->val[3] > 100) {
		DLB2_LOG_ERR("Error parsing cos bandwidth devarg. Should be bw0:bw1:bw2:bw3  where all values combined are <= 100");
		return -EINVAL;
	}

	return 0;
}

static int
set_sw_credit_quanta(const char *key __rte_unused,
	const char *value,
	void *opaque)
{
	int *sw_credit_quanta = opaque;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(sw_credit_quanta, value);
	if (ret < 0)
		return ret;

	if (*sw_credit_quanta <= 0) {
		DLB2_LOG_ERR("sw_credit_quanta must be > 0");
		return -EINVAL;
	}

	return 0;
}

static int
set_hw_credit_quanta(const char *key __rte_unused,
	const char *value,
	void *opaque)
{
	int *hw_credit_quanta = opaque;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(hw_credit_quanta, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int
set_default_depth_thresh(const char *key __rte_unused,
	const char *value,
	void *opaque)
{
	int *default_depth_thresh = opaque;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(default_depth_thresh, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int
set_vector_opts_enab(const char *key __rte_unused,
	const char *value,
	void *opaque)
{
	bool *dlb2_vector_opts_enabled = opaque;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	if ((*value == 'y') || (*value == 'Y'))
		*dlb2_vector_opts_enabled = true;
	else
		*dlb2_vector_opts_enabled = false;

	return 0;
}

static int
set_default_ldb_port_allocation(const char *key __rte_unused,
		      const char *value,
		      void *opaque)
{
	bool *default_ldb_port_allocation = opaque;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	if ((*value == 'y') || (*value == 'Y'))
		*default_ldb_port_allocation = true;
	else
		*default_ldb_port_allocation = false;

	return 0;
}

static int
set_enable_cq_weight(const char *key __rte_unused,
		      const char *value,
		      void *opaque)
{
	bool *enable_cq_weight = opaque;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	if ((*value == 'y') || (*value == 'Y'))
		*enable_cq_weight = true;
	else
		*enable_cq_weight = false;

	return 0;
}

static int set_hl_override(const char *key __rte_unused, const char *value,
			   void *opaque)
{
	bool *default_hl = opaque;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	if ((*value == 'n') || (*value == 'N') || (*value == '0'))
		*default_hl = false;
	else
		*default_hl = true;

	return 0;
}

static int set_hl_entries(const char *key __rte_unused, const char *value,
			  void *opaque)
{
	int hl_entries = 0;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(&hl_entries, value);
	if (ret < 0)
		return ret;

	if (!hl_entries || (uint32_t)hl_entries > DLB2_MAX_HL_ENTRIES) {
		DLB2_LOG_ERR(
		    "alloc_hl_entries %u out of range, must be in [1 - %d]",
		    hl_entries, DLB2_MAX_HL_ENTRIES);
		return -EINVAL;
	}
	*(uint32_t *)opaque = hl_entries;

	return 0;
}

static int
set_qid_depth_thresh(const char *key __rte_unused,
		     const char *value,
		     void *opaque)
{
	struct dlb2_qid_depth_thresholds *qid_thresh = opaque;
	int first, last, thresh, i;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	/* command line override may take one of the following 3 forms:
	 * qid_depth_thresh=all:<threshold_value> ... all queues
	 * qid_depth_thresh=qidA-qidB:<threshold_value> ... a range of queues
	 * qid_depth_thresh=qid:<threshold_value> ... just one queue
	 */
	if (sscanf(value, "all:%d", &thresh) == 1) {
		first = 0;
		last = DLB2_MAX_NUM_QUEUES(DLB2_HW_V2) - 1;
	} else if (sscanf(value, "%d-%d:%d", &first, &last, &thresh) == 3) {
		/* we have everything we need */
	} else if (sscanf(value, "%d:%d", &first, &thresh) == 2) {
		last = first;
	} else {
		DLB2_LOG_ERR("Error parsing qid depth devarg. Should be all:val, qid-qid:val, or qid:val");
		return -EINVAL;
	}

	if (first > last || first < 0 ||
		last >= DLB2_MAX_NUM_QUEUES(DLB2_HW_V2)) {
		DLB2_LOG_ERR("Error parsing qid depth devarg, invalid qid value");
		return -EINVAL;
	}

	if (thresh < 0 || thresh > DLB2_MAX_QUEUE_DEPTH_THRESHOLD) {
		DLB2_LOG_ERR("Error parsing qid depth devarg, threshold > %d",
			     DLB2_MAX_QUEUE_DEPTH_THRESHOLD);
		return -EINVAL;
	}

	for (i = first; i <= last; i++)
		qid_thresh->val[i] = thresh; /* indexed by qid */

	return 0;
}

static int
set_qid_depth_thresh_v2_5(const char *key __rte_unused,
			  const char *value,
			  void *opaque)
{
	struct dlb2_qid_depth_thresholds *qid_thresh = opaque;
	int first, last, thresh, i;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer");
		return -EINVAL;
	}

	/* command line override may take one of the following 3 forms:
	 * qid_depth_thresh=all:<threshold_value> ... all queues
	 * qid_depth_thresh=qidA-qidB:<threshold_value> ... a range of queues
	 * qid_depth_thresh=qid:<threshold_value> ... just one queue
	 */
	if (sscanf(value, "all:%d", &thresh) == 1) {
		first = 0;
		last = DLB2_MAX_NUM_QUEUES(DLB2_HW_V2_5) - 1;
	} else if (sscanf(value, "%d-%d:%d", &first, &last, &thresh) == 3) {
		/* we have everything we need */
	} else if (sscanf(value, "%d:%d", &first, &thresh) == 2) {
		last = first;
	} else {
		DLB2_LOG_ERR("Error parsing qid depth devarg. Should be all:val, qid-qid:val, or qid:val");
		return -EINVAL;
	}

	if (first > last || first < 0 ||
		last >= DLB2_MAX_NUM_QUEUES(DLB2_HW_V2_5)) {
		DLB2_LOG_ERR("Error parsing qid depth devarg, invalid qid value");
		return -EINVAL;
	}

	if (thresh < 0 || thresh > DLB2_MAX_QUEUE_DEPTH_THRESHOLD) {
		DLB2_LOG_ERR("Error parsing qid depth devarg, threshold > %d",
			     DLB2_MAX_QUEUE_DEPTH_THRESHOLD);
		return -EINVAL;
	}

	for (i = first; i <= last; i++)
		qid_thresh->val[i] = thresh; /* indexed by qid */

	return 0;
}

static void
dlb2_eventdev_info_get(struct rte_eventdev *dev,
		       struct rte_event_dev_info *dev_info)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	int ret;

	ret = dlb2_hw_query_resources(dlb2);
	if (ret) {
		const struct rte_eventdev_data *data = dev->data;

		DLB2_LOG_ERR("get resources err=%d, devid=%d",
			     ret, data->dev_id);
		/* fn is void, so fall through and return values set up in
		 * probe
		 */
	}

	/* Add num resources currently owned by this domain.
	 * These would become available if the scheduling domain were reset due
	 * to the application recalling eventdev_configure to *reconfigure* the
	 * domain.
	 */
	evdev_dlb2_default_info.max_event_ports += dlb2->num_ldb_ports;
	evdev_dlb2_default_info.max_event_queues += dlb2->num_ldb_queues;
	if (dlb2->version == DLB2_HW_V2_5) {
		evdev_dlb2_default_info.max_num_events +=
			dlb2->max_credits;
	} else {
		evdev_dlb2_default_info.max_num_events +=
			dlb2->max_ldb_credits;
	}
	evdev_dlb2_default_info.max_event_queues =
		RTE_MIN(evdev_dlb2_default_info.max_event_queues,
			RTE_EVENT_MAX_QUEUES_PER_DEV);

	evdev_dlb2_default_info.max_num_events =
		RTE_MIN(evdev_dlb2_default_info.max_num_events,
			dlb2->max_num_events_override);

	*dev_info = evdev_dlb2_default_info;
}

static int
dlb2_hw_create_sched_domain(struct dlb2_eventdev *dlb2,
			    struct dlb2_hw_dev *handle,
			    const struct dlb2_hw_rsrcs *resources_asked,
			    uint8_t device_version)
{
	uint32_t total_asked_ports;
	struct dlb2_create_sched_domain_args *cfg;
	uint32_t cos_ports = 0, max_cos_port = 0;
	int ret = 0;

	if (resources_asked == NULL) {
		DLB2_LOG_ERR("dlb2: dlb2_create NULL parameter");
		ret = EINVAL;
		goto error_exit;
	}

	total_asked_ports = resources_asked->num_ldb_ports + resources_asked->num_dir_ports;

	/* Map generic qm resources to dlb2 resources */
	cfg = &handle->cfg.resources;

	/* DIR ports and queues */

	cfg->num_dir_ports = resources_asked->num_dir_ports;
	if (device_version == DLB2_HW_V2_5)
		cfg->num_credits = resources_asked->num_credits;
	else
		cfg->num_dir_credits = resources_asked->num_dir_credits;

	/* LDB queues */

	cfg->num_ldb_queues = resources_asked->num_ldb_queues;

	/* LDB ports */

	/* tally of COS ports from cmd line */
	cos_ports = dlb2->cos_ports[0] + dlb2->cos_ports[1] +
		    dlb2->cos_ports[2] + dlb2->cos_ports[3];

	for (int i = 0; i < DLB2_MAX_NUM_PORTS_ALL; i++) {
		if (dlb2->ev_ports[i].cos_id != DLB2_COS_DEFAULT)
			max_cos_port = i;
	}

	if (cos_ports > resources_asked->num_ldb_ports || max_cos_port >= total_asked_ports) {
		DLB2_LOG_ERR("dlb2: Insufficient num_ldb_ports=%d: cos_ports=%d max_cos_port=%d",
			resources_asked->num_ldb_ports, cos_ports, max_cos_port);
		ret = EINVAL;
		goto error_exit;
	}

	cfg->cos_strict = 0; /* Best effort */
	cfg->num_cos_ldb_ports[0] = dlb2->cos_ports[0];
	cfg->num_cos_ldb_ports[1] = dlb2->cos_ports[1];
	cfg->num_cos_ldb_ports[2] = dlb2->cos_ports[2];
	cfg->num_cos_ldb_ports[3] = dlb2->cos_ports[3];
	cfg->num_ldb_ports = resources_asked->num_ldb_ports - cos_ports;

	if (device_version == DLB2_HW_V2)
		cfg->num_ldb_credits = resources_asked->num_ldb_credits;

	cfg->num_atomic_inflights =
		DLB2_NUM_ATOMIC_INFLIGHTS_PER_QUEUE *
		cfg->num_ldb_queues;

	/* If hl_entries is non-zero then user specified command line option.
	 * Else compute using default_port_hl that has been set earlier based
	 * on use_default_hl option
	 */
	if (dlb2->hl_entries) {
		cfg->num_hist_list_entries = dlb2->hl_entries;
	} else {
		cfg->num_hist_list_entries =
			resources_asked->num_ldb_ports * dlb2->default_port_hl;
	}

	if (device_version == DLB2_HW_V2_5) {
		DLB2_LOG_LINE_DBG("sched domain create - ldb_qs=%d, ldb_ports=%d, dir_ports=%d, atomic_inflights=%d, hist_list_entries=%d, credits=%d",
			     cfg->num_ldb_queues,
			     resources_asked->num_ldb_ports,
			     cfg->num_dir_ports,
			     cfg->num_atomic_inflights,
			     cfg->num_hist_list_entries,
			     cfg->num_credits);
	} else {
		DLB2_LOG_LINE_DBG("sched domain create - ldb_qs=%d, ldb_ports=%d, dir_ports=%d, atomic_inflights=%d, hist_list_entries=%d, ldb_credits=%d, dir_credits=%d",
			     cfg->num_ldb_queues,
			     resources_asked->num_ldb_ports,
			     cfg->num_dir_ports,
			     cfg->num_atomic_inflights,
			     cfg->num_hist_list_entries,
			     cfg->num_ldb_credits,
			     cfg->num_dir_credits);
	}

	/* Configure the QM */

	ret = dlb2_iface_sched_domain_create(handle, cfg);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: domain create failed, ret = %d, extra status: %s",
			     ret,
			     dlb2_error_strings[cfg->response.status]);

		goto error_exit;
	}

	handle->domain_id = cfg->response.id;
	handle->cfg.configured = true;

error_exit:

	return ret;
}

static void
dlb2_hw_reset_sched_domain(const struct rte_eventdev *dev, bool reconfig)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	enum dlb2_configuration_state config_state;
	int i, j;

	dlb2_iface_domain_reset(dlb2);

	/* Free all dynamically allocated port memory */
	for (i = 0; i < dlb2->num_ports; i++)
		dlb2_free_qe_mem(&dlb2->ev_ports[i].qm_port);

	/* If reconfiguring, mark the device's queues and ports as "previously
	 * configured." If the user doesn't reconfigure them, the PMD will
	 * reapply their previous configuration when the device is started.
	 */
	config_state = (reconfig) ? DLB2_PREV_CONFIGURED :
		DLB2_NOT_CONFIGURED;

	for (i = 0; i < dlb2->num_ports; i++) {
		dlb2->ev_ports[i].qm_port.config_state = config_state;
		/* Reset setup_done so ports can be reconfigured */
		dlb2->ev_ports[i].setup_done = false;
		for (j = 0; j < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; j++)
			dlb2->ev_ports[i].link[j].mapped = false;
	}

	for (i = 0; i < dlb2->num_queues; i++)
		dlb2->ev_queues[i].qm_queue.config_state = config_state;

	for (i = 0; i < DLB2_MAX_NUM_QUEUES(DLB2_HW_V2_5); i++)
		dlb2->ev_queues[i].setup_done = false;

	dlb2->num_ports = 0;
	dlb2->num_ldb_ports = 0;
	dlb2->num_dir_ports = 0;
	dlb2->num_queues = 0;
	dlb2->num_ldb_queues = 0;
	dlb2->num_dir_queues = 0;
	if (dlb2->version == DLB2_HW_V2_5) {
		dlb2->num_credits = 0;
		dlb2->max_credits = 0;
	} else {
		dlb2->num_ldb_credits = 0;
		dlb2->num_dir_credits = 0;
		dlb2->max_ldb_credits = 0;
		dlb2->max_dir_credits = 0;
	}
	dlb2->configured = false;
}

/* Note: 1 QM instance per QM device, QM instance/device == event device */
static int
dlb2_eventdev_configure(const struct rte_eventdev *dev)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	struct dlb2_hw_rsrcs *rsrcs = &handle->info.hw_rsrc_max;
	const struct rte_eventdev_data *data = dev->data;
	const struct rte_event_dev_config *config = &data->dev_conf;
	int ret;

	/* If this eventdev is already configured, we must release the current
	 * scheduling domain before attempting to configure a new one.
	 */
	if (dlb2->configured) {
		dlb2_hw_reset_sched_domain(dev, true);
		ret = dlb2_hw_query_resources(dlb2);
		if (ret) {
			DLB2_LOG_ERR("get resources err=%d, devid=%d",
				     ret, data->dev_id);
			return ret;
		}
	}

	if (config->nb_event_queues > rsrcs->num_queues) {
		DLB2_LOG_ERR("nb_event_queues parameter (%d) exceeds the QM device's capabilities (%d).",
			     config->nb_event_queues,
			     rsrcs->num_queues);
		return -EINVAL;
	}
	if (config->nb_event_ports > (rsrcs->num_ldb_ports
			+ rsrcs->num_dir_ports)) {
		DLB2_LOG_ERR("nb_event_ports parameter (%d) exceeds the QM device's capabilities (%d).",
			     config->nb_event_ports,
			     (rsrcs->num_ldb_ports + rsrcs->num_dir_ports));
		return -EINVAL;
	}
	if (config->nb_events_limit > rsrcs->nb_events_limit) {
		DLB2_LOG_ERR("nb_events_limit parameter (%d) exceeds the QM device's capabilities (%d).",
			     config->nb_events_limit,
			     rsrcs->nb_events_limit);
		return -EINVAL;
	}

	if (config->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT)
		dlb2->global_dequeue_wait = false;
	else {
		uint32_t timeout32;

		dlb2->global_dequeue_wait = true;

		/* note size mismatch of timeout vals in eventdev lib. */
		timeout32 = config->dequeue_timeout_ns;

		dlb2->global_dequeue_wait_ticks =
			timeout32 * (rte_get_timer_hz() / 1E9);
	}

	/* Does this platform support umonitor/umwait? */
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_WAITPKG))
		dlb2->umwait_allowed = true;

	rsrcs->num_dir_ports = config->nb_single_link_event_port_queues;
	rsrcs->num_ldb_ports  = config->nb_event_ports - rsrcs->num_dir_ports;
	/* 1 dir queue per dir port */
	rsrcs->num_ldb_queues = config->nb_event_queues - rsrcs->num_dir_ports;

	if (dlb2->version == DLB2_HW_V2_5) {
		rsrcs->num_credits = 0;
		if (rsrcs->num_ldb_queues || rsrcs->num_dir_ports)
			rsrcs->num_credits = config->nb_events_limit;
	} else {
		/* Scale down nb_events_limit by 4 for directed credits,
		 * since there are 4x as many load-balanced credits.
		 */
		rsrcs->num_ldb_credits = 0;
		rsrcs->num_dir_credits = 0;

		if (rsrcs->num_ldb_queues)
			rsrcs->num_ldb_credits = config->nb_events_limit;
		if (rsrcs->num_dir_ports)
			rsrcs->num_dir_credits = config->nb_events_limit / 2;
		if (dlb2->num_dir_credits_override != -1)
			rsrcs->num_dir_credits = dlb2->num_dir_credits_override;
	}

	if (dlb2_hw_create_sched_domain(dlb2, handle, rsrcs,
					dlb2->version) < 0) {
		DLB2_LOG_ERR("dlb2_hw_create_sched_domain failed");
		return -ENODEV;
	}

	dlb2->new_event_limit = config->nb_events_limit;
	rte_atomic_store_explicit(&dlb2->inflights, 0, rte_memory_order_seq_cst);

	/* Save number of ports/queues for this event dev */
	dlb2->num_ports = config->nb_event_ports;
	dlb2->num_queues = config->nb_event_queues;
	dlb2->num_dir_ports = rsrcs->num_dir_ports;
	dlb2->num_ldb_ports = dlb2->num_ports - dlb2->num_dir_ports;
	dlb2->num_ldb_queues = dlb2->num_queues - dlb2->num_dir_ports;
	dlb2->num_dir_queues = dlb2->num_dir_ports;
	if (dlb2->version == DLB2_HW_V2_5) {
		dlb2->credit_pool = rsrcs->num_credits;
		dlb2->max_credits = rsrcs->num_credits;
		dlb2->num_credits = rsrcs->num_credits;
	} else {
		dlb2->ldb_credit_pool = rsrcs->num_ldb_credits;
		dlb2->max_ldb_credits = rsrcs->num_ldb_credits;
		dlb2->num_ldb_credits = rsrcs->num_ldb_credits;
		dlb2->dir_credit_pool = rsrcs->num_dir_credits;
		dlb2->max_dir_credits = rsrcs->num_dir_credits;
		dlb2->num_dir_credits = rsrcs->num_dir_credits;
	}

	dlb2->configured = true;

	return 0;
}

static void
dlb2_eventdev_port_default_conf_get(struct rte_eventdev *dev,
				    uint8_t port_id,
				    struct rte_event_port_conf *port_conf)
{
	RTE_SET_USED(port_id);
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);

	port_conf->new_event_threshold = dlb2->new_event_limit;
	port_conf->dequeue_depth = dlb2->default_port_hl / 2;
	port_conf->enqueue_depth = evdev_dlb2_default_info.max_event_port_enqueue_depth;
	port_conf->event_port_cfg = 0;
}

static void
dlb2_eventdev_queue_default_conf_get(struct rte_eventdev *dev,
				     uint8_t queue_id,
				     struct rte_event_queue_conf *queue_conf)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);

	queue_conf->nb_atomic_flows = 1024;
	queue_conf->nb_atomic_order_sequences = 64;
	queue_conf->event_queue_cfg = 0;
	queue_conf->priority = 0;
}

static int32_t
dlb2_get_sn_allocation(struct dlb2_eventdev *dlb2, int group)
{
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	struct dlb2_get_sn_allocation_args cfg;
	int ret;

	cfg.group = group;

	ret = dlb2_iface_get_sn_allocation(handle, &cfg);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: get_sn_allocation ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		return ret;
	}

	return cfg.response.id;
}

static int
dlb2_set_sn_allocation(struct dlb2_eventdev *dlb2, int group, int num)
{
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	struct dlb2_set_sn_allocation_args cfg;
	int ret;

	cfg.num = num;
	cfg.group = group;

	ret = dlb2_iface_set_sn_allocation(handle, &cfg);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: set_sn_allocation ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		return ret;
	}

	return ret;
}

static int32_t
dlb2_get_sn_occupancy(struct dlb2_eventdev *dlb2, int group)
{
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	struct dlb2_get_sn_occupancy_args cfg;
	int ret;

	cfg.group = group;

	ret = dlb2_iface_get_sn_occupancy(handle, &cfg);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: get_sn_occupancy ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		return ret;
	}

	return cfg.response.id;
}

/* Query the current sequence number allocations and, if they conflict with the
 * requested LDB queue configuration, attempt to re-allocate sequence numbers.
 * This is best-effort; if it fails, the PMD will attempt to configure the
 * load-balanced queue and return an error.
 */
static void
dlb2_program_sn_allocation(struct dlb2_eventdev *dlb2,
			   const struct rte_event_queue_conf *queue_conf)
{
	int grp_occupancy[DLB2_NUM_SN_GROUPS];
	int grp_alloc[DLB2_NUM_SN_GROUPS];
	int i, sequence_numbers;

	sequence_numbers = (int)queue_conf->nb_atomic_order_sequences;

	for (i = 0; i < DLB2_NUM_SN_GROUPS; i++) {
		int total_slots;

		grp_alloc[i] = dlb2_get_sn_allocation(dlb2, i);
		if (grp_alloc[i] < 0)
			return;

		total_slots = DLB2_MAX_LDB_SN_ALLOC / grp_alloc[i];

		grp_occupancy[i] = dlb2_get_sn_occupancy(dlb2, i);
		if (grp_occupancy[i] < 0)
			return;

		/* DLB has at least one available slot for the requested
		 * sequence numbers, so no further configuration required.
		 */
		if (grp_alloc[i] == sequence_numbers &&
		    grp_occupancy[i] < total_slots)
			return;
	}

	/* None of the sequence number groups are configured for the requested
	 * sequence numbers, so we have to reconfigure one of them. This is
	 * only possible if a group is not in use.
	 */
	for (i = 0; i < DLB2_NUM_SN_GROUPS; i++) {
		if (grp_occupancy[i] == 0)
			break;
	}

	if (i == DLB2_NUM_SN_GROUPS) {
		DLB2_LOG_ERR("[%s()] No groups with %d sequence_numbers are available or have free slots",
		       __func__, sequence_numbers);
		return;
	}

	/* Attempt to configure slot i with the requested number of sequence
	 * numbers. Ignore the return value -- if this fails, the error will be
	 * caught during subsequent queue configuration.
	 */
	dlb2_set_sn_allocation(dlb2, i, sequence_numbers);
}

static int32_t
dlb2_hw_create_ldb_queue(struct dlb2_eventdev *dlb2,
			 struct dlb2_eventdev_queue *ev_queue,
			 const struct rte_event_queue_conf *evq_conf)
{
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	struct dlb2_queue *queue = &ev_queue->qm_queue;
	struct dlb2_create_ldb_queue_args cfg;
	int32_t ret;
	uint32_t qm_qid;
	int sched_type = -1;

	if (evq_conf == NULL)
		return -EINVAL;

	if (evq_conf->event_queue_cfg & RTE_EVENT_QUEUE_CFG_ALL_TYPES) {
		if (evq_conf->nb_atomic_order_sequences != 0)
			sched_type = RTE_SCHED_TYPE_ORDERED;
		else
			sched_type = RTE_SCHED_TYPE_PARALLEL;
	} else
		sched_type = evq_conf->schedule_type;

	cfg.num_atomic_inflights = DLB2_NUM_ATOMIC_INFLIGHTS_PER_QUEUE;
	cfg.num_sequence_numbers = evq_conf->nb_atomic_order_sequences;
	cfg.num_qid_inflights = evq_conf->nb_atomic_order_sequences;

	if (sched_type != RTE_SCHED_TYPE_ORDERED) {
		cfg.num_sequence_numbers = 0;
		cfg.num_qid_inflights = 2048;
	}

	/* App should set this to the number of hardware flows they want, not
	 * the overall number of flows they're going to use. E.g. if app is
	 * using 64 flows and sets compression to 64, best-case they'll get
	 * 64 unique hashed flows in hardware.
	 */
	switch (evq_conf->nb_atomic_flows) {
	/* Valid DLB2 compression levels */
	case 64:
	case 128:
	case 256:
	case 512:
	case (1 * 1024): /* 1K */
	case (2 * 1024): /* 2K */
	case (4 * 1024): /* 4K */
	case (64 * 1024): /* 64K */
		cfg.lock_id_comp_level = evq_conf->nb_atomic_flows;
		break;
	default:
		/* Invalid compression level */
		cfg.lock_id_comp_level = 0; /* no compression */
	}

	if (ev_queue->depth_threshold == 0) {
		cfg.depth_threshold = dlb2->default_depth_thresh;
		ev_queue->depth_threshold =
			dlb2->default_depth_thresh;
	} else
		cfg.depth_threshold = ev_queue->depth_threshold;

	ret = dlb2_iface_ldb_queue_create(handle, &cfg);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: create LB event queue error, ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		return -EINVAL;
	}

	qm_qid = cfg.response.id;

	/* Save off queue config for debug, resource lookups, and reconfig */
	queue->num_qid_inflights = cfg.num_qid_inflights;
	queue->num_atm_inflights = cfg.num_atomic_inflights;

	queue->sched_type = sched_type;
	queue->config_state = DLB2_CONFIGURED;

	DLB2_LOG_LINE_DBG("Created LB event queue %d, nb_inflights=%d, nb_seq=%d, qid inflights=%d",
		     qm_qid,
		     cfg.num_atomic_inflights,
		     cfg.num_sequence_numbers,
		     cfg.num_qid_inflights);

	return qm_qid;
}

static int
dlb2_eventdev_ldb_queue_setup(struct rte_eventdev *dev,
			      struct dlb2_eventdev_queue *ev_queue,
			      const struct rte_event_queue_conf *queue_conf)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	int32_t qm_qid;

	if (queue_conf->nb_atomic_order_sequences)
		dlb2_program_sn_allocation(dlb2, queue_conf);

	qm_qid = dlb2_hw_create_ldb_queue(dlb2, ev_queue, queue_conf);
	if (qm_qid < 0) {
		DLB2_LOG_ERR("Failed to create the load-balanced queue");

		return qm_qid;
	}

	dlb2->qm_ldb_to_ev_queue_id[qm_qid] = ev_queue->id;

	ev_queue->qm_queue.id = qm_qid;

	return 0;
}

static int dlb2_num_dir_queues_setup(struct dlb2_eventdev *dlb2)
{
	int i, num = 0;

	for (i = 0; i < dlb2->num_queues; i++) {
		if (dlb2->ev_queues[i].setup_done &&
		    dlb2->ev_queues[i].qm_queue.is_directed)
			num++;
	}

	return num;
}

static void
dlb2_queue_link_teardown(struct dlb2_eventdev *dlb2,
			 struct dlb2_eventdev_queue *ev_queue)
{
	struct dlb2_eventdev_port *ev_port;
	int i, j;

	for (i = 0; i < dlb2->num_ports; i++) {
		ev_port = &dlb2->ev_ports[i];

		for (j = 0; j < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; j++) {
			if (!ev_port->link[j].valid ||
			    ev_port->link[j].queue_id != ev_queue->id)
				continue;

			ev_port->link[j].valid = false;
			ev_port->num_links--;
		}
	}

	ev_queue->num_links = 0;
}

static int
dlb2_eventdev_queue_setup(struct rte_eventdev *dev,
			  uint8_t ev_qid,
			  const struct rte_event_queue_conf *queue_conf)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	struct dlb2_eventdev_queue *ev_queue;
	int ret;

	if (queue_conf == NULL)
		return -EINVAL;

	if (ev_qid >= dlb2->num_queues)
		return -EINVAL;

	ev_queue = &dlb2->ev_queues[ev_qid];

	ev_queue->qm_queue.is_directed = queue_conf->event_queue_cfg &
		RTE_EVENT_QUEUE_CFG_SINGLE_LINK;
	ev_queue->id = ev_qid;
	ev_queue->conf = *queue_conf;

	if (!ev_queue->qm_queue.is_directed) {
		ret = dlb2_eventdev_ldb_queue_setup(dev, ev_queue, queue_conf);
	} else {
		/* The directed queue isn't setup until link time, at which
		 * point we know its directed port ID. Directed queue setup
		 * will only fail if this queue is already setup or there are
		 * no directed queues left to configure.
		 */
		ret = 0;

		ev_queue->qm_queue.config_state = DLB2_NOT_CONFIGURED;

		if (ev_queue->setup_done ||
		    dlb2_num_dir_queues_setup(dlb2) == dlb2->num_dir_queues)
			ret = -EINVAL;
	}

	/* Tear down pre-existing port->queue links */
	if (!ret && dlb2->run_state == DLB2_RUN_STATE_STOPPED)
		dlb2_queue_link_teardown(dlb2, ev_queue);

	if (!ret)
		ev_queue->setup_done = true;

	return ret;
}

static int
dlb2_init_consume_qe(struct dlb2_port *qm_port, char *mz_name)
{
	struct dlb2_cq_pop_qe *qe;

	qe = rte_zmalloc(mz_name,
			DLB2_NUM_QES_PER_CACHE_LINE *
				sizeof(struct dlb2_cq_pop_qe),
			RTE_CACHE_LINE_SIZE);

	if (qe == NULL)	{
		DLB2_LOG_ERR("dlb2: no memory for consume_qe");
		return -ENOMEM;
	}
	qm_port->consume_qe = qe;

	qe->qe_valid = 0;
	qe->qe_frag = 0;
	qe->qe_comp = 0;
	qe->cq_token = 1;
	/* Tokens value is 0-based; i.e. '0' returns 1 token, '1' returns 2,
	 * and so on.
	 */
	qe->tokens = 0;	/* set at run time */
	qe->meas_lat = 0;
	qe->no_dec = 0;
	/* Completion IDs are disabled */
	qe->cmp_id = 0;

	return 0;
}

static int
dlb2_init_int_arm_qe(struct dlb2_port *qm_port, char *mz_name)
{
	struct dlb2_enqueue_qe *qe;

	qe = rte_zmalloc(mz_name,
			DLB2_NUM_QES_PER_CACHE_LINE *
				sizeof(struct dlb2_enqueue_qe),
			RTE_CACHE_LINE_SIZE);

	if (qe == NULL) {
		DLB2_LOG_ERR("dlb2: no memory for complete_qe");
		return -ENOMEM;
	}
	qm_port->int_arm_qe = qe;

	/* V2 - INT ARM is CQ_TOKEN + FRAG */
	qe->qe_valid = 0;
	qe->qe_frag = 1;
	qe->qe_comp = 0;
	qe->cq_token = 1;
	qe->meas_lat = 0;
	qe->no_dec = 0;
	/* Completion IDs are disabled */
	qe->cmp_id = 0;

	return 0;
}

static int
dlb2_init_qe_mem(struct dlb2_port *qm_port, char *mz_name)
{
	int ret, sz;

	sz = DLB2_NUM_QES_PER_CACHE_LINE * sizeof(struct dlb2_enqueue_qe);

	qm_port->qe4 = rte_zmalloc(mz_name, sz, RTE_CACHE_LINE_SIZE);

	if (qm_port->qe4 == NULL) {
		DLB2_LOG_ERR("dlb2: no qe4 memory");
		ret = -ENOMEM;
		goto error_exit;
	}

	if (qm_port->reorder_en) {
		sz = sizeof(struct dlb2_reorder);
		qm_port->order = rte_zmalloc(mz_name, sz, RTE_CACHE_LINE_SIZE);

		if (qm_port->order == NULL) {
			DLB2_LOG_ERR("dlb2: no reorder memory");
			ret = -ENOMEM;
			goto error_exit;
		}
	}

	ret = dlb2_init_int_arm_qe(qm_port, mz_name);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: dlb2_init_int_arm_qe ret=%d", ret);
		goto error_exit;
	}

	ret = dlb2_init_consume_qe(qm_port, mz_name);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: dlb2_init_consume_qe ret=%d", ret);
		goto error_exit;
	}

	return 0;

error_exit:

	dlb2_free_qe_mem(qm_port);

	return ret;
}

static inline uint16_t
dlb2_event_enqueue_burst_delayed(void *event_port,
				 const struct rte_event events[],
				 uint16_t num);

static inline uint16_t
dlb2_event_enqueue_new_burst_delayed(void *event_port,
				     const struct rte_event events[],
				     uint16_t num);

static inline uint16_t
dlb2_event_enqueue_forward_burst_delayed(void *event_port,
					 const struct rte_event events[],
					 uint16_t num);

/* Generate the required bitmask for rotate-style expected QE gen bits.
 * This requires a pattern of 1's and zeros, starting with expected as
 * 1 bits, so when hardware writes 0's they're "new". This requires the
 * ring size to be powers of 2 to wrap correctly.
 */
static void
dlb2_hw_cq_bitmask_init(struct dlb2_port *qm_port, uint32_t cq_depth)
{
	uint64_t cq_build_mask = 0;
	uint32_t i;

	if (cq_depth > 64)
		return; /* need to fall back to scalar code */

	/*
	 * all 1's in first u64, all zeros in second is correct bit pattern to
	 * start. Special casing == 64 easier than adapting complex loop logic.
	 */
	if (cq_depth == 64) {
		qm_port->cq_rolling_mask = 0;
		qm_port->cq_rolling_mask_2 = -1;
		return;
	}

	for (i = 0; i < 64; i += (cq_depth * 2))
		cq_build_mask |= ((1ULL << cq_depth) - 1) << (i + cq_depth);

	qm_port->cq_rolling_mask = cq_build_mask;
	qm_port->cq_rolling_mask_2 = cq_build_mask;
}

static int
dlb2_hw_create_ldb_port(struct dlb2_eventdev *dlb2,
			struct dlb2_eventdev_port *ev_port,
			uint32_t dequeue_depth,
			uint32_t enqueue_depth)
{
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	struct dlb2_create_ldb_port_args cfg = { {0} };
	struct dlb2_port *qm_port = &ev_port->qm_port;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t qm_port_id;
	int ret;
	RTE_SET_USED(enqueue_depth);

	if (handle == NULL)
		return -EINVAL;

	if (dequeue_depth < DLB2_MIN_CQ_DEPTH) {
		DLB2_LOG_ERR("dlb2: invalid cq depth, must be at least %d",
			     DLB2_MIN_CQ_DEPTH);
		return -EINVAL;
	}

	rte_spinlock_lock(&handle->resource_lock);

	/* We round up to the next power of 2 if necessary */
	cfg.cq_depth = rte_align32pow2(dequeue_depth);
	cfg.cq_depth_threshold = 1;

	if (dlb2->version == DLB2_HW_V2_5 && qm_port->enable_inflight_ctrl) {
		cfg.enable_inflight_ctrl = 1;
		cfg.inflight_threshold = qm_port->inflight_threshold;
	}

	if (qm_port->hist_list)
		cfg.cq_history_list_size = qm_port->hist_list;
	else if (cfg.enable_inflight_ctrl)
		cfg.cq_history_list_size = RTE_MIN(cfg.cq_depth, dlb2->default_port_hl);
	else if (dlb2->default_port_hl == DLB2_FIXED_CQ_HL_SIZE)
		cfg.cq_history_list_size = DLB2_FIXED_CQ_HL_SIZE;
	else
		cfg.cq_history_list_size = cfg.cq_depth * 2;

	cfg.cos_id = ev_port->cos_id;
	cfg.cos_strict = 0;/* best effots */

	/* Per QM values */

	ret = dlb2_iface_ldb_port_create(handle, &cfg,  dlb2->poll_mode);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: dlb2_ldb_port_create error, ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		goto error_exit;
	}

	qm_port_id = cfg.response.id;

	DLB2_LOG_LINE_DBG("dlb2: ev_port %d uses qm LB port %d <<<<<",
		     ev_port->id, qm_port_id);

	qm_port = &ev_port->qm_port;
	qm_port->ev_port = ev_port; /* back ptr */
	qm_port->dlb2 = dlb2; /* back ptr */
	/*
	 * Allocate and init local qe struct(s).
	 * Note: MOVDIR64 requires the enqueue QE (qe4) to be aligned.
	 */

	snprintf(mz_name, sizeof(mz_name), "dlb2_ldb_port%d",
		 ev_port->id);

	ret = dlb2_init_qe_mem(qm_port, mz_name);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: init_qe_mem failed, ret=%d", ret);
		goto error_exit;
	}

	qm_port->id = qm_port_id;

	if (dlb2->version == DLB2_HW_V2) {
		qm_port->cached_ldb_credits = 0;
		qm_port->cached_dir_credits = 0;
	} else
		qm_port->cached_credits = 0;

	if (dlb2->version == DLB2_HW_V2_5 && (dlb2->enable_cq_weight == true)) {
		struct dlb2_enable_cq_weight_args cq_weight_args = { {0} };
		cq_weight_args.port_id = qm_port->id;
		cq_weight_args.limit = dequeue_depth;
		ret = dlb2_iface_enable_cq_weight(handle, &cq_weight_args);

		if (ret < 0) {
			DLB2_LOG_ERR("dlb2: dlb2_dir_port_create error, ret=%d (driver status: %s)",
					ret,
					dlb2_error_strings[cfg.response.  status]);
			goto error_exit;
		}
	}

	/* CQs with depth < 8 use an 8-entry queue, but withhold credits so
	 * the effective depth is smaller.
	 */
	qm_port->cq_depth = cfg.cq_depth <= 8 ? 8 : cfg.cq_depth;
	qm_port->cq_idx = 0;
	qm_port->cq_idx_unmasked = 0;

	if (dlb2->poll_mode == DLB2_CQ_POLL_MODE_SPARSE)
		qm_port->cq_depth_mask = (qm_port->cq_depth * 4) - 1;
	else
		qm_port->cq_depth_mask = qm_port->cq_depth - 1;

	qm_port->gen_bit_shift = rte_popcount32(qm_port->cq_depth_mask);
	/* starting value of gen bit - it toggles at wrap time */
	qm_port->gen_bit = 1;

	dlb2_hw_cq_bitmask_init(qm_port, qm_port->cq_depth);

	qm_port->int_armed = false;

	/* Save off for later use in info and lookup APIs. */
	qm_port->qid_mappings = &dlb2->qm_ldb_to_ev_queue_id[0];

	qm_port->dequeue_depth = dequeue_depth;
	qm_port->token_pop_thresh = dequeue_depth;

	/* The default enqueue functions do not include delayed-pop support for
	 * performance reasons.
	 */
	if (qm_port->token_pop_mode == RTE_PMD_DLB2_DELAYED_POP) {
		dlb2->event_dev->enqueue_burst =
			dlb2_event_enqueue_burst_delayed;
		dlb2->event_dev->enqueue_new_burst =
			dlb2_event_enqueue_new_burst_delayed;
		dlb2->event_dev->enqueue_forward_burst =
			dlb2_event_enqueue_forward_burst_delayed;
	}

	qm_port->owed_tokens = 0;
	qm_port->issued_releases = 0;

	/* Save config message too. */
	rte_memcpy(&qm_port->cfg.ldb, &cfg, sizeof(qm_port->cfg.ldb));

	/* update state */
	qm_port->state = PORT_STARTED; /* enabled at create time */
	qm_port->config_state = DLB2_CONFIGURED;

	if (dlb2->version == DLB2_HW_V2) {
		qm_port->credit_pool[DLB2_DIR_QUEUE] = &dlb2->dir_credit_pool;
		qm_port->credit_pool[DLB2_LDB_QUEUE] = &dlb2->ldb_credit_pool;

		DLB2_LOG_LINE_DBG("dlb2: created ldb port %d, depth = %d",
			     qm_port_id,
			     dequeue_depth);
	} else {
		qm_port->credit_pool[DLB2_COMBINED_POOL] = &dlb2->credit_pool;

		DLB2_LOG_LINE_DBG("dlb2: created ldb port %d, depth = %d",
			     qm_port_id,
			     dequeue_depth);
	}

	qm_port->use_scalar = false;

#if (!defined RTE_ARCH_X86_64)
	qm_port->use_scalar = true;
#else
	if ((qm_port->cq_depth > 64) ||
	    (!rte_is_power_of_2(qm_port->cq_depth)) ||
	    (dlb2->vector_opts_enabled == false))
		qm_port->use_scalar = true;
#endif

	rte_spinlock_unlock(&handle->resource_lock);

	return 0;

error_exit:

	dlb2_free_qe_mem(qm_port);

	rte_spinlock_unlock(&handle->resource_lock);

	DLB2_LOG_ERR("dlb2: create ldb port failed!");

	return ret;
}

static void
dlb2_port_link_teardown(struct dlb2_eventdev *dlb2,
			struct dlb2_eventdev_port *ev_port)
{
	struct dlb2_eventdev_queue *ev_queue;
	int i;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		if (!ev_port->link[i].valid)
			continue;

		ev_queue = &dlb2->ev_queues[ev_port->link[i].queue_id];

		ev_port->link[i].valid = false;
		ev_port->num_links--;
		ev_queue->num_links--;
	}
}

static int
dlb2_hw_create_dir_port(struct dlb2_eventdev *dlb2,
			struct dlb2_eventdev_port *ev_port,
			uint32_t dequeue_depth,
			uint32_t enqueue_depth)
{
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	struct dlb2_create_dir_port_args cfg = { {0} };
	int ret;
	struct dlb2_port *qm_port = NULL;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t qm_port_id;
	uint16_t ldb_credit_high_watermark = 0;
	uint16_t dir_credit_high_watermark = 0;
	uint16_t credit_high_watermark = 0;

	if (dlb2 == NULL || handle == NULL)
		return -EINVAL;

	if (dequeue_depth < DLB2_MIN_CQ_DEPTH) {
		DLB2_LOG_ERR("dlb2: invalid dequeue_depth, must be %d-%d",
			     DLB2_MIN_CQ_DEPTH, DLB2_MAX_INPUT_QUEUE_DEPTH);
		return -EINVAL;
	}

	if (enqueue_depth < DLB2_MIN_ENQUEUE_DEPTH) {
		DLB2_LOG_ERR("dlb2: invalid enqueue_depth, must be at least %d",
			     DLB2_MIN_ENQUEUE_DEPTH);
		return -EINVAL;
	}

	rte_spinlock_lock(&handle->resource_lock);

	/* Directed queues are configured at link time. */
	cfg.queue_id = -1;

	/* We round up to the next power of 2 if necessary */
	cfg.cq_depth = rte_align32pow2(dequeue_depth);
	cfg.cq_depth_threshold = 1;

	/* User controls the LDB high watermark via enqueue depth. The DIR high
	 * watermark is equal, unless the directed credit pool is too small.
	 */
	if (dlb2->version == DLB2_HW_V2) {
		ldb_credit_high_watermark = enqueue_depth;
		/* Don't use enqueue_depth if it would require more directed
		 * credits than are available.
		 */
		dir_credit_high_watermark =
			RTE_MIN(enqueue_depth,
				handle->cfg.num_dir_credits / dlb2->num_ports);
	} else
		credit_high_watermark = enqueue_depth;

	if (ev_port->conf.event_port_cfg & RTE_EVENT_PORT_CFG_HINT_PRODUCER)
		cfg.is_producer = 1;

	/* Per QM values */

	ret = dlb2_iface_dir_port_create(handle, &cfg,  dlb2->poll_mode);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: dlb2_dir_port_create error, ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		goto error_exit;
	}

	qm_port_id = cfg.response.id;

	DLB2_LOG_LINE_DBG("dlb2: ev_port %d uses qm DIR port %d <<<<<",
		     ev_port->id, qm_port_id);

	qm_port = &ev_port->qm_port;
	qm_port->ev_port = ev_port; /* back ptr */
	qm_port->dlb2 = dlb2;  /* back ptr */

	/*
	 * Init local qe struct(s).
	 * Note: MOVDIR64 requires the enqueue QE to be aligned
	 */

	snprintf(mz_name, sizeof(mz_name), "dlb2_dir_port%d",
		 ev_port->id);

	ret = dlb2_init_qe_mem(qm_port, mz_name);

	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: init_qe_mem failed, ret=%d", ret);
		goto error_exit;
	}

	qm_port->id = qm_port_id;

	if (dlb2->version == DLB2_HW_V2) {
		qm_port->cached_ldb_credits = 0;
		qm_port->cached_dir_credits = 0;
	} else
		qm_port->cached_credits = 0;

	/* CQs with depth < 8 use an 8-entry queue, but withhold credits so
	 * the effective depth is smaller.
	 */
	qm_port->cq_depth = cfg.cq_depth <= 8 ? 8 : cfg.cq_depth;
	qm_port->cq_idx = 0;
	qm_port->cq_idx_unmasked = 0;

	if (dlb2->poll_mode == DLB2_CQ_POLL_MODE_SPARSE)
		qm_port->cq_depth_mask = (qm_port->cq_depth * 4) - 1;
	else
		qm_port->cq_depth_mask = qm_port->cq_depth - 1;

	qm_port->gen_bit_shift = rte_popcount32(qm_port->cq_depth_mask);
	/* starting value of gen bit - it toggles at wrap time */
	qm_port->gen_bit = 1;
	dlb2_hw_cq_bitmask_init(qm_port, qm_port->cq_depth);

	qm_port->int_armed = false;

	/* Save off for later use in info and lookup APIs. */
	qm_port->qid_mappings = &dlb2->qm_dir_to_ev_queue_id[0];

	qm_port->dequeue_depth = dequeue_depth;

	/* Directed ports are auto-pop, by default. */
	qm_port->token_pop_mode = RTE_PMD_DLB2_AUTO_POP;
	qm_port->owed_tokens = 0;
	qm_port->issued_releases = 0;

	/* Save config message too. */
	rte_memcpy(&qm_port->cfg.dir, &cfg, sizeof(qm_port->cfg.dir));

	/* update state */
	qm_port->state = PORT_STARTED; /* enabled at create time */
	qm_port->config_state = DLB2_CONFIGURED;

	if (dlb2->version == DLB2_HW_V2) {
		qm_port->dir_credits = dir_credit_high_watermark;
		qm_port->ldb_credits = ldb_credit_high_watermark;
		qm_port->credit_pool[DLB2_DIR_QUEUE] = &dlb2->dir_credit_pool;
		qm_port->credit_pool[DLB2_LDB_QUEUE] = &dlb2->ldb_credit_pool;

		DLB2_LOG_LINE_DBG("dlb2: created dir port %d, depth = %d cr=%d,%d",
			     qm_port_id,
			     dequeue_depth,
			     dir_credit_high_watermark,
			     ldb_credit_high_watermark);
	} else {
		qm_port->credits = credit_high_watermark;
		qm_port->credit_pool[DLB2_COMBINED_POOL] = &dlb2->credit_pool;

		DLB2_LOG_LINE_DBG("dlb2: created dir port %d, depth = %d cr=%d",
			     qm_port_id,
			     dequeue_depth,
			     credit_high_watermark);
	}

#if (!defined RTE_ARCH_X86_64)
	qm_port->use_scalar = true;
#else
	if ((qm_port->cq_depth > 64) ||
	    (!rte_is_power_of_2(qm_port->cq_depth)) ||
	    (dlb2->vector_opts_enabled == false))
		qm_port->use_scalar = true;
#endif

	rte_spinlock_unlock(&handle->resource_lock);

	return 0;

error_exit:

	if (qm_port)
		dlb2_free_qe_mem(qm_port);

	rte_spinlock_unlock(&handle->resource_lock);

	DLB2_LOG_ERR("dlb2: create dir port failed!");

	return ret;
}

static int
dlb2_eventdev_port_setup(struct rte_eventdev *dev,
			 uint8_t ev_port_id,
			 const struct rte_event_port_conf *port_conf)
{
	struct dlb2_eventdev *dlb2;
	struct dlb2_eventdev_port *ev_port;
	uint32_t hw_credit_quanta, sw_credit_quanta;
	int ret;

	if (dev == NULL || port_conf == NULL) {
		DLB2_LOG_ERR("Null parameter");
		return -EINVAL;
	}

	dlb2 = dlb2_pmd_priv(dev);

	if (ev_port_id >= DLB2_MAX_NUM_PORTS(dlb2->version))
		return -EINVAL;

	if (port_conf->dequeue_depth >
		evdev_dlb2_default_info.max_event_port_dequeue_depth ||
	    port_conf->enqueue_depth >
		evdev_dlb2_default_info.max_event_port_enqueue_depth)
		return -EINVAL;

	if ((port_conf->event_port_cfg & RTE_EVENT_PORT_CFG_INDEPENDENT_ENQ) &&
	    port_conf->dequeue_depth > DLB2_MAX_CQ_DEPTH_REORDER) {
		DLB2_LOG_ERR("evport %d: Max dequeue depth supported with reorder is %d",
			     ev_port_id, DLB2_MAX_CQ_DEPTH_REORDER);
		return -EINVAL;
	}

	ev_port = &dlb2->ev_ports[ev_port_id];
	/* configured? */
	if (ev_port->setup_done) {
		DLB2_LOG_ERR("evport %d is already configured", ev_port_id);
		return -EINVAL;
	}

	/* Default for worker ports */
	sw_credit_quanta = dlb2->sw_credit_quanta;
	hw_credit_quanta = dlb2->hw_credit_quanta;

	ev_port->qm_port.is_producer = false;
	ev_port->qm_port.is_directed = port_conf->event_port_cfg &
		RTE_EVENT_PORT_CFG_SINGLE_LINK;

	if (port_conf->event_port_cfg & RTE_EVENT_PORT_CFG_HINT_PRODUCER) {
		/* Producer type ports. Mostly enqueue */
		sw_credit_quanta = DLB2_SW_CREDIT_P_QUANTA_DEFAULT;
		hw_credit_quanta = DLB2_SW_CREDIT_P_BATCH_SZ;
		ev_port->qm_port.is_producer = true;
	}
	if (port_conf->event_port_cfg & RTE_EVENT_PORT_CFG_HINT_CONSUMER) {
		/* Consumer type ports. Mostly dequeue */
		sw_credit_quanta = DLB2_SW_CREDIT_C_QUANTA_DEFAULT;
		hw_credit_quanta = DLB2_SW_CREDIT_C_BATCH_SZ;
	}
	ev_port->credit_update_quanta = sw_credit_quanta;
	ev_port->qm_port.hw_credit_quanta = hw_credit_quanta;

	ev_port->enq_retries = port_conf->enqueue_depth;

	ev_port->qm_port.reorder_id = 0;
	ev_port->qm_port.reorder_en = port_conf->event_port_cfg &
				      RTE_EVENT_PORT_CFG_INDEPENDENT_ENQ;

	/* Save off port config for reconfig */
	ev_port->conf = *port_conf;


	/*
	 * Create port
	 */

	if (!ev_port->qm_port.is_directed) {
		ret = dlb2_hw_create_ldb_port(dlb2,
					      ev_port,
					      port_conf->dequeue_depth,
					      port_conf->enqueue_depth);
		if (ret < 0) {
			DLB2_LOG_ERR("Failed to create the lB port ve portId=%d",
				     ev_port_id);

			return ret;
		}
	} else {
		ret = dlb2_hw_create_dir_port(dlb2,
					      ev_port,
					      port_conf->dequeue_depth,
					      port_conf->enqueue_depth);
		if (ret < 0) {
			DLB2_LOG_ERR("Failed to create the DIR port");
			return ret;
		}
	}

	ev_port->id = ev_port_id;
	ev_port->enq_configured = true;
	ev_port->setup_done = true;
	ev_port->inflight_max = port_conf->new_event_threshold;
	ev_port->implicit_release = !(port_conf->event_port_cfg &
		  RTE_EVENT_PORT_CFG_DISABLE_IMPL_REL);
	ev_port->outstanding_releases = 0;
	ev_port->inflight_credits = 0;
	ev_port->dlb2 = dlb2; /* reverse link */

	/* Default for worker ports */
	sw_credit_quanta = dlb2->sw_credit_quanta;
	hw_credit_quanta = dlb2->hw_credit_quanta;

	if (port_conf->event_port_cfg & RTE_EVENT_PORT_CFG_HINT_PRODUCER) {
		/* Producer type ports. Mostly enqueue */
		sw_credit_quanta = DLB2_SW_CREDIT_P_QUANTA_DEFAULT;
		hw_credit_quanta = DLB2_SW_CREDIT_P_BATCH_SZ;
	}
	if (port_conf->event_port_cfg & RTE_EVENT_PORT_CFG_HINT_CONSUMER) {
		/* Consumer type ports. Mostly dequeue */
		sw_credit_quanta = DLB2_SW_CREDIT_C_QUANTA_DEFAULT;
		hw_credit_quanta = DLB2_SW_CREDIT_C_BATCH_SZ;
	}
	ev_port->credit_update_quanta = sw_credit_quanta;
	ev_port->qm_port.hw_credit_quanta = hw_credit_quanta;


	/* Tear down pre-existing port->queue links */
	if (dlb2->run_state == DLB2_RUN_STATE_STOPPED)
		dlb2_port_link_teardown(dlb2, &dlb2->ev_ports[ev_port_id]);

	dev->data->ports[ev_port_id] = &dlb2->ev_ports[ev_port_id];

	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512VL) &&
	    rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_512)
		ev_port->qm_port.use_avx512 = true;
	else
		ev_port->qm_port.use_avx512 = false;

	return 0;
}

static int16_t
dlb2_hw_map_ldb_qid_to_port(struct dlb2_hw_dev *handle,
			    uint32_t qm_port_id,
			    uint16_t qm_qid,
			    uint8_t priority)
{
	struct dlb2_map_qid_args cfg;
	int32_t ret;

	if (handle == NULL)
		return -EINVAL;

	/* Build message */
	cfg.port_id = qm_port_id;
	cfg.qid = qm_qid;
	cfg.priority = EV_TO_DLB2_PRIO(priority);

	ret = dlb2_iface_map_qid(handle, &cfg);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: map qid error, ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		DLB2_LOG_ERR("dlb2: grp=%d, qm_port=%d, qm_qid=%d prio=%d",
			     handle->domain_id, cfg.port_id,
			     cfg.qid,
			     cfg.priority);
	} else {
		DLB2_LOG_LINE_DBG("dlb2: mapped queue %d to qm_port %d",
			     qm_qid, qm_port_id);
	}

	return ret;
}

static int
dlb2_event_queue_join_ldb(struct dlb2_eventdev *dlb2,
			  struct dlb2_eventdev_port *ev_port,
			  struct dlb2_eventdev_queue *ev_queue,
			  uint8_t priority)
{
	int first_avail = -1;
	int ret, i;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		if (ev_port->link[i].valid) {
			if (ev_port->link[i].queue_id == ev_queue->id &&
			    ev_port->link[i].priority == priority) {
				if (ev_port->link[i].mapped)
					return 0; /* already mapped */
				first_avail = i;
			}
		} else if (first_avail == -1)
			first_avail = i;
	}
	if (first_avail == -1) {
		DLB2_LOG_ERR("dlb2: qm_port %d has no available QID slots.",
			     ev_port->qm_port.id);
		return -EINVAL;
	}

	ret = dlb2_hw_map_ldb_qid_to_port(&dlb2->qm_instance,
					  ev_port->qm_port.id,
					  ev_queue->qm_queue.id,
					  priority);

	if (!ret)
		ev_port->link[first_avail].mapped = true;

	return ret;
}

static int32_t
dlb2_hw_create_dir_queue(struct dlb2_eventdev *dlb2,
			 struct dlb2_eventdev_queue *ev_queue,
			 int32_t qm_port_id)
{
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	struct dlb2_create_dir_queue_args cfg;
	int32_t ret;

	/* The directed port is always configured before its queue */
	cfg.port_id = qm_port_id;

	if (ev_queue->depth_threshold == 0) {
		cfg.depth_threshold = dlb2->default_depth_thresh;
		ev_queue->depth_threshold =
			dlb2->default_depth_thresh;
	} else
		cfg.depth_threshold = ev_queue->depth_threshold;

	ret = dlb2_iface_dir_queue_create(handle, &cfg);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: create DIR event queue error, ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		return -EINVAL;
	}

	return cfg.response.id;
}

static int
dlb2_eventdev_dir_queue_setup(struct dlb2_eventdev *dlb2,
			      struct dlb2_eventdev_queue *ev_queue,
			      struct dlb2_eventdev_port *ev_port)
{
	int32_t qm_qid;

	qm_qid = dlb2_hw_create_dir_queue(dlb2, ev_queue, ev_port->qm_port.id);

	if (qm_qid < 0) {
		DLB2_LOG_ERR("Failed to create the DIR queue");
		return qm_qid;
	}

	dlb2->qm_dir_to_ev_queue_id[qm_qid] = ev_queue->id;

	ev_queue->qm_queue.id = qm_qid;

	return 0;
}

static int
dlb2_do_port_link(struct rte_eventdev *dev,
		  struct dlb2_eventdev_queue *ev_queue,
		  struct dlb2_eventdev_port *ev_port,
		  uint8_t prio)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	int err;

	/* Don't link until start time. */
	if (dlb2->run_state == DLB2_RUN_STATE_STOPPED)
		return 0;

	if (ev_queue->qm_queue.is_directed)
		err = dlb2_eventdev_dir_queue_setup(dlb2, ev_queue, ev_port);
	else
		err = dlb2_event_queue_join_ldb(dlb2, ev_port, ev_queue, prio);

	if (err) {
		DLB2_LOG_ERR("port link failure for %s ev_q %d, ev_port %d",
			     ev_queue->qm_queue.is_directed ? "DIR" : "LDB",
			     ev_queue->id, ev_port->id);

		rte_errno = err;
		return -1;
	}

	return 0;
}

static int
dlb2_validate_port_link(struct dlb2_eventdev_port *ev_port,
			uint8_t queue_id,
			bool link_exists,
			int index)
{
	struct dlb2_eventdev *dlb2 = ev_port->dlb2;
	struct dlb2_eventdev_queue *ev_queue;
	bool port_is_dir, queue_is_dir;

	if (queue_id > dlb2->num_queues) {
		rte_errno = -EINVAL;
		return -1;
	}

	ev_queue = &dlb2->ev_queues[queue_id];

	if (!ev_queue->setup_done &&
	    ev_queue->qm_queue.config_state != DLB2_PREV_CONFIGURED) {
		rte_errno = -EINVAL;
		return -1;
	}

	port_is_dir = ev_port->qm_port.is_directed;
	queue_is_dir = ev_queue->qm_queue.is_directed;

	if (port_is_dir != queue_is_dir) {
		DLB2_LOG_ERR("%s queue %u can't link to %s port %u",
			     queue_is_dir ? "DIR" : "LDB", ev_queue->id,
			     port_is_dir ? "DIR" : "LDB", ev_port->id);

		rte_errno = -EINVAL;
		return -1;
	}

	/* Check if there is space for the requested link */
	if (!link_exists && index == -1) {
		DLB2_LOG_ERR("no space for new link");
		rte_errno = -ENOSPC;
		return -1;
	}

	/* Check if the directed port is already linked */
	if (ev_port->qm_port.is_directed && ev_port->num_links > 0 &&
	    !link_exists) {
		DLB2_LOG_ERR("Can't link DIR port %d to >1 queues",
			     ev_port->id);
		rte_errno = -EINVAL;
		return -1;
	}

	/* Check if the directed queue is already linked */
	if (ev_queue->qm_queue.is_directed && ev_queue->num_links > 0 &&
	    !link_exists) {
		DLB2_LOG_ERR("Can't link DIR queue %d to >1 ports",
			     ev_queue->id);
		rte_errno = -EINVAL;
		return -1;
	}

	return 0;
}

static int
dlb2_eventdev_port_link(struct rte_eventdev *dev, void *event_port,
			const uint8_t queues[], const uint8_t priorities[],
			uint16_t nb_links)

{
	struct dlb2_eventdev_port *ev_port = event_port;
	struct dlb2_eventdev *dlb2;
	int i, j;

	RTE_SET_USED(dev);

	if (ev_port == NULL) {
		DLB2_LOG_ERR("dlb2: evport not setup");
		rte_errno = -EINVAL;
		return 0;
	}

	if (!ev_port->setup_done &&
	    ev_port->qm_port.config_state != DLB2_PREV_CONFIGURED) {
		DLB2_LOG_ERR("dlb2: evport not setup");
		rte_errno = -EINVAL;
		return 0;
	}

	/* Note: rte_event_port_link() ensures the PMD won't receive a NULL
	 * queues pointer.
	 */
	if (nb_links == 0) {
		DLB2_LOG_LINE_DBG("dlb2: nb_links is 0");
		return 0; /* Ignore and return success */
	}

	dlb2 = ev_port->dlb2;

	DLB2_LOG_LINE_DBG("Linking %u queues to %s port %d",
		     nb_links,
		     ev_port->qm_port.is_directed ? "DIR" : "LDB",
		     ev_port->id);

	for (i = 0; i < nb_links; i++) {
		struct dlb2_eventdev_queue *ev_queue;
		uint8_t queue_id, prio;
		bool found = false;
		int index = -1;

		queue_id = queues[i];
		prio = priorities[i];

		/* Check if the link already exists. */
		for (j = 0; j < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; j++)
			if (ev_port->link[j].valid) {
				if (ev_port->link[j].queue_id == queue_id) {
					found = true;
					index = j;
					break;
				}
			} else if (index == -1) {
				index = j;
			}

		/* could not link */
		if (index == -1)
			break;

		/* Check if already linked at the requested priority */
		if (found && ev_port->link[j].priority == prio)
			continue;

		if (dlb2_validate_port_link(ev_port, queue_id, found, index))
			break; /* return index of offending queue */

		ev_queue = &dlb2->ev_queues[queue_id];

		if (dlb2_do_port_link(dev, ev_queue, ev_port, prio))
			break; /* return index of offending queue */

		ev_queue->num_links++;

		ev_port->link[index].queue_id = queue_id;
		ev_port->link[index].priority = prio;
		ev_port->link[index].valid = true;
		/* Entry already exists?  If so, then must be prio change */
		if (!found)
			ev_port->num_links++;
	}
	return i;
}

static int16_t
dlb2_hw_unmap_ldb_qid_from_port(struct dlb2_hw_dev *handle,
				uint32_t qm_port_id,
				uint16_t qm_qid)
{
	struct dlb2_unmap_qid_args cfg;
	int32_t ret;

	if (handle == NULL)
		return -EINVAL;

	cfg.port_id = qm_port_id;
	cfg.qid = qm_qid;

	ret = dlb2_iface_unmap_qid(handle, &cfg);
	if (ret < 0)
		DLB2_LOG_ERR("dlb2: unmap qid error, ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);

	return ret;
}

static int
dlb2_event_queue_detach_ldb(struct dlb2_eventdev *dlb2,
			    struct dlb2_eventdev_port *ev_port,
			    struct dlb2_eventdev_queue *ev_queue)
{
	int ret, i;

	/* Don't unlink until start time. */
	if (dlb2->run_state == DLB2_RUN_STATE_STOPPED)
		return 0;

	for (i = 0; i < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		if (ev_port->link[i].valid &&
		    ev_port->link[i].queue_id == ev_queue->id)
			break; /* found */
	}

	/* This is expected with eventdev API!
	 * It blindly attempts to unmap all queues.
	 */
	if (i == DLB2_MAX_NUM_QIDS_PER_LDB_CQ) {
		DLB2_LOG_LINE_DBG("dlb2: ignoring LB QID %d not mapped for qm_port %d.",
			     ev_queue->qm_queue.id,
			     ev_port->qm_port.id);
		return 0;
	}

	ret = dlb2_hw_unmap_ldb_qid_from_port(&dlb2->qm_instance,
					      ev_port->qm_port.id,
					      ev_queue->qm_queue.id);
	if (!ret)
		ev_port->link[i].mapped = false;

	return ret;
}

static inline void
dlb2_port_credits_return(struct dlb2_port *qm_port)
{
	/* Return all port credits */
	if (qm_port->dlb2->version == DLB2_HW_V2_5) {
		if (qm_port->cached_credits) {
			rte_atomic_fetch_add_explicit(qm_port->credit_pool[DLB2_COMBINED_POOL],
					   qm_port->cached_credits, rte_memory_order_seq_cst);
			qm_port->cached_credits = 0;
		}
	} else {
		if (qm_port->cached_ldb_credits) {
			rte_atomic_fetch_add_explicit(qm_port->credit_pool[DLB2_LDB_QUEUE],
					   qm_port->cached_ldb_credits, rte_memory_order_seq_cst);
			qm_port->cached_ldb_credits = 0;
		}
		if (qm_port->cached_dir_credits) {
			rte_atomic_fetch_add_explicit(qm_port->credit_pool[DLB2_DIR_QUEUE],
					   qm_port->cached_dir_credits, rte_memory_order_seq_cst);
			qm_port->cached_dir_credits = 0;
		}
	}
}

static inline void
dlb2_release_sw_credits(struct dlb2_eventdev *dlb2,
			struct dlb2_eventdev_port *ev_port, uint16_t val)
{
	if (ev_port->inflight_credits) {
		rte_atomic_fetch_sub_explicit(&dlb2->inflights, val, rte_memory_order_seq_cst);
		ev_port->inflight_credits -= val;
	}
}

static void dlb2_check_and_return_credits(struct dlb2_eventdev_port *ev_port,
					  bool cond, uint32_t threshold)
{
#if DLB_SW_CREDITS_CHECKS || DLB_HW_CREDITS_CHECKS
	if (cond) {
		if (++ev_port->credit_return_count > threshold) {
#if DLB_SW_CREDITS_CHECKS
			dlb2_release_sw_credits(ev_port->dlb2, ev_port,
						ev_port->inflight_credits);
#endif
#if DLB_HW_CREDITS_CHECKS
			dlb2_port_credits_return(&ev_port->qm_port);
#endif
			ev_port->credit_return_count = 0;
		}
	} else {
		ev_port->credit_return_count = 0;
	}
#else
/* Avoid compilation warning of unused parameters */
(void)ev_port, (void)cond, (void)threshold;
#endif
}

static int
dlb2_eventdev_port_unlink(struct rte_eventdev *dev, void *event_port,
			  uint8_t queues[], uint16_t nb_unlinks)
{
	struct dlb2_eventdev_port *ev_port = event_port;
	struct dlb2_eventdev *dlb2;
	int i;

	RTE_SET_USED(dev);

	if (!ev_port->setup_done) {
		DLB2_LOG_ERR("dlb2: evport %d is not configured",
			     ev_port->id);
		rte_errno = -EINVAL;
		return 0;
	}

	if (queues == NULL || nb_unlinks == 0) {
		DLB2_LOG_LINE_DBG("dlb2: queues is NULL or nb_unlinks is 0");
		nb_unlinks = 0; /* Ignore and return success */
		goto ret_credits;
	}

	if (ev_port->qm_port.is_directed) {
		DLB2_LOG_LINE_DBG("dlb2: ignore unlink from dir port %d",
			     ev_port->id);
		rte_errno = 0;
		goto ret_credits;
	}

	dlb2 = ev_port->dlb2;

	for (i = 0; i < nb_unlinks; i++) {
		struct dlb2_eventdev_queue *ev_queue;
		int ret, j;

		if (queues[i] >= dlb2->num_queues) {
			DLB2_LOG_ERR("dlb2: invalid queue id %d", queues[i]);
			rte_errno = -EINVAL;
			return i; /* return index of offending queue */
		}

		ev_queue = &dlb2->ev_queues[queues[i]];

		/* Does a link exist? */
		for (j = 0; j < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; j++)
			if (ev_port->link[j].queue_id == queues[i] &&
			    ev_port->link[j].valid)
				break;

		if (j == DLB2_MAX_NUM_QIDS_PER_LDB_CQ)
			continue;

		ret = dlb2_event_queue_detach_ldb(dlb2, ev_port, ev_queue);
		if (ret) {
			DLB2_LOG_ERR("unlink err=%d for port %d queue %d",
				     ret, ev_port->id, queues[i]);
			rte_errno = -ENOENT;
			return i; /* return index of offending queue */
		}

		ev_port->link[j].valid = false;
		ev_port->num_links--;
		ev_queue->num_links--;
	}

ret_credits:
	if (ev_port->inflight_credits)
		dlb2_check_and_return_credits(ev_port, true, 0);

	return nb_unlinks;
}

static int
dlb2_eventdev_port_unlinks_in_progress(struct rte_eventdev *dev,
				       void *event_port)
{
	struct dlb2_eventdev_port *ev_port = event_port;
	struct dlb2_eventdev *dlb2;
	struct dlb2_hw_dev *handle;
	struct dlb2_pending_port_unmaps_args cfg;
	int ret;

	RTE_SET_USED(dev);

	if (!ev_port->setup_done) {
		DLB2_LOG_ERR("dlb2: evport %d is not configured",
			     ev_port->id);
		rte_errno = -EINVAL;
		return 0;
	}

	cfg.port_id = ev_port->qm_port.id;
	dlb2 = ev_port->dlb2;
	handle = &dlb2->qm_instance;
	ret = dlb2_iface_pending_port_unmaps(handle, &cfg);

	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: num_unlinks_in_progress ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		return ret;
	}

	return cfg.response.id;
}

static int
dlb2_eventdev_reapply_configuration(struct rte_eventdev *dev)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	int ret, i;

	/* If an event queue or port was previously configured, but hasn't been
	 * reconfigured, reapply its original configuration.
	 */
	for (i = 0; i < dlb2->num_queues; i++) {
		struct dlb2_eventdev_queue *ev_queue;

		ev_queue = &dlb2->ev_queues[i];

		if (ev_queue->qm_queue.config_state != DLB2_PREV_CONFIGURED)
			continue;

		ret = dlb2_eventdev_queue_setup(dev, i, &ev_queue->conf);
		if (ret < 0) {
			DLB2_LOG_ERR("dlb2: failed to reconfigure queue %d", i);
			return ret;
		}
	}

	for (i = 0; i < dlb2->num_ports; i++) {
		struct dlb2_eventdev_port *ev_port = &dlb2->ev_ports[i];

		if (ev_port->qm_port.config_state != DLB2_PREV_CONFIGURED)
			continue;

		ret = dlb2_eventdev_port_setup(dev, i, &ev_port->conf);
		if (ret < 0) {
			DLB2_LOG_ERR("dlb2: failed to reconfigure ev_port %d",
				     i);
			return ret;
		}
	}

	return 0;
}

static int
dlb2_eventdev_apply_port_links(struct rte_eventdev *dev)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	int i;

	/* Perform requested port->queue links */
	for (i = 0; i < dlb2->num_ports; i++) {
		struct dlb2_eventdev_port *ev_port = &dlb2->ev_ports[i];
		int j;

		for (j = 0; j < DLB2_MAX_NUM_QIDS_PER_LDB_CQ; j++) {
			struct dlb2_eventdev_queue *ev_queue;
			uint8_t prio, queue_id;

			if (!ev_port->link[j].valid)
				continue;

			prio = ev_port->link[j].priority;
			queue_id = ev_port->link[j].queue_id;

			if (dlb2_validate_port_link(ev_port, queue_id, true, j))
				return -EINVAL;

			ev_queue = &dlb2->ev_queues[queue_id];

			if (dlb2_do_port_link(dev, ev_queue, ev_port, prio))
				return -EINVAL;
		}
	}

	return 0;
}

static int
dlb2_eventdev_start(struct rte_eventdev *dev)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	struct dlb2_start_domain_args cfg;
	int ret, i;

	rte_spinlock_lock(&dlb2->qm_instance.resource_lock);
	if (dlb2->run_state != DLB2_RUN_STATE_STOPPED) {
		DLB2_LOG_ERR("bad state %d for dev_start",
			     (int)dlb2->run_state);
		rte_spinlock_unlock(&dlb2->qm_instance.resource_lock);
		return -EINVAL;
	}
	dlb2->run_state = DLB2_RUN_STATE_STARTING;
	rte_spinlock_unlock(&dlb2->qm_instance.resource_lock);

	/* If the device was configured more than once, some event ports and/or
	 * queues may need to be reconfigured.
	 */
	ret = dlb2_eventdev_reapply_configuration(dev);
	if (ret)
		return ret;

	/* The DLB PMD delays port links until the device is started. */
	ret = dlb2_eventdev_apply_port_links(dev);
	if (ret)
		return ret;

	for (i = 0; i < dlb2->num_ports; i++) {
		if (!dlb2->ev_ports[i].setup_done) {
			DLB2_LOG_ERR("dlb2: port %d not setup", i);
			return -ESTALE;
		}
	}

	for (i = 0; i < dlb2->num_queues; i++) {
		if (dlb2->ev_queues[i].num_links == 0) {
			DLB2_LOG_ERR("dlb2: queue %d is not linked", i);
			return -ENOLINK;
		}
	}

	ret = dlb2_iface_sched_domain_start(handle, &cfg);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: sched_domain_start ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		return ret;
	}

	dlb2->run_state = DLB2_RUN_STATE_STARTED;
	DLB2_LOG_LINE_DBG("dlb2: sched_domain_start completed OK");

	return 0;
}

static inline uint32_t
dlb2_port_credits_get(struct dlb2_port *qm_port,
		      enum dlb2_hw_queue_types type)
{
	uint32_t credits = *qm_port->credit_pool[type];
	/* By default hw_credit_quanta is DLB2_SW_CREDIT_BATCH_SZ */
	uint32_t batch_size = qm_port->hw_credit_quanta;

	if (unlikely(credits < batch_size))
		batch_size = credits;

	if (likely(credits &&
		   rte_atomic_compare_exchange_strong_explicit(
			qm_port->credit_pool[type],
			&credits, credits - batch_size,
			rte_memory_order_seq_cst, rte_memory_order_seq_cst)))
		return batch_size;
	else
		return 0;
}

static inline void
dlb2_replenish_sw_credits(struct dlb2_eventdev *dlb2,
			  struct dlb2_eventdev_port *ev_port)
{
	uint16_t quanta = ev_port->credit_update_quanta;

	if (ev_port->inflight_credits >= quanta * 2) {
		/* Replenish credits, saving one quanta for enqueues */
		uint16_t val = ev_port->inflight_credits - quanta;

		dlb2_release_sw_credits(dlb2, ev_port, val);
	}
}

static inline int
dlb2_check_enqueue_sw_credits(struct dlb2_eventdev *dlb2,
			      struct dlb2_eventdev_port *ev_port)
{
	uint32_t sw_inflights = rte_atomic_load_explicit(&dlb2->inflights,
						rte_memory_order_seq_cst);
	const int num = 1;

	if (unlikely(ev_port->inflight_max < sw_inflights)) {
		DLB2_INC_STAT(ev_port->stats.traffic.tx_nospc_inflight_max, 1);
		rte_errno = -ENOSPC;
		return 1;
	}

	if (ev_port->inflight_credits < num) {
		/* check if event enqueue brings ev_port over max threshold */
		uint32_t credit_update_quanta = ev_port->credit_update_quanta;

		if (sw_inflights + credit_update_quanta >
				dlb2->new_event_limit) {
			DLB2_INC_STAT(
			ev_port->stats.traffic.tx_nospc_new_event_limit,
			1);
			rte_errno = -ENOSPC;
			return 1;
		}

		/* Application will retry if this attempt fails due to contention */
		if (rte_atomic_compare_exchange_strong_explicit(&dlb2->inflights, &sw_inflights,
			(sw_inflights+credit_update_quanta), rte_memory_order_seq_cst,
			rte_memory_order_seq_cst))
			ev_port->inflight_credits += (credit_update_quanta);
		else {
			rte_errno = -ENOSPC;
			return 1;
		}

		if (ev_port->inflight_credits < num) {
			DLB2_INC_STAT(
			ev_port->stats.traffic.tx_nospc_inflight_credits,
			1);
			rte_errno = -ENOSPC;
			return 1;
		}
	}

	return 0;
}

static inline int
dlb2_check_enqueue_hw_ldb_credits(struct dlb2_port *qm_port)
{
	if (unlikely(qm_port->cached_ldb_credits == 0)) {
		qm_port->cached_ldb_credits =
			dlb2_port_credits_get(qm_port,
					      DLB2_LDB_QUEUE);
		if (unlikely(qm_port->cached_ldb_credits == 0)) {
			DLB2_INC_STAT(
			qm_port->ev_port->stats.traffic.tx_nospc_ldb_hw_credits,
			1);
			DLB2_LOG_LINE_DBG("ldb credits exhausted");
			return 1; /* credits exhausted */
		}
	}

	return 0;
}

static inline int
dlb2_check_enqueue_hw_dir_credits(struct dlb2_port *qm_port)
{
	if (unlikely(qm_port->cached_dir_credits == 0)) {
		qm_port->cached_dir_credits =
			dlb2_port_credits_get(qm_port,
					      DLB2_DIR_QUEUE);
		if (unlikely(qm_port->cached_dir_credits == 0)) {
			DLB2_INC_STAT(
			qm_port->ev_port->stats.traffic.tx_nospc_dir_hw_credits,
			1);
			DLB2_LOG_LINE_DBG("dir credits exhausted");
			return 1; /* credits exhausted */
		}
	}

	return 0;
}

static inline int
dlb2_check_enqueue_hw_credits(struct dlb2_port *qm_port)
{
	if (unlikely(qm_port->cached_credits == 0)) {
		qm_port->cached_credits =
			dlb2_port_credits_get(qm_port,
					      DLB2_COMBINED_POOL);
		if (unlikely(qm_port->cached_credits == 0)) {
			DLB2_INC_STAT(
			qm_port->ev_port->stats.traffic.tx_nospc_hw_credits, 1);
			DLB2_LOG_LINE_DBG("credits exhausted");
			return 1; /* credits exhausted */
		}
	}

	return 0;
}

static __rte_always_inline void
dlb2_pp_write(struct process_local_port_data *port_data, struct dlb2_enqueue_qe *qe4)
{
	dlb2_movdir64b(port_data->pp_addr, qe4);
}

static __rte_always_inline void
dlb2_pp_write_reorder(struct process_local_port_data *port_data,
	      struct dlb2_enqueue_qe *qe4)
{
	for (uint8_t i = 0; i < 4; i++) {
		if (qe4[i].cmd_byte != DLB2_NOOP_CMD_BYTE) {
			dlb2_movdir64b(port_data->pp_addr, qe4);
			return;
		}
	}
}

static __rte_always_inline int
dlb2_pp_check4_write(struct process_local_port_data *port_data,
	      struct dlb2_enqueue_qe *qe4)
{
	for (uint8_t i = 0; i < DLB2_NUM_QES_PER_CACHE_LINE; i++)
		if (((uint64_t *)&qe4[i])[1] == 0)
			return 0;

	dlb2_movdir64b(port_data->pp_addr, qe4);
	memset(qe4, 0, DLB2_NUM_QES_PER_CACHE_LINE * sizeof(struct dlb2_enqueue_qe));
	return DLB2_NUM_QES_PER_CACHE_LINE;
}

static inline int
dlb2_consume_qe_immediate(struct dlb2_port *qm_port, int num)
{
	struct process_local_port_data *port_data;
	struct dlb2_cq_pop_qe *qe;

	RTE_ASSERT(qm_port->config_state == DLB2_CONFIGURED);

	qe = qm_port->consume_qe;

	qe->tokens = num - 1;

	/* No store fence needed since no pointer is being sent, and CQ token
	 * pops can be safely reordered with other HCWs.
	 */
	port_data = &dlb2_port[qm_port->id][PORT_TYPE(qm_port)];

	dlb2_movdir64b_single(port_data->pp_addr, qe);

	DLB2_LOG_LINE_DBG("dlb2: consume immediate - %d QEs", num);

	qm_port->owed_tokens = 0;

	return 0;
}

static inline void
dlb2_hw_do_enqueue(struct dlb2_port *qm_port,
		   bool do_sfence,
		   struct process_local_port_data *port_data)
{
	/* Since MOVDIR64B is weakly-ordered, use an SFENCE to ensure that
	 * application writes complete before enqueueing the QE.
	 */
	if (do_sfence)
		rte_wmb();

	dlb2_pp_write(port_data, qm_port->qe4);
}

static inline void
dlb2_construct_token_pop_qe(struct dlb2_port *qm_port, int idx)
{
	struct dlb2_cq_pop_qe *qe = (void *)qm_port->qe4;
	int num = qm_port->owed_tokens;

	qe[idx].cmd_byte = DLB2_POP_CMD_BYTE;
	qe[idx].tokens = num - 1;

	qm_port->owed_tokens = 0;
}

static inline int
dlb2_event_enqueue_prep(struct dlb2_eventdev_port *ev_port,
			struct dlb2_port *qm_port,
			const struct rte_event ev[],
			uint8_t *sched_type,
			uint8_t *queue_id)
{
	struct dlb2_eventdev *dlb2 = ev_port->dlb2;
	struct dlb2_eventdev_queue *ev_queue;
#if DLB_HW_CREDITS_CHECKS
	uint16_t *cached_credits = NULL;
#endif
	struct dlb2_queue *qm_queue;

	ev_queue = &dlb2->ev_queues[ev->queue_id];
	qm_queue = &ev_queue->qm_queue;
	*queue_id = qm_queue->id;

	/* Ignore sched_type and hardware credits on release events */
	if (ev->op == RTE_EVENT_OP_RELEASE)
		goto op_check;

	if (!qm_queue->is_directed) {
#if DLB_HW_CREDITS_CHECKS
		/* Load balanced destination queue */

		if (dlb2->version == DLB2_HW_V2) {
			if (dlb2_check_enqueue_hw_ldb_credits(qm_port)) {
				rte_errno = -ENOSPC;
				return 1;
			}
			cached_credits = &qm_port->cached_ldb_credits;
		} else {
			if (dlb2_check_enqueue_hw_credits(qm_port)) {
				rte_errno = -ENOSPC;
				return 1;
			}
			cached_credits = &qm_port->cached_credits;
		}
#endif
#if DLB_TYPE_CHECK
		switch (ev->sched_type) {
		case RTE_SCHED_TYPE_ORDERED:
			DLB2_LOG_LINE_DBG("dlb2: put_qe: RTE_SCHED_TYPE_ORDERED");
			if (qm_queue->sched_type != RTE_SCHED_TYPE_ORDERED) {
				DLB2_LOG_ERR("dlb2: tried to send ordered event to unordered queue %d",
					     *queue_id);
				rte_errno = -EINVAL;
				return 1;
			}
			*sched_type = DLB2_SCHED_ORDERED;
			break;
		case RTE_SCHED_TYPE_ATOMIC:
			DLB2_LOG_LINE_DBG("dlb2: put_qe: RTE_SCHED_TYPE_ATOMIC");
			*sched_type = DLB2_SCHED_ATOMIC;
			break;
		case RTE_SCHED_TYPE_PARALLEL:
			DLB2_LOG_LINE_DBG("dlb2: put_qe: RTE_SCHED_TYPE_PARALLEL");
			if (qm_queue->sched_type == RTE_SCHED_TYPE_ORDERED)
				*sched_type = DLB2_SCHED_ORDERED;
			else
				*sched_type = DLB2_SCHED_UNORDERED;
			break;
		default:
			DLB2_LOG_ERR("Unsupported LDB sched type in put_qe");
			DLB2_INC_STAT(ev_port->stats.tx_invalid, 1);
			rte_errno = -EINVAL;
			return 1;
		}
#else
#if (RTE_SCHED_TYPE_PARALLEL != 2) || (RTE_SCHED_TYPE_ATOMIC != 1)
#error "ERROR: RTE event schedule type values changed. Needs a code change"
#endif
		/* Map RTE eventdev schedule type to DLB HW schedule type */
		if (qm_queue->sched_type != RTE_SCHED_TYPE_ORDERED)
			/* RTE-Parallel -> DLB-UnOrd 2->1, RTE-Atm -> DLB-Atm 1->0 */
			*sched_type = ev->sched_type - 1;
		else /* To support CFG_ALL_TYPEs */
			*sched_type = DLB2_SCHED_ORDERED; /* RTE-Ord -> DLB-Ord 0->2 */
#endif
	} else {
		/* Directed destination queue */
#if DLB_HW_CREDITS_CHECKS
		if (dlb2->version == DLB2_HW_V2) {
			if (dlb2_check_enqueue_hw_dir_credits(qm_port)) {
				rte_errno = -ENOSPC;
				return 1;
			}
			cached_credits = &qm_port->cached_dir_credits;
		} else {
			if (dlb2_check_enqueue_hw_credits(qm_port)) {
				rte_errno = -ENOSPC;
				return 1;
			}
			cached_credits = &qm_port->cached_credits;
		}
#endif
		DLB2_LOG_LINE_DBG("dlb2: put_qe: RTE_SCHED_TYPE_DIRECTED");

		*sched_type = DLB2_SCHED_DIRECTED;
	}

op_check:
	switch (ev->op) {
	case RTE_EVENT_OP_NEW:
#if DLB_SW_CREDITS_CHECKS
		/* Check that a sw credit is available */
		if (dlb2_check_enqueue_sw_credits(dlb2, ev_port)) {
			rte_errno = -ENOSPC;
			return 1;
		}
		ev_port->inflight_credits--;
#endif
#if DLB_HW_CREDITS_CHECKS
		(*cached_credits)--;
#endif
		break;
	case RTE_EVENT_OP_FORWARD:
		/* Check for outstanding_releases underflow. If this occurs,
		 * the application is not using the EVENT_OPs correctly; for
		 * example, forwarding or releasing events that were not
		 * dequeued.
		 */
		RTE_ASSERT(ev_port->outstanding_releases > 0);
		ev_port->outstanding_releases--;
		qm_port->issued_releases++;
#if DLB_HW_CREDITS_CHECKS
		(*cached_credits)--;
#endif
		break;
	case RTE_EVENT_OP_RELEASE:
#if DLB_SW_CREDITS_CHECKS
		ev_port->inflight_credits++;
#endif
		/* Check for outstanding_releases underflow. If this occurs,
		 * the application is not using the EVENT_OPs correctly; for
		 * example, forwarding or releasing events that were not
		 * dequeued.
		 */
		RTE_ASSERT(ev_port->outstanding_releases > 0);
		ev_port->outstanding_releases--;
		qm_port->issued_releases++;

#if DLB_SW_CREDITS_CHECKS
		/* Replenish s/w credits if enough are cached */
		dlb2_replenish_sw_credits(dlb2, ev_port);
#endif
		break;
	}

	DLB2_INC_STAT(ev_port->stats.tx_op_cnt[ev->op], 1);
	DLB2_INC_STAT(ev_port->stats.traffic.tx_ok, 1);

#ifndef RTE_LIBRTE_PMD_DLB_QUELL_STATS
	if (ev->op != RTE_EVENT_OP_RELEASE) {
		DLB2_INC_STAT(ev_port->stats.queue[ev->queue_id].enq_ok, 1);
		DLB2_INC_STAT(ev_port->stats.tx_sched_cnt[*sched_type], 1);
	}
#endif

	return 0;
}

static inline __m128i
dlb2_event_to_qe(const struct rte_event *ev, uint8_t cmd, uint8_t sched_type, uint8_t qid)
{
	__m128i dlb2_to_qe_shuffle = _mm_set_epi8(
	    0xFF, 0xFF,			 /* zero out cmd word */
	    1, 0,			 /* low 16-bits of flow id */
	    0xFF, 0xFF, /* zero QID, sched_type etc fields to be filled later */
	    3, 2,			 /* top of flow id, event type and subtype */
	    15, 14, 13, 12, 11, 10, 9, 8 /* data from end of event goes at start */
	);

	/* event may not be 16 byte aligned. Use 16 byte unaligned load */
	__m128i tmp = _mm_lddqu_si128((const __m128i *)ev);
	__m128i qe = _mm_shuffle_epi8(tmp, dlb2_to_qe_shuffle);
	struct dlb2_enqueue_qe *dq = (struct dlb2_enqueue_qe *)&qe;
	/* set the cmd field */
	qe = _mm_insert_epi8(qe, cmd, 15);
	/* insert missing 16-bits with qid, sched_type and priority */
	uint16_t qid_stype_prio =
	    qid | (uint16_t)sched_type << 8 | ((uint16_t)ev->priority & 0xE0) << 5;
	qe = _mm_insert_epi16(qe, qid_stype_prio, 5);
	dq->weight = RTE_PMD_DLB2_GET_QE_WEIGHT(ev);
	return qe;
}

static inline uint16_t
__dlb2_event_enqueue_burst_reorder(void *event_port,
		const struct rte_event events[],
		uint16_t num,
		bool use_delayed)
{
	struct dlb2_eventdev_port *ev_port = event_port;
	struct dlb2_port *qm_port = &ev_port->qm_port;
	struct dlb2_reorder *order = qm_port->order;
	struct process_local_port_data *port_data;
	bool is_directed = qm_port->is_directed;
	uint8_t n = order->next_to_enqueue;
	uint8_t p_cnt = 0;
	int retries = ev_port->enq_retries;
	__m128i new_qes[4], *from = NULL;
	int num_new = 0;
	int num_tx;
	int i;

	RTE_ASSERT(ev_port->enq_configured);
	RTE_ASSERT(events != NULL);

	port_data = &dlb2_port[qm_port->id][PORT_TYPE(qm_port)];

	if (!port_data->mmaped)
		dlb2_iface_low_level_io_init(&ev_port->dlb2->qm_instance);

	num_tx = RTE_MIN(num, ev_port->conf.enqueue_depth);
#if DLB2_BYPASS_FENCE_ON_PP == 1
	if (!qm_port->is_producer) /* Call memory fense once at the start */
		rte_wmb();	   /*  calls _mm_sfence() */
#else
	rte_wmb(); /*  calls _mm_sfence() */
#endif
	for (i = 0; i < num_tx; i++) {
		uint8_t sched_type = 0;
		uint8_t reorder_idx = events[i].impl_opaque;
		int16_t thresh = qm_port->token_pop_thresh;
		uint8_t qid = 0;
		int ret;

		while ((ret = dlb2_event_enqueue_prep(ev_port, qm_port, &events[i],
						      &sched_type, &qid)) != 0 &&
		       rte_errno == -ENOSPC && --retries > 0)
			rte_pause();

		if (ret != 0) /* Either there is error or retires exceeded */
			break;

		switch (events[i].op) {
		case RTE_EVENT_OP_NEW:
			new_qes[num_new++] = dlb2_event_to_qe(
			    &events[i], DLB2_NEW_CMD_BYTE, sched_type, qid);
			if (num_new == RTE_DIM(new_qes)) {
				dlb2_pp_write(port_data, (struct dlb2_enqueue_qe *)&new_qes);
				num_new = 0;
			}
			break;
		case RTE_EVENT_OP_FORWARD: {
			order->enq_reorder[reorder_idx].m128 = dlb2_event_to_qe(
			    &events[i], is_directed ? DLB2_NEW_CMD_BYTE : DLB2_FWD_CMD_BYTE,
			    sched_type, qid);
			n += dlb2_pp_check4_write(port_data, &order->enq_reorder[n].qe);
			break;
		}
		case RTE_EVENT_OP_RELEASE: {
			order->enq_reorder[reorder_idx].m128 = dlb2_event_to_qe(
			    &events[i], is_directed ? DLB2_NOOP_CMD_BYTE : DLB2_COMP_CMD_BYTE,
			    sched_type, 0xFF);
			break;
		}
		}

		if (use_delayed && qm_port->token_pop_mode == RTE_PMD_DLB2_DELAYED_POP &&
		    (events[i].op == RTE_EVENT_OP_FORWARD ||
		     events[i].op == RTE_EVENT_OP_RELEASE) &&
		    qm_port->issued_releases >= thresh - 1) {

			dlb2_consume_qe_immediate(qm_port, qm_port->owed_tokens);

			/* Reset the releases for the next QE batch */
			qm_port->issued_releases -= thresh;

			/* When using delayed token pop mode, the
			 * initial token threshold is the full CQ
			 * depth. After the first token pop, we need to
			 * reset it to the dequeue_depth.
			 */
			qm_port->token_pop_thresh =
			    qm_port->dequeue_depth;
		}
	}
	while (order->enq_reorder[n].u64[1] != 0) {
		__m128i tmp[4] = {0}, *send = NULL;
		bool enq;

		if (!p_cnt)
			from = &order->enq_reorder[n].m128;

		p_cnt++;
		n++;

		enq = !n || p_cnt == 4 || !order->enq_reorder[n].u64[1];
		if (!enq)
			continue;

		if (p_cnt < 4) {
			memcpy(tmp, from, p_cnt * sizeof(struct dlb2_enqueue_qe));
			send = tmp;
		} else {
			send  = from;
		}

		if (is_directed)
			dlb2_pp_write_reorder(port_data, (struct dlb2_enqueue_qe *)send);
		else
			dlb2_pp_write(port_data, (struct dlb2_enqueue_qe *)send);
		memset(from, 0, p_cnt * sizeof(struct dlb2_enqueue_qe));
		p_cnt = 0;
	}
	order->next_to_enqueue = n;

	if (num_new > 0) {
		switch (num_new) {
		case 1:
			new_qes[1] = _mm_setzero_si128(); /* fall-through */
		case 2:
			new_qes[2] = _mm_setzero_si128(); /* fall-through */
		case 3:
			new_qes[3] = _mm_setzero_si128();
		}
		dlb2_pp_write(port_data, (struct dlb2_enqueue_qe *)&new_qes);
		num_new = 0;
	}

	return i;
}

static inline uint16_t
__dlb2_event_enqueue_burst(void *event_port,
			   const struct rte_event events[],
			   uint16_t num,
			   bool use_delayed)
{
	struct dlb2_eventdev_port *ev_port = event_port;
	struct dlb2_port *qm_port = &ev_port->qm_port;
	struct process_local_port_data *port_data;
	int retries = ev_port->enq_retries;
	int num_tx;
	int i;

	RTE_ASSERT(ev_port->enq_configured);
	RTE_ASSERT(events != NULL);

	if (qm_port->reorder_en)
		return __dlb2_event_enqueue_burst_reorder(event_port, events, num, use_delayed);

	i = 0;

	port_data = &dlb2_port[qm_port->id][PORT_TYPE(qm_port)];

	if (!port_data->mmaped)
		dlb2_iface_low_level_io_init(&ev_port->dlb2->qm_instance);

	num_tx = RTE_MIN(num, ev_port->conf.enqueue_depth);
	while (i < num_tx) {
		uint8_t sched_types[DLB2_NUM_QES_PER_CACHE_LINE];
		uint8_t queue_ids[DLB2_NUM_QES_PER_CACHE_LINE];
		int pop_offs = 0;
		int j = 0;

		memset(qm_port->qe4,
		       0,
		       DLB2_NUM_QES_PER_CACHE_LINE *
		       sizeof(struct dlb2_enqueue_qe));

		for (; j < DLB2_NUM_QES_PER_CACHE_LINE && (i + j) < num; j++) {
			const struct rte_event *ev = &events[i + j];
			int16_t thresh = qm_port->token_pop_thresh;
			int ret;

			if (use_delayed &&
			    qm_port->token_pop_mode == RTE_PMD_DLB2_DELAYED_POP &&
			    (ev->op == RTE_EVENT_OP_FORWARD ||
			     ev->op == RTE_EVENT_OP_RELEASE) &&
			    qm_port->issued_releases >= thresh - 1) {
				/* Insert the token pop QE and break out. This
				 * may result in a partial HCW, but that is
				 * simpler than supporting arbitrary QE
				 * insertion.
				 */
				dlb2_construct_token_pop_qe(qm_port, j);

				/* Reset the releases for the next QE batch */
				qm_port->issued_releases -= thresh;

				pop_offs = 1;
				j++;
				break;
			}

			/*
			 * Retry if insufficient credits
			 */
			do {
				ret = dlb2_event_enqueue_prep(ev_port,
							      qm_port,
							      ev,
							      &sched_types[j],
							      &queue_ids[j]);
			} while ((ret == -ENOSPC) && (retries-- > 0));

			if (ret != 0)
				break;
		}

		if (j == 0)
			break;

		dlb2_event_build_hcws(qm_port, &events[i], j - pop_offs,
				      sched_types, queue_ids);

#if DLB2_BYPASS_FENCE_ON_PP == 1
		/* Bypass fence instruction for producer ports */
		dlb2_hw_do_enqueue(qm_port, i == 0 && !qm_port->is_producer, port_data);
#else
		dlb2_hw_do_enqueue(qm_port, i == 0, port_data);
#endif

		/* Don't include the token pop QE in the enqueue count */
		i += j - pop_offs;

		/* Don't interpret j < DLB2_NUM_... as out-of-credits if
		 * pop_offs != 0
		 */
		if (j < DLB2_NUM_QES_PER_CACHE_LINE && pop_offs == 0)
			break;
	}

	dlb2_check_and_return_credits(ev_port, !i,
				      DLB2_ENQ_FAIL_CREDIT_RETURN_THRES);
	return i;
}

static uint16_t
dlb2_event_enqueue_burst(void *event_port,
			     const struct rte_event events[],
			     uint16_t num)
{
	return __dlb2_event_enqueue_burst(event_port, events, num, false);
}

static uint16_t
dlb2_event_enqueue_burst_delayed(void *event_port,
				     const struct rte_event events[],
				     uint16_t num)
{
	return __dlb2_event_enqueue_burst(event_port, events, num, true);
}

static uint16_t
dlb2_event_enqueue_new_burst(void *event_port,
			     const struct rte_event events[],
			     uint16_t num)
{
	return __dlb2_event_enqueue_burst(event_port, events, num, false);
}

static uint16_t
dlb2_event_enqueue_new_burst_delayed(void *event_port,
				     const struct rte_event events[],
				     uint16_t num)
{
	return __dlb2_event_enqueue_burst(event_port, events, num, true);
}

static uint16_t
dlb2_event_enqueue_forward_burst(void *event_port,
				 const struct rte_event events[],
				 uint16_t num)
{
	return __dlb2_event_enqueue_burst(event_port, events, num, false);
}

static uint16_t
dlb2_event_enqueue_forward_burst_delayed(void *event_port,
					 const struct rte_event events[],
					 uint16_t num)
{
	return __dlb2_event_enqueue_burst(event_port, events, num, true);
}

static void
dlb2_event_release(struct dlb2_eventdev *dlb2,
		   uint8_t port_id,
		   int n)
{
	struct process_local_port_data *port_data;
	struct dlb2_eventdev_port *ev_port;
	struct dlb2_port *qm_port;
	int i;

	if (port_id > dlb2->num_ports) {
		DLB2_LOG_ERR("Invalid port id %d in dlb2-event_release",
			     port_id);
		rte_errno = -EINVAL;
		return;
	}

	ev_port = &dlb2->ev_ports[port_id];
	qm_port = &ev_port->qm_port;
	port_data = &dlb2_port[qm_port->id][PORT_TYPE(qm_port)];

	i = 0;

	if (qm_port->is_directed) {
		i = n;
		goto sw_credit_update;
	}

	while (i < n) {
		int pop_offs = 0;
		int j = 0;

		/* Zero-out QEs */
		_mm_storeu_si128((void *)&qm_port->qe4[0], _mm_setzero_si128());
		_mm_storeu_si128((void *)&qm_port->qe4[1], _mm_setzero_si128());
		_mm_storeu_si128((void *)&qm_port->qe4[2], _mm_setzero_si128());
		_mm_storeu_si128((void *)&qm_port->qe4[3], _mm_setzero_si128());


		for (; j < DLB2_NUM_QES_PER_CACHE_LINE && (i + j) < n; j++) {
			int16_t thresh = qm_port->token_pop_thresh;

			if (qm_port->token_pop_mode == RTE_PMD_DLB2_DELAYED_POP &&
			    qm_port->issued_releases >= thresh - 1) {
				/* Insert the token pop QE */
				dlb2_construct_token_pop_qe(qm_port, j);

				/* Reset the releases for the next QE batch */
				qm_port->issued_releases -= thresh;

				pop_offs = 1;
				j++;
				break;
			}

			qm_port->qe4[j].cmd_byte = DLB2_COMP_CMD_BYTE;
			qm_port->issued_releases++;
		}

		dlb2_hw_do_enqueue(qm_port, i == 0, port_data);

		/* Don't include the token pop QE in the release count */
		i += j - pop_offs;
	}

sw_credit_update:
	/* each release returns one credit */
	if (unlikely(!ev_port->outstanding_releases)) {
		DLB2_LOG_ERR("%s: Outstanding releases underflowed.",
			     __func__);
		return;
	}
	ev_port->outstanding_releases -= i;
#if DLB_SW_CREDITS_CHECKS
	ev_port->inflight_credits += i;

	/* Replenish s/w credits if enough releases are performed */
	dlb2_replenish_sw_credits(dlb2, ev_port);
#endif
}

static inline void
dlb2_port_credits_inc(struct dlb2_port *qm_port, int num)
{
	uint32_t batch_size = qm_port->hw_credit_quanta;
	int val;

	/* increment port credits, and return to pool if exceeds threshold */
	if (qm_port->dlb2->version == DLB2_HW_V2_5) {
		qm_port->cached_credits += num;
		if (qm_port->cached_credits >= 2 * batch_size) {
			val = qm_port->cached_credits - batch_size;
			rte_atomic_fetch_add_explicit(
			    qm_port->credit_pool[DLB2_COMBINED_POOL], val,
			    rte_memory_order_seq_cst);
			qm_port->cached_credits -= val;
		}
	} else if (!qm_port->is_directed) {
		qm_port->cached_ldb_credits += num;
		if (qm_port->cached_ldb_credits >= 2 * batch_size) {
			val = qm_port->cached_ldb_credits - batch_size;
			rte_atomic_fetch_add_explicit(qm_port->credit_pool[DLB2_LDB_QUEUE],
					   val, rte_memory_order_seq_cst);
			qm_port->cached_ldb_credits -= val;
		}
	} else {
		qm_port->cached_ldb_credits += num;
		if (qm_port->cached_ldb_credits >= 2 * batch_size) {
			val = qm_port->cached_ldb_credits - batch_size;
			rte_atomic_fetch_add_explicit(qm_port->credit_pool[DLB2_LDB_QUEUE],
					   val, rte_memory_order_seq_cst);
			qm_port->cached_ldb_credits -= val;
		}
	}
}

#define CLB_MASK_IDX 0
#define CLB_VAL_IDX 1
static int
dlb2_monitor_callback(const uint64_t val,
		const uint64_t opaque[RTE_POWER_MONITOR_OPAQUE_SZ])
{
	/* abort if the value matches */
	return (val & opaque[CLB_MASK_IDX]) == opaque[CLB_VAL_IDX] ? -1 : 0;
}

static inline int
dlb2_dequeue_wait(struct dlb2_eventdev *dlb2,
		  struct dlb2_eventdev_port *ev_port,
		  struct dlb2_port *qm_port,
		  uint64_t timeout,
		  uint64_t start_ticks)
{
	struct process_local_port_data *port_data;
	uint64_t elapsed_ticks;

	port_data = &dlb2_port[qm_port->id][PORT_TYPE(qm_port)];

	elapsed_ticks = rte_get_timer_cycles() - start_ticks;

	/* Wait/poll time expired */
	if (elapsed_ticks >= timeout) {
		/* Return all credits before blocking if remaining credits in
		 * system is less than quanta.
		 */
		uint32_t sw_inflights = rte_atomic_load_explicit(&dlb2->inflights,
			rte_memory_order_seq_cst);
		uint32_t quanta = ev_port->credit_update_quanta;

		if (dlb2->new_event_limit - sw_inflights < quanta)
			dlb2_check_and_return_credits(ev_port, true, 0);
		return 1;
	} else if (dlb2->umwait_allowed) {
		struct rte_power_monitor_cond pmc;
		volatile struct dlb2_dequeue_qe *cq_base;
		union {
			uint64_t raw_qe[2];
			struct dlb2_dequeue_qe qe;
		} qe_mask;
		uint64_t expected_value;
		volatile uint64_t *monitor_addr;

		qe_mask.qe.cq_gen = 1; /* set mask */

		cq_base = port_data->cq_base;
		monitor_addr = (volatile uint64_t *)(volatile void *)
			&cq_base[qm_port->cq_idx];
		monitor_addr++; /* cq_gen bit is in second 64bit location */

		if (qm_port->gen_bit)
			expected_value = qe_mask.raw_qe[1];
		else
			expected_value = 0;

		pmc.addr = monitor_addr;
		/* store expected value and comparison mask in opaque data */
		pmc.opaque[CLB_VAL_IDX] = expected_value;
		pmc.opaque[CLB_MASK_IDX] = qe_mask.raw_qe[1];
		/* set up callback */
		pmc.fn = dlb2_monitor_callback;
		pmc.size = sizeof(uint64_t);

		rte_power_monitor(&pmc, timeout + start_ticks);

		DLB2_INC_STAT(ev_port->stats.traffic.rx_umonitor_umwait, 1);
	} else {
		uint64_t poll_interval = dlb2->poll_interval;
		uint64_t curr_ticks = rte_get_timer_cycles();
		uint64_t init_ticks = curr_ticks;

		while ((curr_ticks - start_ticks < timeout) &&
		       (curr_ticks - init_ticks < poll_interval))
			curr_ticks = rte_get_timer_cycles();
	}

	return 0;
}

static __rte_noinline int
dlb2_process_dequeue_qes(struct dlb2_eventdev_port *ev_port,
			 struct dlb2_port *qm_port,
			 struct rte_event *events,
			 struct dlb2_dequeue_qe *qes,
			 int cnt)
{
	uint8_t *qid_mappings = qm_port->qid_mappings;
	int i, num, evq_id;

	for (i = 0, num = 0; i < cnt; i++) {
		struct dlb2_dequeue_qe *qe = &qes[i];
		int sched_type_map[DLB2_NUM_HW_SCHED_TYPES] = {
			[DLB2_SCHED_ATOMIC] = RTE_SCHED_TYPE_ATOMIC,
			[DLB2_SCHED_UNORDERED] = RTE_SCHED_TYPE_PARALLEL,
			[DLB2_SCHED_ORDERED] = RTE_SCHED_TYPE_ORDERED,
			[DLB2_SCHED_DIRECTED] = RTE_SCHED_TYPE_ATOMIC,
		};

		/* Fill in event information.
		 * Note that flow_id must be embedded in the data by
		 * the app, such as the mbuf RSS hash field if the data
		 * buffer is a mbuf.
		 */
		if (unlikely(qe->error)) {
			DLB2_LOG_ERR("QE error bit ON");
			DLB2_INC_STAT(ev_port->stats.traffic.rx_drop, 1);
			dlb2_consume_qe_immediate(qm_port, 1);
			continue; /* Ignore */
		}

		events[num].u64 = qe->data;
		events[num].flow_id = qe->flow_id;
		events[num].priority = DLB2_TO_EV_PRIO((uint8_t)qe->priority);
		events[num].event_type = qe->u.event_type.major;
		events[num].sub_event_type = qe->u.event_type.sub;
		events[num].sched_type = sched_type_map[qe->sched_type];
		events[num].impl_opaque = qm_port->reorder_id++;
		RTE_PMD_DLB2_SET_QID_DEPTH(&events[num], qe->qid_depth);

		/* qid not preserved for directed queues */
		if (qm_port->is_directed)
			evq_id = ev_port->link[0].queue_id;
		else
			evq_id = qid_mappings[qe->qid];

		events[num].queue_id = evq_id;
		DLB2_INC_STAT(
			ev_port->stats.queue[evq_id].qid_depth[qe->qid_depth],
			1);
		DLB2_INC_STAT(ev_port->stats.rx_sched_cnt[qe->sched_type], 1);
		num++;
	}

	DLB2_INC_STAT(ev_port->stats.traffic.rx_ok, num);

	return num;
}

static inline int
dlb2_process_dequeue_four_qes(struct dlb2_eventdev_port *ev_port,
			      struct dlb2_port *qm_port,
			      struct rte_event *events,
			      struct dlb2_dequeue_qe *qes)
{
	int sched_type_map[] = {
		[DLB2_SCHED_ATOMIC] = RTE_SCHED_TYPE_ATOMIC,
		[DLB2_SCHED_UNORDERED] = RTE_SCHED_TYPE_PARALLEL,
		[DLB2_SCHED_ORDERED] = RTE_SCHED_TYPE_ORDERED,
		[DLB2_SCHED_DIRECTED] = RTE_SCHED_TYPE_ATOMIC,
	};
	const int num_events = DLB2_NUM_QES_PER_CACHE_LINE;
	uint8_t *qid_mappings = qm_port->qid_mappings;

	/* In the unlikely case that any of the QE error bits are set, process
	 * them one at a time.
	 */
	if (unlikely(qes[0].error || qes[1].error ||
		     qes[2].error || qes[3].error))
		return dlb2_process_dequeue_qes(ev_port, qm_port, events,
						 qes, num_events);
	const __m128i qe_to_ev_shuffle =
	    _mm_set_epi8(7, 6, 5, 4, 3, 2, 1, 0, /* last 8-bytes = data from first 8 */
			 0xFF, 0xFF, 0xFF, 0xFF, /* fill in later as 32-bit value*/
			 9, 8,			 /* event type and sub-event, + 4 zero bits */
			 13, 12 /* flow id, 16 bits */);
	for (int i = 0; i < 4; i++) {
		const __m128i hw_qe = _mm_load_si128((void *)&qes[i]);
		const __m128i event = _mm_shuffle_epi8(hw_qe, qe_to_ev_shuffle);
		/* prepare missing 32-bits for op, sched_type, QID, Priority and
		 * sequence number in impl_opaque
		 */
		const uint16_t qid_sched_prio = _mm_extract_epi16(hw_qe, 5);
		/* Extract qid_depth and format it as per event header */
		const uint8_t qid_depth = (_mm_extract_epi8(hw_qe, 15) & 0x6) << 1;
		const uint32_t qid =  (qm_port->is_directed) ? ev_port->link[0].queue_id :
					qid_mappings[(uint8_t)qid_sched_prio];
		const uint32_t sched_type = sched_type_map[(qid_sched_prio >> 8) & 0x3];
		const uint32_t priority = (qid_sched_prio >> 5) & 0xE0;

		const uint32_t dword1 = qid_depth |
		    sched_type << 6 | qid << 8 | priority << 16 | (qm_port->reorder_id + i) << 24;

		/* events[] may not be 16 byte aligned. So use separate load and store */
		const __m128i tmpEv = _mm_insert_epi32(event, dword1, 1);
		_mm_storeu_si128((__m128i *) &events[i], tmpEv);
	}
	qm_port->reorder_id += 4;

	DLB2_INC_STAT(ev_port->stats.rx_sched_cnt[qes[0].sched_type], 1);
	DLB2_INC_STAT(ev_port->stats.rx_sched_cnt[qes[1].sched_type], 1);
	DLB2_INC_STAT(ev_port->stats.rx_sched_cnt[qes[2].sched_type], 1);
	DLB2_INC_STAT(ev_port->stats.rx_sched_cnt[qes[3].sched_type], 1);

	DLB2_INC_STAT(
		ev_port->stats.queue[events[0].queue_id].
			qid_depth[qes[0].qid_depth],
		1);
	DLB2_INC_STAT(
		ev_port->stats.queue[events[1].queue_id].
			qid_depth[qes[1].qid_depth],
		1);
	DLB2_INC_STAT(
		ev_port->stats.queue[events[2].queue_id].
			qid_depth[qes[2].qid_depth],
		1);
	DLB2_INC_STAT(
		ev_port->stats.queue[events[3].queue_id].
			qid_depth[qes[3].qid_depth],
		1);

	DLB2_INC_STAT(ev_port->stats.traffic.rx_ok, num_events);

	return num_events;
}

static __rte_always_inline int
dlb2_recv_qe_sparse(struct dlb2_port *qm_port, struct dlb2_dequeue_qe *qe)
{
	volatile struct dlb2_dequeue_qe *cq_addr;
	uint8_t xor_mask[2] = {0x0F, 0x00};
	const uint8_t and_mask = 0x0F;
	__m128i *qes = (__m128i *)qe;
	uint8_t gen_bits, gen_bit;
	uintptr_t addr[4];
	uint16_t idx;

	cq_addr = dlb2_port[qm_port->id][PORT_TYPE(qm_port)].cq_base;

	idx = qm_port->cq_idx_unmasked & qm_port->cq_depth_mask;
	/* Load the next 4 QEs */
	addr[0] = (uintptr_t)&cq_addr[idx];
	addr[1] = (uintptr_t)&cq_addr[(idx +  4) & qm_port->cq_depth_mask];
	addr[2] = (uintptr_t)&cq_addr[(idx +  8) & qm_port->cq_depth_mask];
	addr[3] = (uintptr_t)&cq_addr[(idx + 12) & qm_port->cq_depth_mask];

	/* Prefetch next batch of QEs (all CQs occupy minimum 8 cache lines) */
	rte_prefetch0(&cq_addr[(idx + 16) & qm_port->cq_depth_mask]);
	rte_prefetch0(&cq_addr[(idx + 20) & qm_port->cq_depth_mask]);
	rte_prefetch0(&cq_addr[(idx + 24) & qm_port->cq_depth_mask]);
	rte_prefetch0(&cq_addr[(idx + 28) & qm_port->cq_depth_mask]);

	/* Correct the xor_mask for wrap-around QEs */
	gen_bit = qm_port->gen_bit;
	xor_mask[gen_bit] ^= !!((idx +  4) > qm_port->cq_depth_mask) << 1;
	xor_mask[gen_bit] ^= !!((idx +  8) > qm_port->cq_depth_mask) << 2;
	xor_mask[gen_bit] ^= !!((idx + 12) > qm_port->cq_depth_mask) << 3;

	/* Read the cache lines backwards to ensure that if QE[N] (N > 0) is
	 * valid, then QEs[0:N-1] are too.
	 */
	qes[3] = _mm_load_si128((__m128i *)(void *)addr[3]);
	rte_compiler_barrier();
	qes[2] = _mm_load_si128((__m128i *)(void *)addr[2]);
	rte_compiler_barrier();
	qes[1] = _mm_load_si128((__m128i *)(void *)addr[1]);
	rte_compiler_barrier();
	qes[0] = _mm_load_si128((__m128i *)(void *)addr[0]);

	/* Extract and combine the gen bits */
	gen_bits = ((_mm_extract_epi8(qes[0], 15) & 0x1) << 0) |
		   ((_mm_extract_epi8(qes[1], 15) & 0x1) << 1) |
		   ((_mm_extract_epi8(qes[2], 15) & 0x1) << 2) |
		   ((_mm_extract_epi8(qes[3], 15) & 0x1) << 3);

	/* XOR the combined bits such that a 1 represents a valid QE */
	gen_bits ^= xor_mask[gen_bit];

	/* Mask off gen bits we don't care about */
	gen_bits &= and_mask;

	return rte_popcount32(gen_bits);
}

static inline void
_process_deq_qes_vec_impl(struct dlb2_port *qm_port,
			  struct rte_event *events,
			  __m128i v_qe_3,
			  __m128i v_qe_2,
			  __m128i v_qe_1,
			  __m128i v_qe_0,
			  __m128i v_qe_meta,
			  __m128i v_qe_status,
			  uint32_t valid_events)
{
	/* Look up the event QIDs, using the hardware QIDs to index the
	 * port's QID mapping.
	 *
	 * Each v_qe_[0-4] is just a 16-byte load of the whole QE. It is
	 * passed along in registers as the QE data is required later.
	 *
	 * v_qe_meta is an u32 unpack of all 4x QEs. A.k.a, it contains one
	 * 32-bit slice of each QE, so makes up a full SSE register. This
	 * allows parallel processing of 4x QEs in a single register.
	 */

	__m128i v_qid_done = {0};
	int hw_qid0 = _mm_extract_epi8(v_qe_meta, 2);
	int hw_qid1 = _mm_extract_epi8(v_qe_meta, 6);
	int hw_qid2 = _mm_extract_epi8(v_qe_meta, 10);
	int hw_qid3 = _mm_extract_epi8(v_qe_meta, 14);

	int ev_qid0 = qm_port->qid_mappings[hw_qid0];
	int ev_qid1 = qm_port->qid_mappings[hw_qid1];
	int ev_qid2 = qm_port->qid_mappings[hw_qid2];
	int ev_qid3 = qm_port->qid_mappings[hw_qid3];

	int hw_sched0 = _mm_extract_epi8(v_qe_meta, 3) & 3ul;
	int hw_sched1 = _mm_extract_epi8(v_qe_meta, 7) & 3ul;
	int hw_sched2 = _mm_extract_epi8(v_qe_meta, 11) & 3ul;
	int hw_sched3 = _mm_extract_epi8(v_qe_meta, 15) & 3ul;

	v_qid_done = _mm_insert_epi8(v_qid_done, ev_qid0, 2);
	v_qid_done = _mm_insert_epi8(v_qid_done, ev_qid1, 6);
	v_qid_done = _mm_insert_epi8(v_qid_done, ev_qid2, 10);
	v_qid_done = _mm_insert_epi8(v_qid_done, ev_qid3, 14);

	/* Schedule field remapping using byte shuffle
	 * - Full byte containing sched field handled here (op, rsvd are zero)
	 * - Note sanitizing the register requires two masking ANDs:
	 *   1) to strip prio/msg_type from byte for correct shuffle lookup
	 *   2) to strip any non-sched-field lanes from any results to OR later
	 * - Final byte result is >> 10 to another byte-lane inside the u32.
	 *   This makes the final combination OR easier to make the rte_event.
	 */
	__m128i v_sched_done;
	__m128i v_sched_bits;
	{
		static const uint8_t sched_type_map[16] = {
			[DLB2_SCHED_ATOMIC] = RTE_SCHED_TYPE_ATOMIC,
			[DLB2_SCHED_UNORDERED] = RTE_SCHED_TYPE_PARALLEL,
			[DLB2_SCHED_ORDERED] = RTE_SCHED_TYPE_ORDERED,
			[DLB2_SCHED_DIRECTED] = RTE_SCHED_TYPE_ATOMIC,
		};
		static const uint8_t sched_and_mask[16] = {
			0x00, 0x00, 0x00, 0x03,
			0x00, 0x00, 0x00, 0x03,
			0x00, 0x00, 0x00, 0x03,
			0x00, 0x00, 0x00, 0x03,
		};

		static const uint8_t qid_depth_mask[16] = {
			0x00, 0x00, 0x00, 0x06,
			0x00, 0x00, 0x00, 0x06,
			0x00, 0x00, 0x00, 0x06,
			0x00, 0x00, 0x00, 0x06,
		};
		const __m128i v_qid_depth_mask  = _mm_loadu_si128(
						  (const __m128i *)qid_depth_mask);
		const __m128i v_sched_map = _mm_loadu_si128(
					     (const __m128i *)sched_type_map);
		__m128i v_sched_mask = _mm_loadu_si128(
					     (const __m128i *)&sched_and_mask);
		v_sched_bits = _mm_and_si128(v_qe_meta, v_sched_mask);
		__m128i v_sched_remapped = _mm_shuffle_epi8(v_sched_map,
							    v_sched_bits);
		__m128i v_preshift = _mm_and_si128(v_sched_remapped,
						   v_sched_mask);
		v_sched_done = _mm_srli_epi32(v_preshift, 10);
		__m128i v_qid_depth =  _mm_and_si128(v_qe_status, v_qid_depth_mask);
		v_qid_depth = _mm_srli_epi32(v_qid_depth, 15);
		v_sched_done = _mm_or_si128(v_sched_done, v_qid_depth);
	}

	/* Priority handling
	 * - QE provides 3 bits of priority
	 * - Shift << 3 to move to MSBs for byte-prio in rte_event
	 * - Mask bits to avoid pollution, leaving only 3 prio MSBs in reg
	 */
	__m128i v_prio_done;
	{
		static const uint8_t prio_mask[16] = {
			0x00, 0x00, 0x00, 0x07 << 5,
			0x00, 0x00, 0x00, 0x07 << 5,
			0x00, 0x00, 0x00, 0x07 << 5,
			0x00, 0x00, 0x00, 0x07 << 5,
		};
		__m128i v_prio_mask  = _mm_loadu_si128(
						(const __m128i *)prio_mask);
		__m128i v_prio_shifted = _mm_slli_epi32(v_qe_meta, 3);
		v_prio_done = _mm_and_si128(v_prio_shifted, v_prio_mask);
	}

	/* Event Sub/Type handling:
	 * we want to keep the lower 12 bits of each QE. Shift up by 20 bits
	 * to get the sub/ev type data into rte_event location, clearing the
	 * lower 20 bits in the process.
	 */
	__m128i v_types_done;
	{
		static const uint8_t event_mask[16] = {
			0x0f, 0x00, 0x00, 0x00,
			0x0f, 0x00, 0x00, 0x00,
			0x0f, 0x00, 0x00, 0x00,
			0x0f, 0x00, 0x00, 0x00,
		};
		static const uint8_t sub_event_mask[16] = {
			0xff, 0x00, 0x00, 0x00,
			0xff, 0x00, 0x00, 0x00,
			0xff, 0x00, 0x00, 0x00,
			0xff, 0x00, 0x00, 0x00,
		};
		static const uint8_t flow_mask[16] = {
			0xff, 0xff, 0x00, 0x00,
			0xff, 0xff, 0x00, 0x00,
			0xff, 0xff, 0x00, 0x00,
			0xff, 0xff, 0x00, 0x00,
		};
		__m128i v_event_mask  = _mm_loadu_si128(
					(const __m128i *)event_mask);
		__m128i v_sub_event_mask  = _mm_loadu_si128(
					(const __m128i *)sub_event_mask);
		__m128i v_flow_mask  = _mm_loadu_si128(
				       (const __m128i *)flow_mask);
		__m128i v_sub = _mm_srli_epi32(v_qe_meta, 4);
		v_sub = _mm_and_si128(v_sub, v_sub_event_mask);
		__m128i v_type = _mm_srli_epi32(v_qe_meta, 12);
		v_type = _mm_and_si128(v_type, v_event_mask);
		v_type = _mm_slli_epi32(v_type, 8);
		v_types_done = _mm_or_si128(v_type, v_sub);
		v_types_done = _mm_slli_epi32(v_types_done, 20);
		__m128i v_flow = _mm_and_si128(v_qe_status, v_flow_mask);
		v_types_done = _mm_or_si128(v_types_done, v_flow);
	}

	/* Combine QID, Sched and Prio fields, then Shift >> 8 bits to align
	 * with the rte_event, allowing unpacks to move/blend with payload.
	 */
	__m128i v_q_s_p_done;
	{
		__m128i v_qid_sched = _mm_or_si128(v_qid_done, v_sched_done);
		__m128i v_q_s_prio = _mm_or_si128(v_qid_sched, v_prio_done);
		v_q_s_p_done = _mm_srli_epi32(v_q_s_prio, 8);
	}

	__m128i v_unpk_ev_23, v_unpk_ev_01, v_ev_2, v_ev_3, v_ev_0, v_ev_1;

	/* Unpack evs into u64 metadata, then indiv events */
	v_unpk_ev_23 = _mm_unpackhi_epi32(v_types_done, v_q_s_p_done);
	v_unpk_ev_01 = _mm_unpacklo_epi32(v_types_done, v_q_s_p_done);

	switch (valid_events) {
	case 4:
		v_ev_3 = _mm_blend_epi16(v_unpk_ev_23, v_qe_3, 0x0F);
		v_ev_3 = _mm_alignr_epi8(v_ev_3, v_ev_3, 8);
		v_ev_3 = _mm_insert_epi8(v_ev_3, qm_port->reorder_id + 3, 7);
		_mm_storeu_si128((__m128i *)&events[3], v_ev_3);
		DLB2_INC_STAT(qm_port->ev_port->stats.rx_sched_cnt[hw_sched3],
			      1);
		DLB2_INC_STAT(qm_port->ev_port->stats.queue[ev_qid3].\
			qid_depth[RTE_PMD_DLB2_GET_QID_DEPTH(&events[3])], 1);
		/* fallthrough */
	case 3:
		v_ev_2 = _mm_unpacklo_epi64(v_unpk_ev_23, v_qe_2);
		v_ev_2 = _mm_insert_epi8(v_ev_2, qm_port->reorder_id + 2, 7);
		_mm_storeu_si128((__m128i *)&events[2], v_ev_2);
		DLB2_INC_STAT(qm_port->ev_port->stats.rx_sched_cnt[hw_sched2],
			      1);
		DLB2_INC_STAT(qm_port->ev_port->stats.queue[ev_qid2].\
			qid_depth[RTE_PMD_DLB2_GET_QID_DEPTH(&events[2])], 1);
		/* fallthrough */
	case 2:
		v_ev_1 = _mm_blend_epi16(v_unpk_ev_01, v_qe_1, 0x0F);
		v_ev_1 = _mm_alignr_epi8(v_ev_1, v_ev_1, 8);
		v_ev_1 = _mm_insert_epi8(v_ev_1, qm_port->reorder_id + 1, 7);
		_mm_storeu_si128((__m128i *)&events[1], v_ev_1);
		DLB2_INC_STAT(qm_port->ev_port->stats.rx_sched_cnt[hw_sched1],
			      1);
		DLB2_INC_STAT(qm_port->ev_port->stats.queue[ev_qid1].\
			qid_depth[RTE_PMD_DLB2_GET_QID_DEPTH(&events[1])], 1);
		/* fallthrough */
	case 1:
		v_ev_0 = _mm_unpacklo_epi64(v_unpk_ev_01, v_qe_0);
		v_ev_0 = _mm_insert_epi8(v_ev_0, qm_port->reorder_id, 7);
		_mm_storeu_si128((__m128i *)&events[0], v_ev_0);
		DLB2_INC_STAT(qm_port->ev_port->stats.rx_sched_cnt[hw_sched0],
			      1);
		DLB2_INC_STAT(qm_port->ev_port->stats.queue[ev_qid0].\
			qid_depth[RTE_PMD_DLB2_GET_QID_DEPTH(&events[0])], 1);
	}
	qm_port->reorder_id += valid_events;
}

static __rte_always_inline int
dlb2_recv_qe_sparse_vec(struct dlb2_port *qm_port, void *events,
			uint32_t max_events)
{
	/* Using unmasked idx for perf, and masking manually */
	uint16_t idx = qm_port->cq_idx_unmasked;
	volatile struct dlb2_dequeue_qe *cq_addr;

	cq_addr = dlb2_port[qm_port->id][PORT_TYPE(qm_port)].cq_base;

	uintptr_t qe_ptr_3 = (uintptr_t)&cq_addr[(idx + 12) &
						 qm_port->cq_depth_mask];
	uintptr_t qe_ptr_2 = (uintptr_t)&cq_addr[(idx +  8) &
						 qm_port->cq_depth_mask];
	uintptr_t qe_ptr_1 = (uintptr_t)&cq_addr[(idx +  4) &
						 qm_port->cq_depth_mask];
	uintptr_t qe_ptr_0 = (uintptr_t)&cq_addr[(idx +  0) &
						 qm_port->cq_depth_mask];

	/* Load QEs from CQ: use compiler barriers to avoid load reordering */
	__m128i v_qe_3 = _mm_loadu_si128((const __m128i *)qe_ptr_3);
	rte_compiler_barrier();
	__m128i v_qe_2 = _mm_loadu_si128((const __m128i *)qe_ptr_2);
	rte_compiler_barrier();
	__m128i v_qe_1 = _mm_loadu_si128((const __m128i *)qe_ptr_1);
	rte_compiler_barrier();
	__m128i v_qe_0 = _mm_loadu_si128((const __m128i *)qe_ptr_0);

	/* Generate the pkt_shuffle mask;
	 * - Avoids load in otherwise load-heavy section of code
	 * - Moves bytes 3,7,11,15 (gen bit bytes) to LSB bytes in XMM
	 */
	const uint32_t stat_shuf_bytes = (15 << 24) | (11 << 16) | (7 << 8) | 3;
	__m128i v_zeros = _mm_setzero_si128();
	__m128i v_ffff = _mm_cmpeq_epi8(v_zeros, v_zeros);
	__m128i v_stat_shuf_mask = _mm_insert_epi32(v_ffff, stat_shuf_bytes, 0);

	/* Extract u32 components required from the QE
	 * - QE[64 to 95 ] for metadata (qid, sched, prio, event type, ...)
	 * - QE[96 to 127] for status (cq gen bit, error)
	 *
	 * Note that stage 1 of the unpacking is re-used for both u32 extracts
	 */
	__m128i v_qe_02 = _mm_unpackhi_epi32(v_qe_0, v_qe_2);
	__m128i v_qe_13 = _mm_unpackhi_epi32(v_qe_1, v_qe_3);
	__m128i v_qe_status = _mm_unpackhi_epi32(v_qe_02, v_qe_13);
	__m128i v_qe_meta   = _mm_unpacklo_epi32(v_qe_02, v_qe_13);

	/* Status byte (gen_bit, error) handling:
	 * - Shuffle to lanes 0,1,2,3, clear all others
	 * - Shift right by 7 for gen bit to MSB, movemask to scalar
	 * - Shift right by 2 for error bit to MSB, movemask to scalar
	 */
	__m128i v_qe_shuffled = _mm_shuffle_epi8(v_qe_status, v_stat_shuf_mask);
	__m128i v_qes_shift_gen_bit = _mm_slli_epi32(v_qe_shuffled, 7);
	int32_t qe_gen_bits = _mm_movemask_epi8(v_qes_shift_gen_bit) & 0xf;

	/* Expected vs Reality of QE Gen bits
	 * - cq_rolling_mask provides expected bits
	 * - QE loads, unpacks/shuffle and movemask provides reality
	 * - XOR of the two gives bitmask of new packets
	 * - POPCNT to get the number of new events
	 */
	uint64_t rolling = qm_port->cq_rolling_mask & 0xF;
	uint64_t qe_xor_bits = (qe_gen_bits ^ rolling);
	uint32_t count_new = rte_popcount32(qe_xor_bits);
	count_new = RTE_MIN(count_new, max_events);
	if (!count_new)
		return 0;

	/* emulate a 128 bit rotate using 2x 64-bit numbers and bit-shifts */

	uint64_t m_rshift = qm_port->cq_rolling_mask >> count_new;
	uint64_t m_lshift = qm_port->cq_rolling_mask << (64 - count_new);
	uint64_t m2_rshift = qm_port->cq_rolling_mask_2 >> count_new;
	uint64_t m2_lshift = qm_port->cq_rolling_mask_2 << (64 - count_new);

	/* shifted out of m2 into MSB of m */
	qm_port->cq_rolling_mask = (m_rshift | m2_lshift);

	/* shifted out of m "looped back" into MSB of m2 */
	qm_port->cq_rolling_mask_2 = (m2_rshift | m_lshift);

	/* Prefetch the next QEs - should run as IPC instead of cycles */
	rte_prefetch0(&cq_addr[(idx + 16) & qm_port->cq_depth_mask]);
	rte_prefetch0(&cq_addr[(idx + 20) & qm_port->cq_depth_mask]);
	rte_prefetch0(&cq_addr[(idx + 24) & qm_port->cq_depth_mask]);
	rte_prefetch0(&cq_addr[(idx + 28) & qm_port->cq_depth_mask]);

	/* Convert QEs from XMM regs to events and store events directly */
	_process_deq_qes_vec_impl(qm_port, events, v_qe_3, v_qe_2, v_qe_1,
				  v_qe_0, v_qe_meta, v_qe_status, count_new);

	return count_new;
}

static inline void
dlb2_inc_cq_idx(struct dlb2_port *qm_port, int cnt)
{
	uint16_t idx = qm_port->cq_idx_unmasked + cnt;

	qm_port->cq_idx_unmasked = idx;
	qm_port->cq_idx = idx & qm_port->cq_depth_mask;
	qm_port->gen_bit = (~(idx >> qm_port->gen_bit_shift)) & 0x1;
}

static inline int16_t
dlb2_hw_dequeue_sparse(struct dlb2_eventdev *dlb2,
		       struct dlb2_eventdev_port *ev_port,
		       struct rte_event *events,
		       uint16_t max_num,
		       uint64_t dequeue_timeout_ticks)
{
	uint64_t start_ticks = 0ULL;
	struct dlb2_port *qm_port;
	int num = 0;
	bool use_scalar;
	uint64_t timeout;

	qm_port = &ev_port->qm_port;
	use_scalar = qm_port->use_scalar;

	if (!dlb2->global_dequeue_wait)
		timeout = dequeue_timeout_ticks;
	else
		timeout = dlb2->global_dequeue_wait_ticks;

	if (timeout != 0)
		start_ticks = rte_get_timer_cycles();

	use_scalar = use_scalar || (max_num & 0x3);

	while (num < max_num) {
		struct dlb2_dequeue_qe qes[DLB2_NUM_QES_PER_CACHE_LINE];
		int num_avail;

		if (use_scalar) {
			int n_iter = 0;
			uint64_t m_rshift, m_lshift, m2_rshift, m2_lshift;

			num_avail = dlb2_recv_qe_sparse(qm_port, qes);
			num_avail = RTE_MIN(num_avail, max_num - num);
			dlb2_inc_cq_idx(qm_port, num_avail << 2);
			if (num_avail == DLB2_NUM_QES_PER_CACHE_LINE)
				n_iter = dlb2_process_dequeue_four_qes(ev_port,
								qm_port,
								&events[num],
								&qes[0]);
			else if (num_avail)
				n_iter = dlb2_process_dequeue_qes(ev_port,
								qm_port,
								&events[num],
								&qes[0],
								num_avail);
			if (n_iter != 0) {
				num += n_iter;
				/* update rolling_mask for vector code support */
				m_rshift = qm_port->cq_rolling_mask >> n_iter;
				m_lshift = qm_port->cq_rolling_mask << (64 - n_iter);
				m2_rshift = qm_port->cq_rolling_mask_2 >> n_iter;
				m2_lshift = qm_port->cq_rolling_mask_2 <<
					(64 - n_iter);
				qm_port->cq_rolling_mask = (m_rshift | m2_lshift);
				qm_port->cq_rolling_mask_2 = (m2_rshift | m_lshift);
			}
		} else { /* !use_scalar */
			num_avail = dlb2_recv_qe_sparse_vec(qm_port,
							    &events[num],
							    max_num - num);
			dlb2_inc_cq_idx(qm_port, num_avail << 2);
			num += num_avail;
			DLB2_INC_STAT(ev_port->stats.traffic.rx_ok, num_avail);
		}
		if (!num_avail) {
			if ((timeout == 0) || (num > 0))
				/* Not waiting in any form or 1+ events recd */
				break;
			else if (dlb2_dequeue_wait(dlb2, ev_port, qm_port,
						   timeout, start_ticks))
				break;
		}
	}

	qm_port->owed_tokens += num;

	if (num) {
		if (qm_port->token_pop_mode == RTE_PMD_DLB2_AUTO_POP)
			dlb2_consume_qe_immediate(qm_port, num);

		ev_port->outstanding_releases += num;

#if DLB_HW_CREDITS_CHECKS
		dlb2_port_credits_inc(qm_port, num);
#endif
	}

	return num;
}

static __rte_always_inline int
dlb2_recv_qe(struct dlb2_port *qm_port, struct dlb2_dequeue_qe *qe,
	     uint8_t *offset)
{
	uint8_t xor_mask[2][4] = { {0x0F, 0x0E, 0x0C, 0x08},
				   {0x00, 0x01, 0x03, 0x07} };
	uint8_t and_mask[4] = {0x0F, 0x0E, 0x0C, 0x08};
	volatile struct dlb2_dequeue_qe *cq_addr;
	__m128i *qes = (__m128i *)qe;
	uint64_t *cache_line_base;
	uint8_t gen_bits;

	cq_addr = dlb2_port[qm_port->id][PORT_TYPE(qm_port)].cq_base;
	cq_addr = &cq_addr[qm_port->cq_idx];

	cache_line_base = (void *)(((uintptr_t)cq_addr) & ~0x3F);
	*offset = ((uintptr_t)cq_addr & 0x30) >> 4;

	/* Load the next CQ cache line from memory. Pack these reads as tight
	 * as possible to reduce the chance that DLB invalidates the line while
	 * the CPU is reading it. Read the cache line backwards to ensure that
	 * if QE[N] (N > 0) is valid, then QEs[0:N-1] are too.
	 *
	 * (Valid QEs start at &qe[offset])
	 */
	qes[3] = _mm_load_si128((__m128i *)&cache_line_base[6]);
	qes[2] = _mm_load_si128((__m128i *)&cache_line_base[4]);
	qes[1] = _mm_load_si128((__m128i *)&cache_line_base[2]);
	qes[0] = _mm_load_si128((__m128i *)&cache_line_base[0]);

	/* Evict the cache line ASAP */
	rte_cldemote(cache_line_base);

	/* Extract and combine the gen bits */
	gen_bits = ((_mm_extract_epi8(qes[0], 15) & 0x1) << 0) |
		   ((_mm_extract_epi8(qes[1], 15) & 0x1) << 1) |
		   ((_mm_extract_epi8(qes[2], 15) & 0x1) << 2) |
		   ((_mm_extract_epi8(qes[3], 15) & 0x1) << 3);

	/* XOR the combined bits such that a 1 represents a valid QE */
	gen_bits ^= xor_mask[qm_port->gen_bit][*offset];

	/* Mask off gen bits we don't care about */
	gen_bits &= and_mask[*offset];

	return rte_popcount32(gen_bits);
}

static inline int16_t
dlb2_hw_dequeue(struct dlb2_eventdev *dlb2,
		struct dlb2_eventdev_port *ev_port,
		struct rte_event *events,
		uint16_t max_num,
		uint64_t dequeue_timeout_ticks)
{
	uint64_t timeout;
	uint64_t start_ticks = 0ULL;
	struct dlb2_port *qm_port;
	int num = 0;

	qm_port = &ev_port->qm_port;

	/* We have a special implementation for waiting. Wait can be:
	 * 1) no waiting at all
	 * 2) busy poll only
	 * 3) wait for interrupt. If wakeup and poll time
	 * has expired, then return to caller
	 * 4) umonitor/umwait repeatedly up to poll time
	 */

	/* If configured for per dequeue wait, then use wait value provided
	 * to this API. Otherwise we must use the global
	 * value from eventdev config time.
	 */
	if (!dlb2->global_dequeue_wait)
		timeout = dequeue_timeout_ticks;
	else
		timeout = dlb2->global_dequeue_wait_ticks;

	if (timeout != 0)
		start_ticks = rte_get_timer_cycles();

	while (num < max_num) {
		struct dlb2_dequeue_qe qes[DLB2_NUM_QES_PER_CACHE_LINE];
		uint8_t offset;
		int num_avail;

		/* Copy up to 4 QEs from the current cache line into qes */
		num_avail = dlb2_recv_qe(qm_port, qes, &offset);

		/* But don't process more than the user requested */
		num_avail = RTE_MIN(num_avail, max_num - num);

		dlb2_inc_cq_idx(qm_port, num_avail);

		if (num_avail == DLB2_NUM_QES_PER_CACHE_LINE)
			num += dlb2_process_dequeue_four_qes(ev_port,
							     qm_port,
							     &events[num],
							     &qes[offset]);
		else if (num_avail)
			num += dlb2_process_dequeue_qes(ev_port,
							qm_port,
							&events[num],
							&qes[offset],
							num_avail);
		else if ((timeout == 0) || (num > 0))
			/* Not waiting in any form, or 1+ events received? */
			break;
		else if (dlb2_dequeue_wait(dlb2, ev_port, qm_port,
					   timeout, start_ticks))
			break;
	}

	qm_port->owed_tokens += num;

	if (num) {
		if (qm_port->token_pop_mode == RTE_PMD_DLB2_AUTO_POP)
			dlb2_consume_qe_immediate(qm_port, num);

		ev_port->outstanding_releases += num;

#if DLB_HW_CREDITS_CHECKS
		dlb2_port_credits_inc(qm_port, num);
#endif
	}

	return num;
}

static uint16_t
dlb2_event_dequeue_burst(void *event_port, struct rte_event *ev, uint16_t num,
			 uint64_t wait)
{
	struct dlb2_eventdev_port *ev_port = event_port;
	struct dlb2_port *qm_port = &ev_port->qm_port;
	struct dlb2_eventdev *dlb2 = ev_port->dlb2;
	struct dlb2_reorder *order = qm_port->order;
	struct process_local_port_data *port_data;
	uint16_t cnt;

	RTE_ASSERT(ev_port->setup_done);
	RTE_ASSERT(ev != NULL);

	port_data = &dlb2_port[qm_port->id][PORT_TYPE(qm_port)];

	if (!port_data->mmaped)
		dlb2_iface_low_level_io_init(&dlb2->qm_instance);

	if (ev_port->implicit_release && ev_port->outstanding_releases > 0) {
		uint16_t out_rels = ev_port->outstanding_releases;
		if (qm_port->reorder_en) {
			/* for directed, no-op command-byte = 0, but set dsi field */
			/* for load-balanced, set COMP */
			uint64_t release_u64 =
			    qm_port->is_directed ? 0xFF : (uint64_t)DLB2_COMP_CMD_BYTE << 56;

			for (uint8_t i = order->next_to_enqueue; i != qm_port->reorder_id; i++)
				if (order->enq_reorder[i].u64[1] == 0)
					order->enq_reorder[i].u64[1] = release_u64;

			__dlb2_event_enqueue_burst_reorder(event_port, NULL, 0,
					qm_port->token_pop_mode == RTE_PMD_DLB2_DELAYED_POP);
		} else {
			dlb2_event_release(dlb2, ev_port->id, out_rels);
		}

		DLB2_INC_STAT(ev_port->stats.tx_implicit_rel, out_rels);
	}

	if (qm_port->token_pop_mode == RTE_PMD_DLB2_DEFERRED_POP && qm_port->owed_tokens)
		dlb2_consume_qe_immediate(qm_port, qm_port->owed_tokens);

	cnt = dlb2_hw_dequeue(dlb2, ev_port, ev, num, wait);

	DLB2_INC_STAT(ev_port->stats.traffic.total_polls, 1);
	DLB2_INC_STAT(ev_port->stats.traffic.zero_polls, ((cnt == 0) ? 1 : 0));
	dlb2_check_and_return_credits(ev_port, !cnt,
				      DLB2_ZERO_DEQ_CREDIT_RETURN_THRES);

	return cnt;
}

static uint16_t
dlb2_event_dequeue_burst_sparse(void *event_port, struct rte_event *ev,
				uint16_t num, uint64_t wait)
{
	struct dlb2_eventdev_port *ev_port = event_port;
	struct dlb2_port *qm_port = &ev_port->qm_port;
	struct dlb2_eventdev *dlb2 = ev_port->dlb2;
	struct dlb2_reorder *order = qm_port->order;
	struct process_local_port_data *port_data;
	uint16_t cnt;

	RTE_ASSERT(ev_port->setup_done);
	RTE_ASSERT(ev != NULL);

	port_data = &dlb2_port[qm_port->id][PORT_TYPE(qm_port)];

	if (!port_data->mmaped)
		dlb2_iface_low_level_io_init(&dlb2->qm_instance);

	if (ev_port->implicit_release && ev_port->outstanding_releases > 0) {
		uint16_t out_rels = ev_port->outstanding_releases;
		if (qm_port->reorder_en) {
			struct rte_event release_burst[8];
			int num_releases = 0;

			/* go through reorder buffer looking for missing releases. */
			for (uint8_t i = order->next_to_enqueue; i != qm_port->reorder_id; i++) {
				if (order->enq_reorder[i].u64[1] == 0) {
					release_burst[num_releases++] = (struct rte_event){
						.op = RTE_EVENT_OP_RELEASE,
							.impl_opaque = i,
					};

					if (num_releases == RTE_DIM(release_burst)) {
						__dlb2_event_enqueue_burst_reorder(event_port,
							release_burst, RTE_DIM(release_burst),
							qm_port->token_pop_mode ==
									RTE_PMD_DLB2_DELAYED_POP);
						num_releases = 0;
					}
				}
			}

			if (num_releases)
				__dlb2_event_enqueue_burst_reorder(event_port, release_burst
					, num_releases,
					qm_port->token_pop_mode == RTE_PMD_DLB2_DELAYED_POP);
		} else {
			dlb2_event_release(dlb2, ev_port->id, out_rels);
		}

		RTE_ASSERT(ev_port->outstanding_releases == 0);
		DLB2_INC_STAT(ev_port->stats.tx_implicit_rel, out_rels);
	}

	if (qm_port->token_pop_mode == RTE_PMD_DLB2_DEFERRED_POP && qm_port->owed_tokens)
		dlb2_consume_qe_immediate(qm_port, qm_port->owed_tokens);

	cnt = dlb2_hw_dequeue_sparse(dlb2, ev_port, ev, num, wait);

	DLB2_INC_STAT(ev_port->stats.traffic.total_polls, 1);
	DLB2_INC_STAT(ev_port->stats.traffic.zero_polls, ((cnt == 0) ? 1 : 0));
	dlb2_check_and_return_credits(ev_port, !cnt,
				      DLB2_ZERO_DEQ_CREDIT_RETURN_THRES);

	return cnt;
}

static void
dlb2_flush_port(struct rte_eventdev *dev, int port_id)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	struct dlb2_eventdev_port *ev_port = &dlb2->ev_ports[port_id];
	struct dlb2_reorder *order = ev_port->qm_port.order;
	eventdev_stop_flush_t flush;
	struct rte_event ev;
	uint8_t dev_id;
	void *arg;
	int i;

	flush = dev->dev_ops->dev_stop_flush;
	dev_id = dev->data->dev_id;
	arg = dev->data->dev_stop_flush_arg;

	while (rte_event_dequeue_burst(dev_id, port_id, &ev, 1, 0)) {
		if (flush)
			flush(dev_id, ev, arg);

		if (dlb2->ev_ports[port_id].qm_port.is_directed)
			continue;

		ev.op = RTE_EVENT_OP_RELEASE;

		rte_event_enqueue_burst(dev_id, port_id, &ev, 1);
	}

	/* Enqueue any additional outstanding releases */
	ev.op = RTE_EVENT_OP_RELEASE;

	for (i = dlb2->ev_ports[port_id].outstanding_releases; i > 0; i--) {
		ev.impl_opaque = order ? order->next_to_enqueue : 0;
		rte_event_enqueue_burst(dev_id, port_id, &ev, 1);
	}
}

static uint32_t
dlb2_get_ldb_queue_depth(struct dlb2_eventdev *dlb2,
			 struct dlb2_eventdev_queue *queue)
{
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	struct dlb2_get_ldb_queue_depth_args cfg;
	int ret;

	cfg.queue_id = queue->qm_queue.id;

	ret = dlb2_iface_get_ldb_queue_depth(handle, &cfg);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: get_ldb_queue_depth ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		return ret;
	}

	return cfg.response.id;
}

static uint32_t
dlb2_get_dir_queue_depth(struct dlb2_eventdev *dlb2,
			 struct dlb2_eventdev_queue *queue)
{
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	struct dlb2_get_dir_queue_depth_args cfg;
	int ret;

	cfg.queue_id = queue->qm_queue.id;

	ret = dlb2_iface_get_dir_queue_depth(handle, &cfg);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: get_dir_queue_depth ret=%d (driver status: %s)",
			     ret, dlb2_error_strings[cfg.response.status]);
		return ret;
	}

	return cfg.response.id;
}

uint32_t
dlb2_get_queue_depth(struct dlb2_eventdev *dlb2,
		     struct dlb2_eventdev_queue *queue)
{
	if (queue->qm_queue.is_directed)
		return dlb2_get_dir_queue_depth(dlb2, queue);
	else
		return dlb2_get_ldb_queue_depth(dlb2, queue);
}

#define PARAM_ERR(param, ret, err_str)\
	do { \
		if (!ret) \
			ret = -EINVAL; \
		DLB2_LOG_ERR("dlb2: dlb2_set_port_param error, param=%" PRIu64 " ret=%d %s",\
			param, ret, err_str); \
	} while (0)

int
dlb2_set_port_param(struct dlb2_eventdev *dlb2,
		    int port_id,
		    uint64_t param_flags,
		    struct rte_pmd_dlb2_port_param *param_val)
{
	struct rte_pmd_dlb2_port_param *port_param = param_val;
	struct dlb2_port *port = &dlb2->ev_ports[port_id].qm_port;
	struct dlb2_hw_dev *handle = &dlb2->qm_instance;
	int ret = 0, bit = 0;

	while (param_flags) {
		uint64_t param = rte_bit_relaxed_test_and_clear64(bit++, &param_flags);

		if (!param)
			continue;
		switch (param) {
		case DLB2_SET_PORT_FLOW_MIGRATION_THRESHOLD:
			if (dlb2->version == DLB2_HW_V2_5) {
				struct dlb2_cq_inflight_ctrl_args args = {0};

				args.enable = true;
				args.port_id = port->id;
				args.threshold = port_param->inflight_threshold;
				if (dlb2->ev_ports[port_id].setup_done)
					ret = dlb2_iface_set_cq_inflight_ctrl(handle, &args);
				if (ret) {
					PARAM_ERR(param, ret, "Failed to set inflight threshold");
					return ret;
				}
				port->enable_inflight_ctrl = true;
				port->inflight_threshold = args.threshold;
			} else {
				PARAM_ERR(param, ret, "FLOW_MIGRATION_THRESHOLD is only supported for 2.5 HW");
				return ret;
			}
			break;
		case DLB2_SET_PORT_HL:
			if (dlb2->ev_ports[port_id].setup_done) {
				PARAM_ERR(param, ret, "DLB2_SET_PORT_HL must be called before setting up port");
				return ret;
			}
			port->hist_list = port_param->port_hl;
			break;
		default:
			PARAM_ERR(param, ret, "Unsupported flag");
			return ret;
		}
	}

	return ret;
}

static bool
dlb2_queue_is_empty(struct dlb2_eventdev *dlb2,
		    struct dlb2_eventdev_queue *queue)
{
	return dlb2_get_queue_depth(dlb2, queue) == 0;
}

static bool
dlb2_linked_queues_empty(struct dlb2_eventdev *dlb2)
{
	int i;

	for (i = 0; i < dlb2->num_queues; i++) {
		if (dlb2->ev_queues[i].num_links == 0)
			continue;
		if (!dlb2_queue_is_empty(dlb2, &dlb2->ev_queues[i]))
			return false;
	}

	return true;
}

static bool
dlb2_queues_empty(struct dlb2_eventdev *dlb2)
{
	int i;

	for (i = 0; i < dlb2->num_queues; i++) {
		if (!dlb2_queue_is_empty(dlb2, &dlb2->ev_queues[i]))
			return false;
	}

	return true;
}

static void
dlb2_drain(struct rte_eventdev *dev)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
	struct dlb2_eventdev_port *ev_port = NULL;
	uint8_t dev_id;
	int i;

	dev_id = dev->data->dev_id;

	while (!dlb2_linked_queues_empty(dlb2)) {
		/* Flush all the ev_ports, which will drain all their connected
		 * queues.
		 */
		for (i = 0; i < dlb2->num_ports; i++)
			dlb2_flush_port(dev, i);
	}

	/* The queues are empty, but there may be events left in the ports. */
	for (i = 0; i < dlb2->num_ports; i++)
		dlb2_flush_port(dev, i);

	/* If the domain's queues are empty, we're done. */
	if (dlb2_queues_empty(dlb2))
		return;

	/* Else, there must be at least one unlinked load-balanced queue.
	 * Select a load-balanced port with which to drain the unlinked
	 * queue(s).
	 */
	for (i = 0; i < dlb2->num_ports; i++) {
		ev_port = &dlb2->ev_ports[i];

		if (!ev_port->qm_port.is_directed)
			break;
	}

	if (i == dlb2->num_ports) {
		DLB2_LOG_ERR("internal error: no LDB ev_ports");
		return;
	}

	rte_errno = 0;
	rte_event_port_unlink(dev_id, ev_port->id, NULL, 0);

	if (rte_errno) {
		DLB2_LOG_ERR("internal error: failed to unlink ev_port %d",
			     ev_port->id);
		return;
	}

	for (i = 0; i < dlb2->num_queues; i++) {
		uint8_t qid, prio;
		int ret;

		if (dlb2_queue_is_empty(dlb2, &dlb2->ev_queues[i]))
			continue;

		qid = i;
		prio = 0;

		/* Link the ev_port to the queue */
		ret = rte_event_port_link(dev_id, ev_port->id, &qid, &prio, 1);
		if (ret != 1) {
			DLB2_LOG_ERR("internal error: failed to link ev_port %d to queue %d",
				     ev_port->id, qid);
			return;
		}

		/* Flush the queue */
		while (!dlb2_queue_is_empty(dlb2, &dlb2->ev_queues[i]))
			dlb2_flush_port(dev, ev_port->id);

		/* Drain any extant events in the ev_port. */
		dlb2_flush_port(dev, ev_port->id);

		/* Unlink the ev_port from the queue */
		ret = rte_event_port_unlink(dev_id, ev_port->id, &qid, 1);
		if (ret != 1) {
			DLB2_LOG_ERR("internal error: failed to unlink ev_port %d to queue %d",
				     ev_port->id, qid);
			return;
		}
	}
}

static void
dlb2_eventdev_stop(struct rte_eventdev *dev)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);

	rte_spinlock_lock(&dlb2->qm_instance.resource_lock);

	if (dlb2->run_state == DLB2_RUN_STATE_STOPPED) {
		DLB2_LOG_LINE_DBG("Internal error: already stopped");
		rte_spinlock_unlock(&dlb2->qm_instance.resource_lock);
		return;
	} else if (dlb2->run_state != DLB2_RUN_STATE_STARTED) {
		DLB2_LOG_ERR("Internal error: bad state %d for dev_stop",
			     (int)dlb2->run_state);
		rte_spinlock_unlock(&dlb2->qm_instance.resource_lock);
		return;
	}

	dlb2->run_state = DLB2_RUN_STATE_STOPPING;

	rte_spinlock_unlock(&dlb2->qm_instance.resource_lock);

	dlb2_drain(dev);

	dlb2->run_state = DLB2_RUN_STATE_STOPPED;
}

static int
dlb2_eventdev_close(struct rte_eventdev *dev)
{
	dlb2_hw_reset_sched_domain(dev, false);

	return 0;
}

static void
dlb2_eventdev_queue_release(struct rte_eventdev *dev, uint8_t id)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(id);

	/* This function intentionally left blank. */
}

static void
dlb2_eventdev_port_release(void *port)
{
	struct dlb2_eventdev_port *ev_port = port;
	struct dlb2_port *qm_port;

	if (ev_port) {
		qm_port = &ev_port->qm_port;
		if (qm_port->config_state == DLB2_CONFIGURED)
			dlb2_free_qe_mem(qm_port);
	}
}

static int
dlb2_eventdev_timeout_ticks(struct rte_eventdev *dev, uint64_t ns,
			    uint64_t *timeout_ticks)
{
	RTE_SET_USED(dev);
	uint64_t cycles_per_ns = rte_get_timer_hz() / 1E9;

	*timeout_ticks = ns * cycles_per_ns;

	return 0;
}

static void
dlb2_entry_points_init(struct rte_eventdev *dev)
{
	struct dlb2_eventdev *dlb2;

	/* Expose PMD's eventdev interface */
	static struct eventdev_ops dlb2_eventdev_entry_ops = {
		.dev_infos_get    = dlb2_eventdev_info_get,
		.dev_configure    = dlb2_eventdev_configure,
		.dev_start        = dlb2_eventdev_start,
		.dev_stop         = dlb2_eventdev_stop,
		.dev_close        = dlb2_eventdev_close,
		.queue_def_conf   = dlb2_eventdev_queue_default_conf_get,
		.queue_setup      = dlb2_eventdev_queue_setup,
		.queue_release    = dlb2_eventdev_queue_release,
		.port_def_conf    = dlb2_eventdev_port_default_conf_get,
		.port_setup       = dlb2_eventdev_port_setup,
		.port_release     = dlb2_eventdev_port_release,
		.port_link        = dlb2_eventdev_port_link,
		.port_unlink      = dlb2_eventdev_port_unlink,
		.port_unlinks_in_progress =
				    dlb2_eventdev_port_unlinks_in_progress,
		.timeout_ticks    = dlb2_eventdev_timeout_ticks,
		.dump             = dlb2_eventdev_dump,
		.xstats_get       = dlb2_eventdev_xstats_get,
		.xstats_get_names = dlb2_eventdev_xstats_get_names,
		.xstats_get_by_name = dlb2_eventdev_xstats_get_by_name,
		.xstats_reset	    = dlb2_eventdev_xstats_reset,
		.dev_selftest     = test_dlb2_eventdev,
	};

	/* Expose PMD's eventdev interface */

	dev->dev_ops = &dlb2_eventdev_entry_ops;
	dev->enqueue_burst = dlb2_event_enqueue_burst;
	dev->enqueue_new_burst = dlb2_event_enqueue_new_burst;
	dev->enqueue_forward_burst = dlb2_event_enqueue_forward_burst;

	dlb2 = dev->data->dev_private;
	if (dlb2->poll_mode == DLB2_CQ_POLL_MODE_SPARSE)
		dev->dequeue_burst = dlb2_event_dequeue_burst_sparse;
	else
		dev->dequeue_burst = dlb2_event_dequeue_burst;
}

int
dlb2_primary_eventdev_probe(struct rte_eventdev *dev,
			    const char *name,
			    struct dlb2_devargs *dlb2_args)
{
	struct dlb2_eventdev *dlb2;
	int err, i;

	dlb2 = dev->data->dev_private;

	dlb2->event_dev = dev; /* backlink */

	evdev_dlb2_default_info.driver_name = name;

	dlb2->max_num_events_override = dlb2_args->max_num_events;
	dlb2->num_dir_credits_override = dlb2_args->num_dir_credits_override;
	dlb2->poll_interval = dlb2_args->poll_interval;
	dlb2->sw_credit_quanta = dlb2_args->sw_credit_quanta;
	dlb2->hw_credit_quanta = dlb2_args->hw_credit_quanta;
	dlb2->default_depth_thresh = dlb2_args->default_depth_thresh;
	dlb2->vector_opts_enabled = dlb2_args->vector_opts_enabled;
	dlb2->enable_cq_weight = dlb2_args->enable_cq_weight;


	if (dlb2_args->max_cq_depth != 0)
		dlb2->max_cq_depth = dlb2_args->max_cq_depth;
	else
		dlb2->max_cq_depth = DLB2_DEFAULT_CQ_DEPTH;

	evdev_dlb2_default_info.max_event_port_dequeue_depth = dlb2->max_cq_depth;

	if (dlb2_args->max_enq_depth != 0)
		dlb2->max_enq_depth = dlb2_args->max_enq_depth;
	else
		dlb2->max_enq_depth = DLB2_DEFAULT_CQ_DEPTH;

	evdev_dlb2_default_info.max_event_port_enqueue_depth =
		dlb2->max_enq_depth;

	dlb2_init_queue_depth_thresholds(dlb2,
					 dlb2_args->qid_depth_thresholds.val);

	dlb2_init_port_cos(dlb2,
			   dlb2_args->port_cos.cos_id);

	dlb2_init_cos_bw(dlb2,
			 &dlb2_args->cos_bw);

	err = dlb2_iface_open(&dlb2->qm_instance, name);
	if (err < 0) {
		DLB2_LOG_ERR("could not open event hardware device, err=%d",
			     err);
		return err;
	}

	err = dlb2_iface_get_device_version(&dlb2->qm_instance,
					    &dlb2->revision);
	if (err < 0) {
		DLB2_LOG_ERR("dlb2: failed to get the device version, err=%d",
			     err);
		return err;
	}

	err = dlb2_hw_query_resources(dlb2);
	if (err) {
		DLB2_LOG_ERR("get resources err=%d for %s",
			     err, name);
		return err;
	}

	if (dlb2_args->use_default_hl) {
		dlb2->default_port_hl = DLB2_FIXED_CQ_HL_SIZE;
		if (dlb2_args->alloc_hl_entries)
			DLB2_LOG_ERR(": Ignoring 'alloc_hl_entries' and using "
				     "default history list sizes for eventdev:"
				     " %s", dev->data->name);
		dlb2->hl_entries = 0;
	} else {
		dlb2->default_port_hl = 2 * DLB2_FIXED_CQ_HL_SIZE;

		if (dlb2_args->alloc_hl_entries >
		    dlb2->hw_rsrc_query_results.num_hist_list_entries) {
			DLB2_LOG_ERR(": Insufficient HL entries asked=%d "
				     "available=%d for eventdev: %s",
				     dlb2->hl_entries,
				     dlb2->hw_rsrc_query_results.num_hist_list_entries,
				     dev->data->name);
			return -EINVAL;
		}
		dlb2->hl_entries = dlb2_args->alloc_hl_entries;
	}

	dlb2_iface_hardware_init(&dlb2->qm_instance);

	/* configure class of service */
	{
		struct dlb2_set_cos_bw_args
			set_cos_bw_args = { {0} };
		int id;
		int ret = 0;

		for (id = 0; id < DLB2_COS_NUM_VALS; id++) {
			set_cos_bw_args.cos_id = id;
			set_cos_bw_args.bandwidth = dlb2->cos_bw[id];
			ret = dlb2_iface_set_cos_bw(&dlb2->qm_instance,
						    &set_cos_bw_args);
			if (ret != 0)
				break;
		}
		if (ret) {
			DLB2_LOG_ERR("dlb2: failed to configure class of service, err=%d",
				     err);
			return err;
		}
	}

	err = dlb2_iface_get_cq_poll_mode(&dlb2->qm_instance, &dlb2->poll_mode);
	if (err < 0) {
		DLB2_LOG_ERR("dlb2: failed to get the poll mode, err=%d",
			     err);
		return err;
	}

	/* Complete xtstats runtime initialization */
	err = dlb2_xstats_init(dlb2);
	if (err) {
		DLB2_LOG_ERR("dlb2: failed to init xstats, err=%d", err);
		return err;
	}

	/* Initialize each port's token pop mode */
	for (i = 0; i < DLB2_MAX_NUM_PORTS(dlb2->version); i++)
		dlb2->ev_ports[i].qm_port.token_pop_mode = RTE_PMD_DLB2_AUTO_POP;

	rte_spinlock_init(&dlb2->qm_instance.resource_lock);

	dlb2_iface_low_level_io_init(NULL);

	dlb2_entry_points_init(dev);

	return 0;
}

int
dlb2_secondary_eventdev_probe(struct rte_eventdev *dev,
			      const char *name)
{
	struct dlb2_eventdev *dlb2;
	int err;

	dlb2 = dev->data->dev_private;

	evdev_dlb2_default_info.driver_name = name;

	err = dlb2_iface_open(&dlb2->qm_instance, name);
	if (err < 0) {
		DLB2_LOG_ERR("could not open event hardware device, err=%d",
			     err);
		return err;
	}

	err = dlb2_hw_query_resources(dlb2);
	if (err) {
		DLB2_LOG_ERR("get resources err=%d for %s",
			     err, name);
		return err;
	}

	dlb2_iface_low_level_io_init(&dlb2->qm_instance);

	dlb2_entry_points_init(dev);

	return 0;
}

int
dlb2_parse_params(const char *params,
		  const char *name,
		  struct dlb2_devargs *dlb2_args,
		  uint8_t version)
{
	int ret = 0;
	static const char * const args[] = { NUMA_NODE_ARG,
					     DLB2_MAX_NUM_EVENTS,
					     DLB2_NUM_DIR_CREDITS,
					     DEV_ID_ARG,
					     DLB2_QID_DEPTH_THRESH_ARG,
					     DLB2_POLL_INTERVAL_ARG,
					     DLB2_SW_CREDIT_QUANTA_ARG,
					     DLB2_HW_CREDIT_QUANTA_ARG,
					     DLB2_DEPTH_THRESH_ARG,
					     DLB2_VECTOR_OPTS_ENAB_ARG,
					     DLB2_MAX_CQ_DEPTH,
					     DLB2_MAX_ENQ_DEPTH,
					     DLB2_PORT_COS,
					     DLB2_COS_BW,
					     DLB2_PRODUCER_COREMASK,
					     DLB2_DEFAULT_LDB_PORT_ALLOCATION_ARG,
					     DLB2_ENABLE_CQ_WEIGHT_ARG,
					     DLB2_USE_DEFAULT_HL,
					     DLB2_ALLOC_HL_ENTRIES,
					     NULL };

	if (params != NULL && params[0] != '\0') {
		struct rte_kvargs *kvlist = rte_kvargs_parse(params, args);

		if (kvlist == NULL) {
			DLB2_LOG_INFO("Ignoring unsupported parameters when creating device '%s'",
				      name);
		} else {
			int ret = rte_kvargs_process(kvlist, NUMA_NODE_ARG,
						     set_numa_node,
						     &dlb2_args->socket_id);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing numa node parameter",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			if (version == DLB2_HW_V2) {
				ret = rte_kvargs_process(kvlist,
						DLB2_MAX_NUM_EVENTS,
						set_max_num_events,
						&dlb2_args->max_num_events);
			} else {
				ret = rte_kvargs_process(kvlist,
						DLB2_MAX_NUM_EVENTS,
						set_max_num_events_v2_5,
						&dlb2_args->max_num_events);
			}

			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing max_num_events parameter",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			if (version == DLB2_HW_V2) {
				ret = rte_kvargs_process(kvlist,
					DLB2_NUM_DIR_CREDITS,
					set_num_dir_credits,
					&dlb2_args->num_dir_credits_override);
				if (ret != 0) {
					DLB2_LOG_ERR("%s: Error parsing num_dir_credits parameter",
						     name);
					rte_kvargs_free(kvlist);
					return ret;
				}
			}
			ret = rte_kvargs_process(kvlist, DEV_ID_ARG,
						 set_dev_id,
						 &dlb2_args->dev_id);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing dev_id parameter",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			if (version == DLB2_HW_V2) {
				ret = rte_kvargs_process(
					kvlist,
					DLB2_QID_DEPTH_THRESH_ARG,
					set_qid_depth_thresh,
					&dlb2_args->qid_depth_thresholds);
			} else {
				ret = rte_kvargs_process(
					kvlist,
					DLB2_QID_DEPTH_THRESH_ARG,
					set_qid_depth_thresh_v2_5,
					&dlb2_args->qid_depth_thresholds);
			}
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing qid_depth_thresh parameter",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, DLB2_POLL_INTERVAL_ARG,
						 set_poll_interval,
						 &dlb2_args->poll_interval);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing poll interval parameter",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist,
						 DLB2_SW_CREDIT_QUANTA_ARG,
						 set_sw_credit_quanta,
						 &dlb2_args->sw_credit_quanta);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing sw credit quanta parameter",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist,
						 DLB2_HW_CREDIT_QUANTA_ARG,
						 set_hw_credit_quanta,
						 &dlb2_args->hw_credit_quanta);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing hw credit quanta parameter",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, DLB2_DEPTH_THRESH_ARG,
					set_default_depth_thresh,
					&dlb2_args->default_depth_thresh);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing set depth thresh parameter",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist,
					DLB2_VECTOR_OPTS_ENAB_ARG,
					set_vector_opts_enab,
					&dlb2_args->vector_opts_enabled);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing vector opts enabled",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist,
					DLB2_MAX_CQ_DEPTH,
					set_max_cq_depth,
					&dlb2_args->max_cq_depth);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing max cq depth",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist,
						 DLB2_MAX_ENQ_DEPTH,
						 set_max_enq_depth,
						 &dlb2_args->max_enq_depth);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing vector opts enabled",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist,
					DLB2_PORT_COS,
					set_port_cos,
					&dlb2_args->port_cos);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing port cos",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist,
					DLB2_COS_BW,
					set_cos_bw,
					&dlb2_args->cos_bw);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing cos_bw",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}


			ret = rte_kvargs_process(kvlist,
						 DLB2_PRODUCER_COREMASK,
						 set_producer_coremask,
						 &dlb2_args->producer_coremask);
			if (ret != 0) {
				DLB2_LOG_ERR(
					"%s: Error parsing producer coremask",
					name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist,
						 DLB2_DEFAULT_LDB_PORT_ALLOCATION_ARG,
						 set_default_ldb_port_allocation,
						 &dlb2_args->default_ldb_port_allocation);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing ldb default port allocation arg",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist,
						 DLB2_ENABLE_CQ_WEIGHT_ARG,
						 set_enable_cq_weight,
						 &dlb2_args->enable_cq_weight);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing enable_cq_weight arg",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}
			if (version == DLB2_HW_V2 && dlb2_args->enable_cq_weight)
				DLB2_LOG_INFO("Ignoring 'enable_cq_weight=y'. Only supported for 2.5 HW onwards");

			ret = rte_kvargs_process(kvlist, DLB2_USE_DEFAULT_HL,
						 set_hl_override,
						 &dlb2_args->use_default_hl);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing hl_override arg",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, DLB2_ALLOC_HL_ENTRIES,
						 set_hl_entries,
						 &dlb2_args->alloc_hl_entries);
			if (ret != 0) {
				DLB2_LOG_ERR("%s: Error parsing hl_override arg",
					     name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			rte_kvargs_free(kvlist);
		}
	}
	return ret;
}
RTE_LOG_REGISTER_DEFAULT(eventdev_dlb2_log_level, NOTICE);
