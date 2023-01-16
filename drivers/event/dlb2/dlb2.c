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
#define DLB2_BYPASS_FENCE_ON_PP 0  /* 1 == Bypass fence, 0 == do not bypass */

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
	.event_dev_cap = (RTE_EVENT_DEV_CAP_EVENT_QOS |
			  RTE_EVENT_DEV_CAP_NONSEQ_MODE |
			  RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |
			  RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES |
			  RTE_EVENT_DEV_CAP_BURST_MODE |
			  RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE |
			  RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK |
			  RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT |
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
		    dlb2->cos_ports[port_cos[q]] < DLB2_MAX_NUM_LDB_PORTS_PER_COS) {
			dlb2->cos_ports[port_cos[q]]++;
			dlb2->max_cos_port = q;
		}
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
		DLB2_LOG_ERR("ioctl get dlb2 num resources, err=%d\n", ret);
		return ret;
	}

	/* Complete filling in device resource info returned to evdev app,
	 * overriding any default values.
	 * The capabilities (CAPs) were set at compile time.
	 */

	if (dlb2->max_cq_depth != DLB2_DEFAULT_CQ_DEPTH)
		num_ldb_ports = DLB2_MAX_HL_ENTRIES / dlb2->max_cq_depth;
	else
		num_ldb_ports = dlb2->hw_rsrc_query_results.num_ldb_ports;

	evdev_dlb2_default_info.max_event_queues =
		dlb2->hw_rsrc_query_results.num_ldb_queues;

	evdev_dlb2_default_info.max_event_ports = num_ldb_ports;

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
		DLB2_LOG_ERR("NULL pointer\n");
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
		DLB2_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(max_cq_depth, value);
	if (ret < 0)
		return ret;

	if (*max_cq_depth < DLB2_MIN_CQ_DEPTH_OVERRIDE ||
	    *max_cq_depth > DLB2_MAX_CQ_DEPTH_OVERRIDE ||
	    !rte_is_power_of_2(*max_cq_depth)) {
		DLB2_LOG_ERR("dlb2: max_cq_depth %d and %d and a power of 2\n",
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
		DLB2_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(max_enq_depth, value);
	if (ret < 0)
		return ret;

	if (*max_enq_depth < DLB2_MIN_ENQ_DEPTH_OVERRIDE ||
	    *max_enq_depth > DLB2_MAX_ENQ_DEPTH_OVERRIDE ||
	    !rte_is_power_of_2(*max_enq_depth)) {
		DLB2_LOG_ERR("dlb2: max_enq_depth %d and %d and a power of 2\n",
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
		DLB2_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(max_num_events, value);
	if (ret < 0)
		return ret;

	if (*max_num_events < 0 || *max_num_events >
			DLB2_MAX_NUM_LDB_CREDITS) {
		DLB2_LOG_ERR("dlb2: max_num_events must be between 0 and %d\n",
			     DLB2_MAX_NUM_LDB_CREDITS);
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
		DLB2_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(num_dir_credits, value);
	if (ret < 0)
		return ret;

	if (*num_dir_credits < 0 ||
	    *num_dir_credits > DLB2_MAX_NUM_DIR_CREDITS(DLB2_HW_V2)) {
		DLB2_LOG_ERR("dlb2: num_dir_credits must be between 0 and %d\n",
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
		DLB2_LOG_ERR("NULL pointer\n");
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
		DLB2_LOG_ERR("NULL pointer\n");
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
	struct dlb2_port_cos *port_cos = opaque;
	int first, last, cos_id, i;

	if (value == NULL || opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer\n");
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
		DLB2_LOG_ERR("Error parsing ldb port port_cos devarg. Should be port-port:val, or port:val\n");
		return -EINVAL;
	}

	if (first > last || first < 0 ||
		last >= DLB2_MAX_NUM_LDB_PORTS) {
		DLB2_LOG_ERR("Error parsing ldb port cos_id arg, invalid port value\n");
		return -EINVAL;
	}

	if (cos_id < DLB2_COS_0 || cos_id > DLB2_COS_3) {
		DLB2_LOG_ERR("Error parsing ldb port cos_id devarg, must be between 0 and 4\n");
		return -EINVAL;
	}

	for (i = first; i <= last; i++)
		port_cos->cos_id[i] = cos_id; /* indexed by port */

	return 0;
}

static int
set_cos_bw(const char *key __rte_unused,
	     const char *value,
	     void *opaque)
{
	struct dlb2_cos_bw *cos_bw = opaque;

	if (opaque == NULL) {
		DLB2_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	/* format must be %d,%d,%d,%d */

	if (sscanf(value, "%d:%d:%d:%d", &cos_bw->val[0], &cos_bw->val[1],
		   &cos_bw->val[2], &cos_bw->val[3]) != 4) {
		DLB2_LOG_ERR("Error parsing cos bandwidth devarg. Should be bw0:bw1:bw2:bw3 where all values combined are <= 100\n");
		return -EINVAL;
	}
	if (cos_bw->val[0] + cos_bw->val[1] + cos_bw->val[2] + cos_bw->val[3] > 100) {
		DLB2_LOG_ERR("Error parsing cos bandwidth devarg. Should be bw0:bw1:bw2:bw3  where all values combined are <= 100\n");
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
		DLB2_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	ret = dlb2_string_to_int(sw_credit_quanta, value);
	if (ret < 0)
		return ret;

	if (*sw_credit_quanta <= 0) {
		DLB2_LOG_ERR("sw_credit_quanta must be > 0\n");
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
		DLB2_LOG_ERR("NULL pointer\n");
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
		DLB2_LOG_ERR("NULL pointer\n");
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
		DLB2_LOG_ERR("NULL pointer\n");
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
		DLB2_LOG_ERR("NULL pointer\n");
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
		DLB2_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	if ((*value == 'y') || (*value == 'Y'))
		*enable_cq_weight = true;
	else
		*enable_cq_weight = false;

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
		DLB2_LOG_ERR("NULL pointer\n");
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
		DLB2_LOG_ERR("Error parsing qid depth devarg. Should be all:val, qid-qid:val, or qid:val\n");
		return -EINVAL;
	}

	if (first > last || first < 0 ||
		last >= DLB2_MAX_NUM_QUEUES(DLB2_HW_V2)) {
		DLB2_LOG_ERR("Error parsing qid depth devarg, invalid qid value\n");
		return -EINVAL;
	}

	if (thresh < 0 || thresh > DLB2_MAX_QUEUE_DEPTH_THRESHOLD) {
		DLB2_LOG_ERR("Error parsing qid depth devarg, threshold > %d\n",
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
		DLB2_LOG_ERR("NULL pointer\n");
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
		DLB2_LOG_ERR("Error parsing qid depth devarg. Should be all:val, qid-qid:val, or qid:val\n");
		return -EINVAL;
	}

	if (first > last || first < 0 ||
		last >= DLB2_MAX_NUM_QUEUES(DLB2_HW_V2_5)) {
		DLB2_LOG_ERR("Error parsing qid depth devarg, invalid qid value\n");
		return -EINVAL;
	}

	if (thresh < 0 || thresh > DLB2_MAX_QUEUE_DEPTH_THRESHOLD) {
		DLB2_LOG_ERR("Error parsing qid depth devarg, threshold > %d\n",
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

		DLB2_LOG_ERR("get resources err=%d, devid=%d\n",
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
	int ret = 0;
	uint32_t cos_ports = 0;
	struct dlb2_create_sched_domain_args *cfg;

	if (resources_asked == NULL) {
		DLB2_LOG_ERR("dlb2: dlb2_create NULL parameter\n");
		ret = EINVAL;
		goto error_exit;
	}

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

	if (cos_ports > resources_asked->num_ldb_ports ||
	    (cos_ports && dlb2->max_cos_port >= resources_asked->num_ldb_ports)) {
		DLB2_LOG_ERR("dlb2: num_ldb_ports < cos_ports\n");
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

	cfg->num_hist_list_entries = resources_asked->num_ldb_ports *
		evdev_dlb2_default_info.max_event_port_dequeue_depth;

	if (device_version == DLB2_HW_V2_5) {
		DLB2_LOG_DBG("sched domain create - ldb_qs=%d, ldb_ports=%d, dir_ports=%d, atomic_inflights=%d, hist_list_entries=%d, credits=%d\n",
			     cfg->num_ldb_queues,
			     resources_asked->num_ldb_ports,
			     cfg->num_dir_ports,
			     cfg->num_atomic_inflights,
			     cfg->num_hist_list_entries,
			     cfg->num_credits);
	} else {
		DLB2_LOG_DBG("sched domain create - ldb_qs=%d, ldb_ports=%d, dir_ports=%d, atomic_inflights=%d, hist_list_entries=%d, ldb_credits=%d, dir_credits=%d\n",
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
		DLB2_LOG_ERR("dlb2: domain create failed, ret = %d, extra status: %s\n",
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
			DLB2_LOG_ERR("get resources err=%d, devid=%d\n",
				     ret, data->dev_id);
			return ret;
		}
	}

	if (config->nb_event_queues > rsrcs->num_queues) {
		DLB2_LOG_ERR("nb_event_queues parameter (%d) exceeds the QM device's capabilities (%d).\n",
			     config->nb_event_queues,
			     rsrcs->num_queues);
		return -EINVAL;
	}
	if (config->nb_event_ports > (rsrcs->num_ldb_ports
			+ rsrcs->num_dir_ports)) {
		DLB2_LOG_ERR("nb_event_ports parameter (%d) exceeds the QM device's capabilities (%d).\n",
			     config->nb_event_ports,
			     (rsrcs->num_ldb_ports + rsrcs->num_dir_ports));
		return -EINVAL;
	}
	if (config->nb_events_limit > rsrcs->nb_events_limit) {
		DLB2_LOG_ERR("nb_events_limit parameter (%d) exceeds the QM device's capabilities (%d).\n",
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
		DLB2_LOG_ERR("dlb2_hw_create_sched_domain failed\n");
		return -ENODEV;
	}

	dlb2->new_event_limit = config->nb_events_limit;
	__atomic_store_n(&dlb2->inflights, 0, __ATOMIC_SEQ_CST);

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
	} else {
		dlb2->ldb_credit_pool = rsrcs->num_ldb_credits;
		dlb2->max_ldb_credits = rsrcs->num_ldb_credits;
		dlb2->dir_credit_pool = rsrcs->num_dir_credits;
		dlb2->max_dir_credits = rsrcs->num_dir_credits;
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
	port_conf->dequeue_depth = 32;
	port_conf->enqueue_depth = DLB2_MAX_ENQUEUE_DEPTH;
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
		DLB2_LOG_ERR("dlb2: get_sn_allocation ret=%d (driver status: %s)\n",
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
		DLB2_LOG_ERR("dlb2: set_sn_allocation ret=%d (driver status: %s)\n",
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
		DLB2_LOG_ERR("dlb2: get_sn_occupancy ret=%d (driver status: %s)\n",
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
		DLB2_LOG_ERR("[%s()] No groups with %d sequence_numbers are available or have free slots\n",
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
		DLB2_LOG_ERR("dlb2: create LB event queue error, ret=%d (driver status: %s)\n",
			     ret, dlb2_error_strings[cfg.response.status]);
		return -EINVAL;
	}

	qm_qid = cfg.response.id;

	/* Save off queue config for debug, resource lookups, and reconfig */
	queue->num_qid_inflights = cfg.num_qid_inflights;
	queue->num_atm_inflights = cfg.num_atomic_inflights;

	queue->sched_type = sched_type;
	queue->config_state = DLB2_CONFIGURED;

	DLB2_LOG_DBG("Created LB event queue %d, nb_inflights=%d, nb_seq=%d, qid inflights=%d\n",
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
		DLB2_LOG_ERR("Failed to create the load-balanced queue\n");

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
		DLB2_LOG_ERR("dlb2: no memory for consume_qe\n");
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
		DLB2_LOG_ERR("dlb2: no memory for complete_qe\n");
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
		DLB2_LOG_ERR("dlb2: no qe4 memory\n");
		ret = -ENOMEM;
		goto error_exit;
	}

	ret = dlb2_init_int_arm_qe(qm_port, mz_name);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: dlb2_init_int_arm_qe ret=%d\n", ret);
		goto error_exit;
	}

	ret = dlb2_init_consume_qe(qm_port, mz_name);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: dlb2_init_consume_qe ret=%d\n", ret);
		goto error_exit;
	}

	return 0;

error_exit:

	dlb2_free_qe_mem(qm_port);

	return ret;
}

static inline uint16_t
dlb2_event_enqueue_delayed(void *event_port,
			   const struct rte_event events[]);

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
	int ret;
	struct dlb2_port *qm_port = NULL;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t qm_port_id;
	uint16_t ldb_credit_high_watermark = 0;
	uint16_t dir_credit_high_watermark = 0;
	uint16_t credit_high_watermark = 0;

	if (handle == NULL)
		return -EINVAL;

	if (dequeue_depth < DLB2_MIN_CQ_DEPTH) {
		DLB2_LOG_ERR("dlb2: invalid cq depth, must be at least %d\n",
			     DLB2_MIN_CQ_DEPTH);
		return -EINVAL;
	}

	if (dlb2->version == DLB2_HW_V2 && ev_port->cq_weight != 0 &&
	    ev_port->cq_weight > dequeue_depth) {
		DLB2_LOG_ERR("dlb2: invalid cq dequeue depth %d, must be >= cq weight %d\n",
			     dequeue_depth, ev_port->cq_weight);
		return -EINVAL;
	}

	rte_spinlock_lock(&handle->resource_lock);

	/* We round up to the next power of 2 if necessary */
	cfg.cq_depth = rte_align32pow2(dequeue_depth);
	cfg.cq_depth_threshold = 1;

	cfg.cq_history_list_size = cfg.cq_depth;

	cfg.cos_id = ev_port->cos_id;
	cfg.cos_strict = 0;/* best effots */

	/* User controls the LDB high watermark via enqueue depth. The DIR high
	 * watermark is equal, unless the directed credit pool is too small.
	 */
	if (dlb2->version == DLB2_HW_V2) {
		ldb_credit_high_watermark = enqueue_depth;
		/* If there are no directed ports, the kernel driver will
		 * ignore this port's directed credit settings. Don't use
		 * enqueue_depth if it would require more directed credits
		 * than are available.
		 */
		dir_credit_high_watermark =
			RTE_MIN(enqueue_depth,
				handle->cfg.num_dir_credits / dlb2->num_ports);
	} else
		credit_high_watermark = enqueue_depth;

	/* Per QM values */

	ret = dlb2_iface_ldb_port_create(handle, &cfg,  dlb2->poll_mode);
	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: dlb2_ldb_port_create error, ret=%d (driver status: %s)\n",
			     ret, dlb2_error_strings[cfg.response.status]);
		goto error_exit;
	}

	qm_port_id = cfg.response.id;

	DLB2_LOG_DBG("dlb2: ev_port %d uses qm LB port %d <<<<<\n",
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
		DLB2_LOG_ERR("dlb2: init_qe_mem failed, ret=%d\n", ret);
		goto error_exit;
	}

	qm_port->id = qm_port_id;

	if (dlb2->version == DLB2_HW_V2_5 && (dlb2->enable_cq_weight == true)) {
		struct dlb2_enable_cq_weight_args cq_weight_args = { {0} };
		cq_weight_args.port_id = qm_port->id;
		cq_weight_args.limit = dequeue_depth;
		ret = dlb2_iface_enable_cq_weight(handle, &cq_weight_args);

		if (ret < 0) {
			DLB2_LOG_ERR("dlb2: dlb2_dir_port_create error, ret=%d (driver status: %s)\n",
					ret,
					dlb2_error_strings[cfg.response.  status]);
			goto error_exit;
		}
		qm_port->cq_weight = dequeue_depth;
	} else {
		qm_port->cq_weight = 0;
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
	if (qm_port->token_pop_mode == DELAYED_POP) {
		dlb2->event_dev->enqueue = dlb2_event_enqueue_delayed;
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
		qm_port->dir_credits = dir_credit_high_watermark;
		qm_port->ldb_credits = ldb_credit_high_watermark;
		qm_port->credit_pool[DLB2_DIR_QUEUE] = &dlb2->dir_credit_pool;
		qm_port->credit_pool[DLB2_LDB_QUEUE] = &dlb2->ldb_credit_pool;

		DLB2_LOG_DBG("dlb2: created ldb port %d, depth = %d, ldb credits=%d, dir credits=%d\n",
			     qm_port_id,
			     dequeue_depth,
			     qm_port->ldb_credits,
			     qm_port->dir_credits);
	} else {
		qm_port->credits = credit_high_watermark;
		qm_port->credit_pool[DLB2_COMBINED_POOL] = &dlb2->credit_pool;

		DLB2_LOG_DBG("dlb2: created ldb port %d, depth = %d, credits=%d\n",
			     qm_port_id,
			     dequeue_depth,
			     qm_port->credits);
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

	if (qm_port)
		dlb2_free_qe_mem(qm_port);

	rte_spinlock_unlock(&handle->resource_lock);

	DLB2_LOG_ERR("dlb2: create ldb port failed!\n");

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
		DLB2_LOG_ERR("dlb2: invalid dequeue_depth, must be %d-%d\n",
			     DLB2_MIN_CQ_DEPTH, DLB2_MAX_INPUT_QUEUE_DEPTH);
		return -EINVAL;
	}

	if (enqueue_depth < DLB2_MIN_ENQUEUE_DEPTH) {
		DLB2_LOG_ERR("dlb2: invalid enqueue_depth, must be at least %d\n",
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
		DLB2_LOG_ERR("dlb2: dlb2_dir_port_create error, ret=%d (driver status: %s)\n",
			     ret, dlb2_error_strings[cfg.response.status]);
		goto error_exit;
	}

	qm_port_id = cfg.response.id;

	DLB2_LOG_DBG("dlb2: ev_port %d uses qm DIR port %d <<<<<\n",
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
		DLB2_LOG_ERR("dlb2: init_qe_mem failed, ret=%d\n", ret);
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
		qm_port->cq_depth_mask = (cfg.cq_depth * 4) - 1;
	else
		qm_port->cq_depth_mask = cfg.cq_depth - 1;

	qm_port->gen_bit_shift = rte_popcount32(qm_port->cq_depth_mask);
	/* starting value of gen bit - it toggles at wrap time */
	qm_port->gen_bit = 1;
	dlb2_hw_cq_bitmask_init(qm_port, qm_port->cq_depth);

	qm_port->int_armed = false;

	/* Save off for later use in info and lookup APIs. */
	qm_port->qid_mappings = &dlb2->qm_dir_to_ev_queue_id[0];

	qm_port->dequeue_depth = dequeue_depth;

	/* Directed ports are auto-pop, by default. */
	qm_port->token_pop_mode = AUTO_POP;
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

		DLB2_LOG_DBG("dlb2: created dir port %d, depth = %d cr=%d,%d\n",
			     qm_port_id,
			     dequeue_depth,
			     dir_credit_high_watermark,
			     ldb_credit_high_watermark);
	} else {
		qm_port->credits = credit_high_watermark;
		qm_port->credit_pool[DLB2_COMBINED_POOL] = &dlb2->credit_pool;

		DLB2_LOG_DBG("dlb2: created dir port %d, depth = %d cr=%d\n",
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

	DLB2_LOG_ERR("dlb2: create dir port failed!\n");

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
		DLB2_LOG_ERR("Null parameter\n");
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

	ev_port = &dlb2->ev_ports[ev_port_id];
	/* configured? */
	if (ev_port->setup_done) {
		DLB2_LOG_ERR("evport %d is already configured\n", ev_port_id);
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

	/*
	 * Validate credit config before creating port
	 */

	if (port_conf->enqueue_depth > sw_credit_quanta ||
	    port_conf->enqueue_depth > hw_credit_quanta) {
		DLB2_LOG_ERR("Invalid port config. Enqueue depth %d must be <= credit quanta %d and batch size %d\n",
			     port_conf->enqueue_depth,
			     sw_credit_quanta,
			     hw_credit_quanta);
		return -EINVAL;
	}
	ev_port->enq_retries = port_conf->enqueue_depth / sw_credit_quanta;

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
			DLB2_LOG_ERR("Failed to create the lB port ve portId=%d\n",
				     ev_port_id);

			return ret;
		}
	} else {
		ret = dlb2_hw_create_dir_port(dlb2,
					      ev_port,
					      port_conf->dequeue_depth,
					      port_conf->enqueue_depth);
		if (ret < 0) {
			DLB2_LOG_ERR("Failed to create the DIR port\n");
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
		DLB2_LOG_ERR("dlb2: map qid error, ret=%d (driver status: %s)\n",
			     ret, dlb2_error_strings[cfg.response.status]);
		DLB2_LOG_ERR("dlb2: grp=%d, qm_port=%d, qm_qid=%d prio=%d\n",
			     handle->domain_id, cfg.port_id,
			     cfg.qid,
			     cfg.priority);
	} else {
		DLB2_LOG_DBG("dlb2: mapped queue %d to qm_port %d\n",
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
		DLB2_LOG_ERR("dlb2: qm_port %d has no available QID slots.\n",
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
		DLB2_LOG_ERR("dlb2: create DIR event queue error, ret=%d (driver status: %s)\n",
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
		DLB2_LOG_ERR("Failed to create the DIR queue\n");
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
		DLB2_LOG_ERR("port link failure for %s ev_q %d, ev_port %d\n",
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
		DLB2_LOG_ERR("%s queue %u can't link to %s port %u\n",
			     queue_is_dir ? "DIR" : "LDB", ev_queue->id,
			     port_is_dir ? "DIR" : "LDB", ev_port->id);

		rte_errno = -EINVAL;
		return -1;
	}

	/* Check if there is space for the requested link */
	if (!link_exists && index == -1) {
		DLB2_LOG_ERR("no space for new link\n");
		rte_errno = -ENOSPC;
		return -1;
	}

	/* Check if the directed port is already linked */
	if (ev_port->qm_port.is_directed && ev_port->num_links > 0 &&
	    !link_exists) {
		DLB2_LOG_ERR("Can't link DIR port %d to >1 queues\n",
			     ev_port->id);
		rte_errno = -EINVAL;
		return -1;
	}

	/* Check if the directed queue is already linked */
	if (ev_queue->qm_queue.is_directed && ev_queue->num_links > 0 &&
	    !link_exists) {
		DLB2_LOG_ERR("Can't link DIR queue %d to >1 ports\n",
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
		DLB2_LOG_ERR("dlb2: evport not setup\n");
		rte_errno = -EINVAL;
		return 0;
	}

	if (!ev_port->setup_done &&
	    ev_port->qm_port.config_state != DLB2_PREV_CONFIGURED) {
		DLB2_LOG_ERR("dlb2: evport not setup\n");
		rte_errno = -EINVAL;
		return 0;
	}

	/* Note: rte_event_port_link() ensures the PMD won't receive a NULL
	 * queues pointer.
	 */
	if (nb_links == 0) {
		DLB2_LOG_DBG("dlb2: nb_links is 0\n");
		return 0; /* Ignore and return success */
	}

	dlb2 = ev_port->dlb2;

	DLB2_LOG_DBG("Linking %u queues to %s port %d\n",
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
		DLB2_LOG_ERR("dlb2: unmap qid error, ret=%d (driver status: %s)\n",
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
		DLB2_LOG_DBG("dlb2: ignoring LB QID %d not mapped for qm_port %d.\n",
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

static int
dlb2_eventdev_port_unlink(struct rte_eventdev *dev, void *event_port,
			  uint8_t queues[], uint16_t nb_unlinks)
{
	struct dlb2_eventdev_port *ev_port = event_port;
	struct dlb2_eventdev *dlb2;
	int i;

	RTE_SET_USED(dev);

	if (!ev_port->setup_done) {
		DLB2_LOG_ERR("dlb2: evport %d is not configured\n",
			     ev_port->id);
		rte_errno = -EINVAL;
		return 0;
	}

	if (queues == NULL || nb_unlinks == 0) {
		DLB2_LOG_DBG("dlb2: queues is NULL or nb_unlinks is 0\n");
		return 0; /* Ignore and return success */
	}

	if (ev_port->qm_port.is_directed) {
		DLB2_LOG_DBG("dlb2: ignore unlink from dir port %d\n",
			     ev_port->id);
		rte_errno = 0;
		return nb_unlinks; /* as if success */
	}

	dlb2 = ev_port->dlb2;

	for (i = 0; i < nb_unlinks; i++) {
		struct dlb2_eventdev_queue *ev_queue;
		int ret, j;

		if (queues[i] >= dlb2->num_queues) {
			DLB2_LOG_ERR("dlb2: invalid queue id %d\n", queues[i]);
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
			DLB2_LOG_ERR("unlink err=%d for port %d queue %d\n",
				     ret, ev_port->id, queues[i]);
			rte_errno = -ENOENT;
			return i; /* return index of offending queue */
		}

		ev_port->link[j].valid = false;
		ev_port->num_links--;
		ev_queue->num_links--;
	}

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
		DLB2_LOG_ERR("dlb2: evport %d is not configured\n",
			     ev_port->id);
		rte_errno = -EINVAL;
		return 0;
	}

	cfg.port_id = ev_port->qm_port.id;
	dlb2 = ev_port->dlb2;
	handle = &dlb2->qm_instance;
	ret = dlb2_iface_pending_port_unmaps(handle, &cfg);

	if (ret < 0) {
		DLB2_LOG_ERR("dlb2: num_unlinks_in_progress ret=%d (driver status: %s)\n",
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
		DLB2_LOG_ERR("bad state %d for dev_start\n",
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
		DLB2_LOG_ERR("dlb2: sched_domain_start ret=%d (driver status: %s)\n",
			     ret, dlb2_error_strings[cfg.response.status]);
		return ret;
	}

	dlb2->run_state = DLB2_RUN_STATE_STARTED;
	DLB2_LOG_DBG("dlb2: sched_domain_start completed OK\n");

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
		   __atomic_compare_exchange_n(
			qm_port->credit_pool[type],
			&credits, credits - batch_size, false,
			__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)))
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

		__atomic_fetch_sub(&dlb2->inflights, val, __ATOMIC_SEQ_CST);
		ev_port->inflight_credits -= val;
	}
}

static inline int
dlb2_check_enqueue_sw_credits(struct dlb2_eventdev *dlb2,
			      struct dlb2_eventdev_port *ev_port)
{
	uint32_t sw_inflights = __atomic_load_n(&dlb2->inflights,
						__ATOMIC_SEQ_CST);
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

		__atomic_fetch_add(&dlb2->inflights, credit_update_quanta,
				   __ATOMIC_SEQ_CST);
		ev_port->inflight_credits += (credit_update_quanta);

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
			DLB2_LOG_DBG("ldb credits exhausted\n");
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
			DLB2_LOG_DBG("dir credits exhausted\n");
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
			DLB2_LOG_DBG("credits exhausted\n");
			return 1; /* credits exhausted */
		}
	}

	return 0;
}

static __rte_always_inline void
dlb2_pp_write(struct dlb2_enqueue_qe *qe4,
	      struct process_local_port_data *port_data)
{
	dlb2_movdir64b(port_data->pp_addr, qe4);
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

	dlb2_movntdq_single(port_data->pp_addr, qe);

	DLB2_LOG_DBG("dlb2: consume immediate - %d QEs\n", num);

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

	dlb2_pp_write(qm_port->qe4, port_data);
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
	uint16_t *cached_credits = NULL;
	struct dlb2_queue *qm_queue;

	ev_queue = &dlb2->ev_queues[ev->queue_id];
	qm_queue = &ev_queue->qm_queue;
	*queue_id = qm_queue->id;

	/* Ignore sched_type and hardware credits on release events */
	if (ev->op == RTE_EVENT_OP_RELEASE)
		goto op_check;

	if (!qm_queue->is_directed) {
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
		switch (ev->sched_type) {
		case RTE_SCHED_TYPE_ORDERED:
			DLB2_LOG_DBG("dlb2: put_qe: RTE_SCHED_TYPE_ORDERED\n");
			if (qm_queue->sched_type != RTE_SCHED_TYPE_ORDERED) {
				DLB2_LOG_ERR("dlb2: tried to send ordered event to unordered queue %d\n",
					     *queue_id);
				rte_errno = -EINVAL;
				return 1;
			}
			*sched_type = DLB2_SCHED_ORDERED;
			break;
		case RTE_SCHED_TYPE_ATOMIC:
			DLB2_LOG_DBG("dlb2: put_qe: RTE_SCHED_TYPE_ATOMIC\n");
			*sched_type = DLB2_SCHED_ATOMIC;
			break;
		case RTE_SCHED_TYPE_PARALLEL:
			DLB2_LOG_DBG("dlb2: put_qe: RTE_SCHED_TYPE_PARALLEL\n");
			if (qm_queue->sched_type == RTE_SCHED_TYPE_ORDERED)
				*sched_type = DLB2_SCHED_ORDERED;
			else
				*sched_type = DLB2_SCHED_UNORDERED;
			break;
		default:
			DLB2_LOG_ERR("Unsupported LDB sched type in put_qe\n");
			DLB2_INC_STAT(ev_port->stats.tx_invalid, 1);
			rte_errno = -EINVAL;
			return 1;
		}
	} else {
		/* Directed destination queue */

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
		DLB2_LOG_DBG("dlb2: put_qe: RTE_SCHED_TYPE_DIRECTED\n");

		*sched_type = DLB2_SCHED_DIRECTED;
	}

op_check:
	switch (ev->op) {
	case RTE_EVENT_OP_NEW:
		/* Check that a sw credit is available */
		if (dlb2_check_enqueue_sw_credits(dlb2, ev_port)) {
			rte_errno = -ENOSPC;
			return 1;
		}
		ev_port->inflight_credits--;
		(*cached_credits)--;
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
		(*cached_credits)--;
		break;
	case RTE_EVENT_OP_RELEASE:
		ev_port->inflight_credits++;
		/* Check for outstanding_releases underflow. If this occurs,
		 * the application is not using the EVENT_OPs correctly; for
		 * example, forwarding or releasing events that were not
		 * dequeued.
		 */
		RTE_ASSERT(ev_port->outstanding_releases > 0);
		ev_port->outstanding_releases--;
		qm_port->issued_releases++;

		/* Replenish s/w credits if enough are cached */
		dlb2_replenish_sw_credits(dlb2, ev_port);
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

	i = 0;

	port_data = &dlb2_port[qm_port->id][PORT_TYPE(qm_port)];
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
			    qm_port->token_pop_mode == DELAYED_POP &&
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

static inline uint16_t
dlb2_event_enqueue(void *event_port,
		   const struct rte_event events[])
{
	return __dlb2_event_enqueue_burst(event_port, events, 1, false);
}

static inline uint16_t
dlb2_event_enqueue_delayed(void *event_port,
			   const struct rte_event events[])
{
	return __dlb2_event_enqueue_burst(event_port, events, 1, true);
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
		DLB2_LOG_ERR("Invalid port id %d in dlb2-event_release\n",
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

			if (qm_port->token_pop_mode == DELAYED_POP &&
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
		DLB2_LOG_ERR("%s: Outstanding releases underflowed.\n",
			     __func__);
		return;
	}
	ev_port->outstanding_releases -= i;
	ev_port->inflight_credits += i;

	/* Replenish s/w credits if enough releases are performed */
	dlb2_replenish_sw_credits(dlb2, ev_port);
}

static inline void
dlb2_port_credits_inc(struct dlb2_port *qm_port, int num)
{
	uint32_t batch_size = qm_port->hw_credit_quanta;

	/* increment port credits, and return to pool if exceeds threshold */
	if (!qm_port->is_directed) {
		if (qm_port->dlb2->version == DLB2_HW_V2) {
			qm_port->cached_ldb_credits += num;
			if (qm_port->cached_ldb_credits >= 2 * batch_size) {
				__atomic_fetch_add(
					qm_port->credit_pool[DLB2_LDB_QUEUE],
					batch_size, __ATOMIC_SEQ_CST);
				qm_port->cached_ldb_credits -= batch_size;
			}
		} else {
			qm_port->cached_credits += num;
			if (qm_port->cached_credits >= 2 * batch_size) {
				__atomic_fetch_add(
				      qm_port->credit_pool[DLB2_COMBINED_POOL],
				      batch_size, __ATOMIC_SEQ_CST);
				qm_port->cached_credits -= batch_size;
			}
		}
	} else {
		if (qm_port->dlb2->version == DLB2_HW_V2) {
			qm_port->cached_dir_credits += num;
			if (qm_port->cached_dir_credits >= 2 * batch_size) {
				__atomic_fetch_add(
					qm_port->credit_pool[DLB2_DIR_QUEUE],
					batch_size, __ATOMIC_SEQ_CST);
				qm_port->cached_dir_credits -= batch_size;
			}
		} else {
			qm_port->cached_credits += num;
			if (qm_port->cached_credits >= 2 * batch_size) {
				__atomic_fetch_add(
				      qm_port->credit_pool[DLB2_COMBINED_POOL],
				      batch_size, __ATOMIC_SEQ_CST);
				qm_port->cached_credits -= batch_size;
			}
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
			DLB2_LOG_ERR("QE error bit ON\n");
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
		events[num].impl_opaque = qe->qid_depth;

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
	__m128i sse_evt[2];

	/* In the unlikely case that any of the QE error bits are set, process
	 * them one at a time.
	 */
	if (unlikely(qes[0].error || qes[1].error ||
		     qes[2].error || qes[3].error))
		return dlb2_process_dequeue_qes(ev_port, qm_port, events,
						 qes, num_events);

	events[0].u64 = qes[0].data;
	events[1].u64 = qes[1].data;
	events[2].u64 = qes[2].data;
	events[3].u64 = qes[3].data;

	/* Construct the metadata portion of two struct rte_events
	 * in one 128b SSE register. Event metadata is constructed in the SSE
	 * registers like so:
	 * sse_evt[0][63:0]:   event[0]'s metadata
	 * sse_evt[0][127:64]: event[1]'s metadata
	 * sse_evt[1][63:0]:   event[2]'s metadata
	 * sse_evt[1][127:64]: event[3]'s metadata
	 */
	sse_evt[0] = _mm_setzero_si128();
	sse_evt[1] = _mm_setzero_si128();

	/* Convert the hardware queue ID to an event queue ID and store it in
	 * the metadata:
	 * sse_evt[0][47:40]   = qid_mappings[qes[0].qid]
	 * sse_evt[0][111:104] = qid_mappings[qes[1].qid]
	 * sse_evt[1][47:40]   = qid_mappings[qes[2].qid]
	 * sse_evt[1][111:104] = qid_mappings[qes[3].qid]
	 */
#define DLB_EVENT_QUEUE_ID_BYTE 5
	sse_evt[0] = _mm_insert_epi8(sse_evt[0],
				     qid_mappings[qes[0].qid],
				     DLB_EVENT_QUEUE_ID_BYTE);
	sse_evt[0] = _mm_insert_epi8(sse_evt[0],
				     qid_mappings[qes[1].qid],
				     DLB_EVENT_QUEUE_ID_BYTE + 8);
	sse_evt[1] = _mm_insert_epi8(sse_evt[1],
				     qid_mappings[qes[2].qid],
				     DLB_EVENT_QUEUE_ID_BYTE);
	sse_evt[1] = _mm_insert_epi8(sse_evt[1],
				     qid_mappings[qes[3].qid],
				     DLB_EVENT_QUEUE_ID_BYTE + 8);

	/* Convert the hardware priority to an event priority and store it in
	 * the metadata, while also returning the queue depth status
	 * value captured by the hardware, storing it in impl_opaque, which can
	 * be read by the application but not modified
	 * sse_evt[0][55:48]   = DLB2_TO_EV_PRIO(qes[0].priority)
	 * sse_evt[0][63:56]   = qes[0].qid_depth
	 * sse_evt[0][119:112] = DLB2_TO_EV_PRIO(qes[1].priority)
	 * sse_evt[0][127:120] = qes[1].qid_depth
	 * sse_evt[1][55:48]   = DLB2_TO_EV_PRIO(qes[2].priority)
	 * sse_evt[1][63:56]   = qes[2].qid_depth
	 * sse_evt[1][119:112] = DLB2_TO_EV_PRIO(qes[3].priority)
	 * sse_evt[1][127:120] = qes[3].qid_depth
	 */
#define DLB_EVENT_PRIO_IMPL_OPAQUE_WORD 3
#define DLB_BYTE_SHIFT 8
	sse_evt[0] =
		_mm_insert_epi16(sse_evt[0],
			DLB2_TO_EV_PRIO((uint8_t)qes[0].priority) |
			(qes[0].qid_depth << DLB_BYTE_SHIFT),
			DLB_EVENT_PRIO_IMPL_OPAQUE_WORD);
	sse_evt[0] =
		_mm_insert_epi16(sse_evt[0],
			DLB2_TO_EV_PRIO((uint8_t)qes[1].priority) |
			(qes[1].qid_depth << DLB_BYTE_SHIFT),
			DLB_EVENT_PRIO_IMPL_OPAQUE_WORD + 4);
	sse_evt[1] =
		_mm_insert_epi16(sse_evt[1],
			DLB2_TO_EV_PRIO((uint8_t)qes[2].priority) |
			(qes[2].qid_depth << DLB_BYTE_SHIFT),
			DLB_EVENT_PRIO_IMPL_OPAQUE_WORD);
	sse_evt[1] =
		_mm_insert_epi16(sse_evt[1],
			DLB2_TO_EV_PRIO((uint8_t)qes[3].priority) |
			(qes[3].qid_depth << DLB_BYTE_SHIFT),
			DLB_EVENT_PRIO_IMPL_OPAQUE_WORD + 4);

	/* Write the event type, sub event type, and flow_id to the event
	 * metadata.
	 * sse_evt[0][31:0]   = qes[0].flow_id |
	 *			qes[0].u.event_type.major << 28 |
	 *			qes[0].u.event_type.sub << 20;
	 * sse_evt[0][95:64]  = qes[1].flow_id |
	 *			qes[1].u.event_type.major << 28 |
	 *			qes[1].u.event_type.sub << 20;
	 * sse_evt[1][31:0]   = qes[2].flow_id |
	 *			qes[2].u.event_type.major << 28 |
	 *			qes[2].u.event_type.sub << 20;
	 * sse_evt[1][95:64]  = qes[3].flow_id |
	 *			qes[3].u.event_type.major << 28 |
	 *			qes[3].u.event_type.sub << 20;
	 */
#define DLB_EVENT_EV_TYPE_DW 0
#define DLB_EVENT_EV_TYPE_SHIFT 28
#define DLB_EVENT_SUB_EV_TYPE_SHIFT 20
	sse_evt[0] = _mm_insert_epi32(sse_evt[0],
			qes[0].flow_id |
			qes[0].u.event_type.major << DLB_EVENT_EV_TYPE_SHIFT |
			qes[0].u.event_type.sub <<  DLB_EVENT_SUB_EV_TYPE_SHIFT,
			DLB_EVENT_EV_TYPE_DW);
	sse_evt[0] = _mm_insert_epi32(sse_evt[0],
			qes[1].flow_id |
			qes[1].u.event_type.major << DLB_EVENT_EV_TYPE_SHIFT |
			qes[1].u.event_type.sub <<  DLB_EVENT_SUB_EV_TYPE_SHIFT,
			DLB_EVENT_EV_TYPE_DW + 2);
	sse_evt[1] = _mm_insert_epi32(sse_evt[1],
			qes[2].flow_id |
			qes[2].u.event_type.major << DLB_EVENT_EV_TYPE_SHIFT |
			qes[2].u.event_type.sub <<  DLB_EVENT_SUB_EV_TYPE_SHIFT,
			DLB_EVENT_EV_TYPE_DW);
	sse_evt[1] = _mm_insert_epi32(sse_evt[1],
			qes[3].flow_id |
			qes[3].u.event_type.major << DLB_EVENT_EV_TYPE_SHIFT  |
			qes[3].u.event_type.sub << DLB_EVENT_SUB_EV_TYPE_SHIFT,
			DLB_EVENT_EV_TYPE_DW + 2);

	/* Write the sched type to the event metadata. 'op' and 'rsvd' are not
	 * set:
	 * sse_evt[0][39:32]  = sched_type_map[qes[0].sched_type] << 6
	 * sse_evt[0][103:96] = sched_type_map[qes[1].sched_type] << 6
	 * sse_evt[1][39:32]  = sched_type_map[qes[2].sched_type] << 6
	 * sse_evt[1][103:96] = sched_type_map[qes[3].sched_type] << 6
	 */
#define DLB_EVENT_SCHED_TYPE_BYTE 4
#define DLB_EVENT_SCHED_TYPE_SHIFT 6
	sse_evt[0] = _mm_insert_epi8(sse_evt[0],
		sched_type_map[qes[0].sched_type] << DLB_EVENT_SCHED_TYPE_SHIFT,
		DLB_EVENT_SCHED_TYPE_BYTE);
	sse_evt[0] = _mm_insert_epi8(sse_evt[0],
		sched_type_map[qes[1].sched_type] << DLB_EVENT_SCHED_TYPE_SHIFT,
		DLB_EVENT_SCHED_TYPE_BYTE + 8);
	sse_evt[1] = _mm_insert_epi8(sse_evt[1],
		sched_type_map[qes[2].sched_type] << DLB_EVENT_SCHED_TYPE_SHIFT,
		DLB_EVENT_SCHED_TYPE_BYTE);
	sse_evt[1] = _mm_insert_epi8(sse_evt[1],
		sched_type_map[qes[3].sched_type] << DLB_EVENT_SCHED_TYPE_SHIFT,
		DLB_EVENT_SCHED_TYPE_BYTE + 8);

	/* Store the metadata to the event (use the double-precision
	 * _mm_storeh_pd because there is no integer function for storing the
	 * upper 64b):
	 * events[0].event = sse_evt[0][63:0]
	 * events[1].event = sse_evt[0][127:64]
	 * events[2].event = sse_evt[1][63:0]
	 * events[3].event = sse_evt[1][127:64]
	 */
	_mm_storel_epi64((__m128i *)&events[0].event, sse_evt[0]);
	_mm_storeh_pd((double *)&events[1].event, (__m128d) sse_evt[0]);
	_mm_storel_epi64((__m128i *)&events[2].event, sse_evt[1]);
	_mm_storeh_pd((double *)&events[3].event, (__m128d) sse_evt[1]);

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
		__m128i v_sub = _mm_srli_epi32(v_qe_meta, 8);
		v_sub = _mm_and_si128(v_sub, v_sub_event_mask);
		__m128i v_type = _mm_and_si128(v_qe_meta, v_event_mask);
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
		_mm_storeu_si128((__m128i *)&events[3], v_ev_3);
		DLB2_INC_STAT(qm_port->ev_port->stats.rx_sched_cnt[hw_sched3],
			      1);
		/* fallthrough */
	case 3:
		v_ev_2 = _mm_unpacklo_epi64(v_unpk_ev_23, v_qe_2);
		_mm_storeu_si128((__m128i *)&events[2], v_ev_2);
		DLB2_INC_STAT(qm_port->ev_port->stats.rx_sched_cnt[hw_sched2],
			      1);
		/* fallthrough */
	case 2:
		v_ev_1 = _mm_blend_epi16(v_unpk_ev_01, v_qe_1, 0x0F);
		v_ev_1 = _mm_alignr_epi8(v_ev_1, v_ev_1, 8);
		_mm_storeu_si128((__m128i *)&events[1], v_ev_1);
		DLB2_INC_STAT(qm_port->ev_port->stats.rx_sched_cnt[hw_sched1],
			      1);
		/* fallthrough */
	case 1:
		v_ev_0 = _mm_unpacklo_epi64(v_unpk_ev_01, v_qe_0);
		_mm_storeu_si128((__m128i *)&events[0], v_ev_0);
		DLB2_INC_STAT(qm_port->ev_port->stats.rx_sched_cnt[hw_sched0],
			      1);
	}
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
		if (qm_port->token_pop_mode == AUTO_POP)
			dlb2_consume_qe_immediate(qm_port, num);

		ev_port->outstanding_releases += num;

		dlb2_port_credits_inc(qm_port, num);
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
		if (qm_port->token_pop_mode == AUTO_POP)
			dlb2_consume_qe_immediate(qm_port, num);

		ev_port->outstanding_releases += num;

		dlb2_port_credits_inc(qm_port, num);
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
	uint16_t cnt;

	RTE_ASSERT(ev_port->setup_done);
	RTE_ASSERT(ev != NULL);

	if (ev_port->implicit_release && ev_port->outstanding_releases > 0) {
		uint16_t out_rels = ev_port->outstanding_releases;

		dlb2_event_release(dlb2, ev_port->id, out_rels);

		DLB2_INC_STAT(ev_port->stats.tx_implicit_rel, out_rels);
	}

	if (qm_port->token_pop_mode == DEFERRED_POP && qm_port->owed_tokens)
		dlb2_consume_qe_immediate(qm_port, qm_port->owed_tokens);

	cnt = dlb2_hw_dequeue(dlb2, ev_port, ev, num, wait);

	DLB2_INC_STAT(ev_port->stats.traffic.total_polls, 1);
	DLB2_INC_STAT(ev_port->stats.traffic.zero_polls, ((cnt == 0) ? 1 : 0));

	return cnt;
}

static uint16_t
dlb2_event_dequeue(void *event_port, struct rte_event *ev, uint64_t wait)
{
	return dlb2_event_dequeue_burst(event_port, ev, 1, wait);
}

static uint16_t
dlb2_event_dequeue_burst_sparse(void *event_port, struct rte_event *ev,
				uint16_t num, uint64_t wait)
{
	struct dlb2_eventdev_port *ev_port = event_port;
	struct dlb2_port *qm_port = &ev_port->qm_port;
	struct dlb2_eventdev *dlb2 = ev_port->dlb2;
	uint16_t cnt;

	RTE_ASSERT(ev_port->setup_done);
	RTE_ASSERT(ev != NULL);

	if (ev_port->implicit_release && ev_port->outstanding_releases > 0) {
		uint16_t out_rels = ev_port->outstanding_releases;

		dlb2_event_release(dlb2, ev_port->id, out_rels);

		DLB2_INC_STAT(ev_port->stats.tx_implicit_rel, out_rels);
	}

	if (qm_port->token_pop_mode == DEFERRED_POP && qm_port->owed_tokens)
		dlb2_consume_qe_immediate(qm_port, qm_port->owed_tokens);

	cnt = dlb2_hw_dequeue_sparse(dlb2, ev_port, ev, num, wait);

	DLB2_INC_STAT(ev_port->stats.traffic.total_polls, 1);
	DLB2_INC_STAT(ev_port->stats.traffic.zero_polls, ((cnt == 0) ? 1 : 0));
	return cnt;
}

static uint16_t
dlb2_event_dequeue_sparse(void *event_port, struct rte_event *ev,
			  uint64_t wait)
{
	return dlb2_event_dequeue_burst_sparse(event_port, ev, 1, wait);
}

static void
dlb2_flush_port(struct rte_eventdev *dev, int port_id)
{
	struct dlb2_eventdev *dlb2 = dlb2_pmd_priv(dev);
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

	for (i = dlb2->ev_ports[port_id].outstanding_releases; i > 0; i--)
		rte_event_enqueue_burst(dev_id, port_id, &ev, 1);
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
		DLB2_LOG_ERR("dlb2: get_ldb_queue_depth ret=%d (driver status: %s)\n",
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
		DLB2_LOG_ERR("dlb2: get_dir_queue_depth ret=%d (driver status: %s)\n",
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
		DLB2_LOG_ERR("internal error: no LDB ev_ports\n");
		return;
	}

	rte_errno = 0;
	rte_event_port_unlink(dev_id, ev_port->id, NULL, 0);

	if (rte_errno) {
		DLB2_LOG_ERR("internal error: failed to unlink ev_port %d\n",
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
			DLB2_LOG_ERR("internal error: failed to link ev_port %d to queue %d\n",
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
			DLB2_LOG_ERR("internal error: failed to unlink ev_port %d to queue %d\n",
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
		DLB2_LOG_DBG("Internal error: already stopped\n");
		rte_spinlock_unlock(&dlb2->qm_instance.resource_lock);
		return;
	} else if (dlb2->run_state != DLB2_RUN_STATE_STARTED) {
		DLB2_LOG_ERR("Internal error: bad state %d for dev_stop\n",
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
	dev->enqueue = dlb2_event_enqueue;
	dev->enqueue_burst = dlb2_event_enqueue_burst;
	dev->enqueue_new_burst = dlb2_event_enqueue_new_burst;
	dev->enqueue_forward_burst = dlb2_event_enqueue_forward_burst;

	dlb2 = dev->data->dev_private;
	if (dlb2->poll_mode == DLB2_CQ_POLL_MODE_SPARSE) {
		dev->dequeue = dlb2_event_dequeue_sparse;
		dev->dequeue_burst = dlb2_event_dequeue_burst_sparse;
	} else {
		dev->dequeue = dlb2_event_dequeue;
		dev->dequeue_burst = dlb2_event_dequeue_burst;
	}
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
		DLB2_LOG_ERR("could not open event hardware device, err=%d\n",
			     err);
		return err;
	}

	err = dlb2_iface_get_device_version(&dlb2->qm_instance,
					    &dlb2->revision);
	if (err < 0) {
		DLB2_LOG_ERR("dlb2: failed to get the device version, err=%d\n",
			     err);
		return err;
	}

	err = dlb2_hw_query_resources(dlb2);
	if (err) {
		DLB2_LOG_ERR("get resources err=%d for %s\n",
			     err, name);
		return err;
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
			DLB2_LOG_ERR("dlb2: failed to configure class of service, err=%d\n",
				     err);
			return err;
		}
	}

	err = dlb2_iface_get_cq_poll_mode(&dlb2->qm_instance, &dlb2->poll_mode);
	if (err < 0) {
		DLB2_LOG_ERR("dlb2: failed to get the poll mode, err=%d\n",
			     err);
		return err;
	}

	/* Complete xtstats runtime initialization */
	err = dlb2_xstats_init(dlb2);
	if (err) {
		DLB2_LOG_ERR("dlb2: failed to init xstats, err=%d\n", err);
		return err;
	}

	/* Initialize each port's token pop mode */
	for (i = 0; i < DLB2_MAX_NUM_PORTS(dlb2->version); i++)
		dlb2->ev_ports[i].qm_port.token_pop_mode = AUTO_POP;

	rte_spinlock_init(&dlb2->qm_instance.resource_lock);

	dlb2_iface_low_level_io_init();

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
		DLB2_LOG_ERR("could not open event hardware device, err=%d\n",
			     err);
		return err;
	}

	err = dlb2_hw_query_resources(dlb2);
	if (err) {
		DLB2_LOG_ERR("get resources err=%d for %s\n",
			     err, name);
		return err;
	}

	dlb2_iface_low_level_io_init();

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
					     NULL };

	if (params != NULL && params[0] != '\0') {
		struct rte_kvargs *kvlist = rte_kvargs_parse(params, args);

		if (kvlist == NULL) {
			RTE_LOG(INFO, PMD,
				"Ignoring unsupported parameters when creating device '%s'\n",
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

			ret = rte_kvargs_process(kvlist, DLB2_MAX_NUM_EVENTS,
						 set_max_num_events,
						 &dlb2_args->max_num_events);
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

			rte_kvargs_free(kvlist);
		}
	}
	return ret;
}
RTE_LOG_REGISTER_DEFAULT(eventdev_dlb2_log_level, NOTICE);
