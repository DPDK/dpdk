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
#include "dlb_inline_fns.h"

/*
 * Resources exposed to eventdev.
 */
#if (RTE_EVENT_MAX_QUEUES_PER_DEV > UINT8_MAX)
#error "RTE_EVENT_MAX_QUEUES_PER_DEV cannot fit in member max_event_queues"
#endif

struct process_local_port_data
dlb_port[DLB_MAX_NUM_PORTS][NUM_DLB_PORT_TYPES];

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
	};

	/* Expose PMD's eventdev interface */
	dev->dev_ops = &dlb_eventdev_entry_ops;
}

int
dlb_primary_eventdev_probe(struct rte_eventdev *dev,
			   const char *name,
			   struct dlb_devargs *dlb_args)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(name);
	RTE_SET_USED(dlb_args);

	return 0;
}

int
dlb_secondary_eventdev_probe(struct rte_eventdev *dev,
			     const char *name)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(name);

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
