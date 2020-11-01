/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#include <assert.h>
#include <errno.h>
#include <nmmintrin.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_io.h>
#include <rte_kvargs.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_prefetch.h>
#include <rte_ring.h>
#include <rte_string_fns.h>

#include <rte_eventdev.h>
#include <rte_eventdev_pmd.h>

#include "dlb_priv.h"
#include "dlb_iface.h"
#include "dlb_inline_fns.h"

/*
 * Resources exposed to eventdev.
 */
#if (RTE_EVENT_MAX_QUEUES_PER_DEV > UINT8_MAX)
#error "RTE_EVENT_MAX_QUEUES_PER_DEV cannot fit in member max_event_queues"
#endif
static struct rte_event_dev_info evdev_dlb_default_info = {
	.driver_name = "", /* probe will set */
	.min_dequeue_timeout_ns = DLB_MIN_DEQUEUE_TIMEOUT_NS,
	.max_dequeue_timeout_ns = DLB_MAX_DEQUEUE_TIMEOUT_NS,
#if (RTE_EVENT_MAX_QUEUES_PER_DEV < DLB_MAX_NUM_LDB_QUEUES)
	.max_event_queues = RTE_EVENT_MAX_QUEUES_PER_DEV,
#else
	.max_event_queues = DLB_MAX_NUM_LDB_QUEUES,
#endif
	.max_event_queue_flows = DLB_MAX_NUM_FLOWS,
	.max_event_queue_priority_levels = DLB_QID_PRIORITIES,
	.max_event_priority_levels = DLB_QID_PRIORITIES,
	.max_event_ports = DLB_MAX_NUM_LDB_PORTS,
	.max_event_port_dequeue_depth = DLB_MAX_CQ_DEPTH,
	.max_event_port_enqueue_depth = DLB_MAX_ENQUEUE_DEPTH,
	.max_event_port_links = DLB_MAX_NUM_QIDS_PER_LDB_CQ,
	.max_num_events = DLB_MAX_NUM_LDB_CREDITS,
	.max_single_link_event_port_queue_pairs = DLB_MAX_NUM_DIR_PORTS,
	.event_dev_cap = (RTE_EVENT_DEV_CAP_QUEUE_QOS |
			  RTE_EVENT_DEV_CAP_EVENT_QOS |
			  RTE_EVENT_DEV_CAP_BURST_MODE |
			  RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |
			  RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE |
			  RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES),
};

struct process_local_port_data
dlb_port[DLB_MAX_NUM_PORTS][NUM_DLB_PORT_TYPES];

uint32_t
dlb_get_queue_depth(struct dlb_eventdev *dlb,
		    struct dlb_eventdev_queue *queue)
{
	/* DUMMY FOR NOW So "xstats" patch compiles */
	RTE_SET_USED(dlb);
	RTE_SET_USED(queue);

	return 0;
}

static int
dlb_hw_query_resources(struct dlb_eventdev *dlb)
{
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_hw_resource_info *dlb_info = &handle->info;
	int ret;

	ret = dlb_iface_get_num_resources(handle,
					  &dlb->hw_rsrc_query_results);
	if (ret) {
		DLB_LOG_ERR("get dlb num resources, err=%d\n", ret);
		return ret;
	}

	/* Complete filling in device resource info returned to evdev app,
	 * overriding any default values.
	 * The capabilities (CAPs) were set at compile time.
	 */

	evdev_dlb_default_info.max_event_queues =
		dlb->hw_rsrc_query_results.num_ldb_queues;

	evdev_dlb_default_info.max_event_ports =
		dlb->hw_rsrc_query_results.num_ldb_ports;

	evdev_dlb_default_info.max_num_events =
		dlb->hw_rsrc_query_results.max_contiguous_ldb_credits;

	/* Save off values used when creating the scheduling domain. */

	handle->info.num_sched_domains =
		dlb->hw_rsrc_query_results.num_sched_domains;

	handle->info.hw_rsrc_max.nb_events_limit =
		dlb->hw_rsrc_query_results.max_contiguous_ldb_credits;

	handle->info.hw_rsrc_max.num_queues =
		dlb->hw_rsrc_query_results.num_ldb_queues +
		dlb->hw_rsrc_query_results.num_dir_ports;

	handle->info.hw_rsrc_max.num_ldb_queues =
		dlb->hw_rsrc_query_results.num_ldb_queues;

	handle->info.hw_rsrc_max.num_ldb_ports =
		dlb->hw_rsrc_query_results.num_ldb_ports;

	handle->info.hw_rsrc_max.num_dir_ports =
		dlb->hw_rsrc_query_results.num_dir_ports;

	handle->info.hw_rsrc_max.reorder_window_size =
		dlb->hw_rsrc_query_results.num_hist_list_entries;

	rte_memcpy(dlb_info, &handle->info.hw_rsrc_max, sizeof(*dlb_info));

	return 0;
}

/* Wrapper for string to int conversion. Substituted for atoi(...), which is
 * unsafe.
 */
#define DLB_BASE_10 10

static int
dlb_string_to_int(int *result, const char *str)
{
	long ret;
	char *endstr;

	if (str == NULL || result == NULL)
		return -EINVAL;

	errno = 0;
	ret = strtol(str, &endstr, DLB_BASE_10);
	if (errno)
		return -errno;

	/* long int and int may be different width for some architectures */
	if (ret < INT_MIN || ret > INT_MAX || endstr == str)
		return -EINVAL;

	*result = ret;
	return 0;
}

static int
set_numa_node(const char *key __rte_unused, const char *value, void *opaque)
{
	int *socket_id = opaque;
	int ret;

	ret = dlb_string_to_int(socket_id, value);
	if (ret < 0)
		return ret;

	if (*socket_id > RTE_MAX_NUMA_NODES)
		return -EINVAL;

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
		DLB_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	ret = dlb_string_to_int(max_num_events, value);
	if (ret < 0)
		return ret;

	if (*max_num_events < 0 || *max_num_events > DLB_MAX_NUM_LDB_CREDITS) {
		DLB_LOG_ERR("dlb: max_num_events must be between 0 and %d\n",
			    DLB_MAX_NUM_LDB_CREDITS);
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
		DLB_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	ret = dlb_string_to_int(num_dir_credits, value);
	if (ret < 0)
		return ret;

	if (*num_dir_credits < 0 ||
	    *num_dir_credits > DLB_MAX_NUM_DIR_CREDITS) {
		DLB_LOG_ERR("dlb: num_dir_credits must be between 0 and %d\n",
			    DLB_MAX_NUM_DIR_CREDITS);
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
		DLB_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	ret = dlb_string_to_int(dev_id, value);
	if (ret < 0)
		return ret;

	return 0;
}

static int
set_defer_sched(const char *key __rte_unused,
		const char *value,
		void *opaque)
{
	int *defer_sched = opaque;

	if (value == NULL || opaque == NULL) {
		DLB_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	if (strncmp(value, "on", 2) != 0) {
		DLB_LOG_ERR("Invalid defer_sched argument \"%s\" (expected \"on\")\n",
			    value);
		return -EINVAL;
	}

	*defer_sched = 1;

	return 0;
}

static int
set_num_atm_inflights(const char *key __rte_unused,
		      const char *value,
		      void *opaque)
{
	int *num_atm_inflights = opaque;
	int ret;

	if (value == NULL || opaque == NULL) {
		DLB_LOG_ERR("NULL pointer\n");
		return -EINVAL;
	}

	ret = dlb_string_to_int(num_atm_inflights, value);
	if (ret < 0)
		return ret;

	if (*num_atm_inflights < 0 ||
	    *num_atm_inflights > DLB_MAX_NUM_ATM_INFLIGHTS) {
		DLB_LOG_ERR("dlb: atm_inflights must be between 0 and %d\n",
			    DLB_MAX_NUM_ATM_INFLIGHTS);
		return -EINVAL;
	}

	return 0;
}

void
dlb_entry_points_init(struct rte_eventdev *dev)
{
	static struct rte_eventdev_ops dlb_eventdev_entry_ops = {
		.dump             = dlb_eventdev_dump,
		.xstats_get       = dlb_eventdev_xstats_get,
		.xstats_get_names = dlb_eventdev_xstats_get_names,
		.xstats_get_by_name = dlb_eventdev_xstats_get_by_name,
		.xstats_reset	    = dlb_eventdev_xstats_reset,
	};

	/* Expose PMD's eventdev interface */
	dev->dev_ops = &dlb_eventdev_entry_ops;
}

int
dlb_primary_eventdev_probe(struct rte_eventdev *dev,
			   const char *name,
			   struct dlb_devargs *dlb_args)
{
	struct dlb_eventdev *dlb;
	int err;

	dlb = dev->data->dev_private;

	dlb->event_dev = dev; /* backlink */

	evdev_dlb_default_info.driver_name = name;

	dlb->max_num_events_override = dlb_args->max_num_events;
	dlb->num_dir_credits_override = dlb_args->num_dir_credits_override;
	dlb->defer_sched = dlb_args->defer_sched;
	dlb->num_atm_inflights_per_queue = dlb_args->num_atm_inflights;

	/* Open the interface.
	 * For vdev mode, this means open the dlb kernel module.
	 */
	err = dlb_iface_open(&dlb->qm_instance, name);
	if (err < 0) {
		DLB_LOG_ERR("could not open event hardware device, err=%d\n",
			    err);
		return err;
	}

	err = dlb_iface_get_device_version(&dlb->qm_instance, &dlb->revision);
	if (err < 0) {
		DLB_LOG_ERR("dlb: failed to get the device version, err=%d\n",
			    err);
		return err;
	}

	err = dlb_hw_query_resources(dlb);
	if (err) {
		DLB_LOG_ERR("get resources err=%d for %s\n", err, name);
		return err;
	}

	err = dlb_iface_get_cq_poll_mode(&dlb->qm_instance, &dlb->poll_mode);
	if (err < 0) {
		DLB_LOG_ERR("dlb: failed to get the poll mode, err=%d\n", err);
		return err;
	}

	/* Complete xtstats runtime initialization */
	err = dlb_xstats_init(dlb);
	if (err) {
		DLB_LOG_ERR("dlb: failed to init xstats, err=%d\n", err);
		return err;
	}

	rte_spinlock_init(&dlb->qm_instance.resource_lock);

	dlb_iface_low_level_io_init(dlb);

	dlb_entry_points_init(dev);

	return 0;
}

int
dlb_secondary_eventdev_probe(struct rte_eventdev *dev,
			     const char *name)
{
	struct dlb_eventdev *dlb;
	int err;

	dlb = dev->data->dev_private;

	evdev_dlb_default_info.driver_name = name;

	err = dlb_iface_open(&dlb->qm_instance, name);
	if (err < 0) {
		DLB_LOG_ERR("could not open event hardware device, err=%d\n",
			    err);
		return err;
	}

	err = dlb_hw_query_resources(dlb);
	if (err) {
		DLB_LOG_ERR("get resources err=%d for %s\n", err, name);
		return err;
	}

	dlb_iface_low_level_io_init(dlb);

	dlb_entry_points_init(dev);

	return 0;
}

int
dlb_parse_params(const char *params,
		 const char *name,
		 struct dlb_devargs *dlb_args)
{
	int ret = 0;
	static const char * const args[] = { NUMA_NODE_ARG,
					     DLB_MAX_NUM_EVENTS,
					     DLB_NUM_DIR_CREDITS,
					     DEV_ID_ARG,
					     DLB_DEFER_SCHED_ARG,
					     DLB_NUM_ATM_INFLIGHTS_ARG,
					     NULL };

	if (params && params[0] != '\0') {
		struct rte_kvargs *kvlist = rte_kvargs_parse(params, args);

		if (kvlist == NULL) {
			DLB_LOG_INFO("Ignoring unsupported parameters when creating device '%s'\n",
				     name);
		} else {
			int ret = rte_kvargs_process(kvlist, NUMA_NODE_ARG,
						     set_numa_node,
						     &dlb_args->socket_id);
			if (ret != 0) {
				DLB_LOG_ERR("%s: Error parsing numa node parameter",
					    name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, DLB_MAX_NUM_EVENTS,
						 set_max_num_events,
						 &dlb_args->max_num_events);
			if (ret != 0) {
				DLB_LOG_ERR("%s: Error parsing max_num_events parameter",
					    name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist,
					DLB_NUM_DIR_CREDITS,
					set_num_dir_credits,
					&dlb_args->num_dir_credits_override);
			if (ret != 0) {
				DLB_LOG_ERR("%s: Error parsing num_dir_credits parameter",
					    name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, DEV_ID_ARG,
						 set_dev_id,
						 &dlb_args->dev_id);
			if (ret != 0) {
				DLB_LOG_ERR("%s: Error parsing dev_id parameter",
					    name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist, DLB_DEFER_SCHED_ARG,
						 set_defer_sched,
						 &dlb_args->defer_sched);
			if (ret != 0) {
				DLB_LOG_ERR("%s: Error parsing defer_sched parameter",
					    name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			ret = rte_kvargs_process(kvlist,
						 DLB_NUM_ATM_INFLIGHTS_ARG,
						 set_num_atm_inflights,
						 &dlb_args->num_atm_inflights);
			if (ret != 0) {
				DLB_LOG_ERR("%s: Error parsing atm_inflights parameter",
					    name);
				rte_kvargs_free(kvlist);
				return ret;
			}

			rte_kvargs_free(kvlist);
		}
	}
	return ret;
}
RTE_LOG_REGISTER(eventdev_dlb_log_level, pmd.event.dlb, NOTICE);
