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

static void
dlb_free_qe_mem(struct dlb_port *qm_port)
{
	if (qm_port == NULL)
		return;

	rte_free(qm_port->qe4);
	qm_port->qe4 = NULL;

	rte_free(qm_port->consume_qe);
	qm_port->consume_qe = NULL;
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

/* VDEV-only notes:
 * This function first unmaps all memory mappings and closes the
 * domain's file descriptor, which causes the driver to reset the
 * scheduling domain. Once that completes (when close() returns), we
 * can safely free the dynamically allocated memory used by the
 * scheduling domain.
 *
 * PF-only notes:
 * We will maintain a use count and use that to determine when
 * a reset is required.  In PF mode, we never mmap, or munmap
 * device memory,  and we own the entire physical PCI device.
 */

static void
dlb_hw_reset_sched_domain(const struct rte_eventdev *dev, bool reconfig)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);
	enum dlb_configuration_state config_state;
	int i, j;

	/* Close and reset the domain */
	dlb_iface_domain_close(dlb);

	/* Free all dynamically allocated port memory */
	for (i = 0; i < dlb->num_ports; i++)
		dlb_free_qe_mem(&dlb->ev_ports[i].qm_port);

	/* If reconfiguring, mark the device's queues and ports as "previously
	 * configured." If the user does not reconfigure them, the PMD will
	 * reapply their previous configuration when the device is started.
	 */
	config_state = (reconfig) ? DLB_PREV_CONFIGURED : DLB_NOT_CONFIGURED;

	for (i = 0; i < dlb->num_ports; i++) {
		dlb->ev_ports[i].qm_port.config_state = config_state;
		/* Reset setup_done so ports can be reconfigured */
		dlb->ev_ports[i].setup_done = false;
		for (j = 0; j < DLB_MAX_NUM_QIDS_PER_LDB_CQ; j++)
			dlb->ev_ports[i].link[j].mapped = false;
	}

	for (i = 0; i < dlb->num_queues; i++)
		dlb->ev_queues[i].qm_queue.config_state = config_state;

	for (i = 0; i < DLB_MAX_NUM_QUEUES; i++)
		dlb->ev_queues[i].setup_done = false;

	dlb->num_ports = 0;
	dlb->num_ldb_ports = 0;
	dlb->num_dir_ports = 0;
	dlb->num_queues = 0;
	dlb->num_ldb_queues = 0;
	dlb->num_dir_queues = 0;
	dlb->configured = false;
}

static int
dlb_ldb_credit_pool_create(struct dlb_hw_dev *handle)
{
	struct dlb_create_ldb_pool_args cfg;
	struct dlb_cmd_response response;
	int ret;

	if (handle == NULL)
		return -EINVAL;

	if (!handle->cfg.resources.num_ldb_credits) {
		handle->cfg.ldb_credit_pool_id = 0;
		handle->cfg.num_ldb_credits = 0;
		return 0;
	}

	cfg.response = (uintptr_t)&response;
	cfg.num_ldb_credits = handle->cfg.resources.num_ldb_credits;

	ret = dlb_iface_ldb_credit_pool_create(handle,
					       &cfg);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: ldb_credit_pool_create ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
	}

	handle->cfg.ldb_credit_pool_id = response.id;
	handle->cfg.num_ldb_credits = cfg.num_ldb_credits;

	return ret;
}

static int
dlb_dir_credit_pool_create(struct dlb_hw_dev *handle)
{
	struct dlb_create_dir_pool_args cfg;
	struct dlb_cmd_response response;
	int ret;

	if (handle == NULL)
		return -EINVAL;

	if (!handle->cfg.resources.num_dir_credits) {
		handle->cfg.dir_credit_pool_id = 0;
		handle->cfg.num_dir_credits = 0;
		return 0;
	}

	cfg.response = (uintptr_t)&response;
	cfg.num_dir_credits = handle->cfg.resources.num_dir_credits;

	ret = dlb_iface_dir_credit_pool_create(handle, &cfg);
	if (ret < 0)
		DLB_LOG_ERR("dlb: dir_credit_pool_create ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);

	handle->cfg.dir_credit_pool_id = response.id;
	handle->cfg.num_dir_credits = cfg.num_dir_credits;

	return ret;
}

static int
dlb_hw_create_sched_domain(struct dlb_hw_dev *handle,
			   struct dlb_eventdev *dlb,
			   const struct dlb_hw_rsrcs *resources_asked)
{
	int ret = 0;
	struct dlb_create_sched_domain_args *config_params;
	struct dlb_cmd_response response;

	if (resources_asked == NULL) {
		DLB_LOG_ERR("dlb: dlb_create NULL parameter\n");
		ret = EINVAL;
		goto error_exit;
	}

	/* Map generic qm resources to dlb resources */
	config_params = &handle->cfg.resources;

	config_params->response = (uintptr_t)&response;

	/* DIR ports and queues */

	config_params->num_dir_ports =
		resources_asked->num_dir_ports;

	config_params->num_dir_credits =
		resources_asked->num_dir_credits;

	/* LDB ports and queues */

	config_params->num_ldb_queues =
		resources_asked->num_ldb_queues;

	config_params->num_ldb_ports =
		resources_asked->num_ldb_ports;

	config_params->num_ldb_credits =
		resources_asked->num_ldb_credits;

	config_params->num_atomic_inflights =
		dlb->num_atm_inflights_per_queue *
		config_params->num_ldb_queues;

	config_params->num_hist_list_entries = config_params->num_ldb_ports *
		DLB_NUM_HIST_LIST_ENTRIES_PER_LDB_PORT;

	/* dlb limited to 1 credit pool per queue type */
	config_params->num_ldb_credit_pools = 1;
	config_params->num_dir_credit_pools = 1;

	DLB_LOG_DBG("sched domain create - ldb_qs=%d, ldb_ports=%d, dir_ports=%d, atomic_inflights=%d, hist_list_entries=%d, ldb_credits=%d, dir_credits=%d, ldb_cred_pools=%d, dir-credit_pools=%d\n",
		    config_params->num_ldb_queues,
		    config_params->num_ldb_ports,
		    config_params->num_dir_ports,
		    config_params->num_atomic_inflights,
		    config_params->num_hist_list_entries,
		    config_params->num_ldb_credits,
		    config_params->num_dir_credits,
		    config_params->num_ldb_credit_pools,
		    config_params->num_dir_credit_pools);

	/* Configure the QM */

	ret = dlb_iface_sched_domain_create(handle, config_params);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: domain create failed, device_id = %d, (driver ret = %d, extra status: %s)\n",
			    handle->device_id,
			    ret,
			    dlb_error_strings[response.status]);
		goto error_exit;
	}

	handle->domain_id = response.id;
	handle->domain_id_valid = 1;

	config_params->response = 0;

	ret = dlb_ldb_credit_pool_create(handle);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: create ldb credit pool failed\n");
		goto error_exit2;
	}

	ret = dlb_dir_credit_pool_create(handle);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: create dir credit pool failed\n");
		goto error_exit2;
	}

	handle->cfg.configured = true;

	return 0;

error_exit2:
	dlb_iface_domain_close(dlb);

error_exit:
	return ret;
}

/* End HW specific */
static void
dlb_eventdev_info_get(struct rte_eventdev *dev,
		      struct rte_event_dev_info *dev_info)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);
	int ret;

	ret = dlb_hw_query_resources(dlb);
	if (ret) {
		const struct rte_eventdev_data *data = dev->data;

		DLB_LOG_ERR("get resources err=%d, devid=%d\n",
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
	evdev_dlb_default_info.max_event_ports += dlb->num_ldb_ports;
	evdev_dlb_default_info.max_event_queues += dlb->num_ldb_queues;
	evdev_dlb_default_info.max_num_events += dlb->num_ldb_credits;

	/* In DLB A-stepping hardware, applications are limited to 128
	 * configured ports (load-balanced or directed). The reported number of
	 * available ports must reflect this.
	 */
	if (dlb->revision < DLB_REV_B0) {
		int used_ports;

		used_ports = DLB_MAX_NUM_LDB_PORTS + DLB_MAX_NUM_DIR_PORTS -
			dlb->hw_rsrc_query_results.num_ldb_ports -
			dlb->hw_rsrc_query_results.num_dir_ports;

		evdev_dlb_default_info.max_event_ports =
			RTE_MIN(evdev_dlb_default_info.max_event_ports,
				128 - used_ports);
	}

	evdev_dlb_default_info.max_event_queues =
		RTE_MIN(evdev_dlb_default_info.max_event_queues,
			RTE_EVENT_MAX_QUEUES_PER_DEV);

	evdev_dlb_default_info.max_num_events =
		RTE_MIN(evdev_dlb_default_info.max_num_events,
			dlb->max_num_events_override);

	*dev_info = evdev_dlb_default_info;
}

/* Note: 1 QM instance per QM device, QM instance/device == event device */
static int
dlb_eventdev_configure(const struct rte_eventdev *dev)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_hw_rsrcs *rsrcs = &handle->info.hw_rsrc_max;
	const struct rte_eventdev_data *data = dev->data;
	const struct rte_event_dev_config *config = &data->dev_conf;
	int ret;

	/* If this eventdev is already configured, we must release the current
	 * scheduling domain before attempting to configure a new one.
	 */
	if (dlb->configured) {
		dlb_hw_reset_sched_domain(dev, true);

		ret = dlb_hw_query_resources(dlb);
		if (ret) {
			DLB_LOG_ERR("get resources err=%d, devid=%d\n",
				    ret, data->dev_id);
			return ret;
		}
	}

	if (config->nb_event_queues > rsrcs->num_queues) {
		DLB_LOG_ERR("nb_event_queues parameter (%d) exceeds the QM device's capabilities (%d).\n",
			    config->nb_event_queues,
			    rsrcs->num_queues);
		return -EINVAL;
	}
	if (config->nb_event_ports > (rsrcs->num_ldb_ports
			+ rsrcs->num_dir_ports)) {
		DLB_LOG_ERR("nb_event_ports parameter (%d) exceeds the QM device's capabilities (%d).\n",
			    config->nb_event_ports,
			    (rsrcs->num_ldb_ports + rsrcs->num_dir_ports));
		return -EINVAL;
	}
	if (config->nb_events_limit > rsrcs->nb_events_limit) {
		DLB_LOG_ERR("nb_events_limit parameter (%d) exceeds the QM device's capabilities (%d).\n",
			    config->nb_events_limit,
			    rsrcs->nb_events_limit);
		return -EINVAL;
	}

	if (config->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT)
		dlb->global_dequeue_wait = false;
	else {
		uint32_t timeout32;

		dlb->global_dequeue_wait = true;

		timeout32 = config->dequeue_timeout_ns;

		dlb->global_dequeue_wait_ticks =
			timeout32 * (rte_get_timer_hz() / 1E9);
	}

	/* Does this platform support umonitor/umwait? */
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_WAITPKG)) {
		if (RTE_LIBRTE_PMD_DLB_UMWAIT_CTL_STATE != 0 &&
		    RTE_LIBRTE_PMD_DLB_UMWAIT_CTL_STATE != 1) {
			DLB_LOG_ERR("invalid value (%d) for RTE_LIBRTE_PMD_DLB_UMWAIT_CTL_STATE must be 0 or 1.\n",
				    RTE_LIBRTE_PMD_DLB_UMWAIT_CTL_STATE);
			return -EINVAL;
		}
		dlb->umwait_allowed = true;
	}

	rsrcs->num_dir_ports = config->nb_single_link_event_port_queues;
	rsrcs->num_ldb_ports = config->nb_event_ports - rsrcs->num_dir_ports;
	/* 1 dir queue per dir port */
	rsrcs->num_ldb_queues = config->nb_event_queues - rsrcs->num_dir_ports;

	/* Scale down nb_events_limit by 4 for directed credits, since there
	 * are 4x as many load-balanced credits.
	 */
	rsrcs->num_ldb_credits = 0;
	rsrcs->num_dir_credits = 0;

	if (rsrcs->num_ldb_queues)
		rsrcs->num_ldb_credits = config->nb_events_limit;
	if (rsrcs->num_dir_ports)
		rsrcs->num_dir_credits = config->nb_events_limit / 4;
	if (dlb->num_dir_credits_override != -1)
		rsrcs->num_dir_credits = dlb->num_dir_credits_override;

	if (dlb_hw_create_sched_domain(handle, dlb, rsrcs) < 0) {
		DLB_LOG_ERR("dlb_hw_create_sched_domain failed\n");
		return -ENODEV;
	}

	dlb->new_event_limit = config->nb_events_limit;
	__atomic_store_n(&dlb->inflights, 0, __ATOMIC_SEQ_CST);

	/* Save number of ports/queues for this event dev */
	dlb->num_ports = config->nb_event_ports;
	dlb->num_queues = config->nb_event_queues;
	dlb->num_dir_ports = rsrcs->num_dir_ports;
	dlb->num_ldb_ports = dlb->num_ports - dlb->num_dir_ports;
	dlb->num_ldb_queues = dlb->num_queues - dlb->num_dir_ports;
	dlb->num_dir_queues = dlb->num_dir_ports;
	dlb->num_ldb_credits = rsrcs->num_ldb_credits;
	dlb->num_dir_credits = rsrcs->num_dir_credits;

	dlb->configured = true;

	return 0;
}

static void
dlb_eventdev_port_default_conf_get(struct rte_eventdev *dev,
				   uint8_t port_id,
				   struct rte_event_port_conf *port_conf)
{
	RTE_SET_USED(port_id);
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);

	port_conf->new_event_threshold = dlb->new_event_limit;
	port_conf->dequeue_depth = 32;
	port_conf->enqueue_depth = DLB_MAX_ENQUEUE_DEPTH;
	port_conf->event_port_cfg = 0;
}

static void
dlb_eventdev_queue_default_conf_get(struct rte_eventdev *dev,
				    uint8_t queue_id,
				    struct rte_event_queue_conf *queue_conf)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);
	queue_conf->nb_atomic_flows = 1024;
	queue_conf->nb_atomic_order_sequences = 32;
	queue_conf->event_queue_cfg = 0;
	queue_conf->priority = 0;
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
		.dev_infos_get    = dlb_eventdev_info_get,
		.dev_configure    = dlb_eventdev_configure,
		.queue_def_conf   = dlb_eventdev_queue_default_conf_get,
		.port_def_conf    = dlb_eventdev_port_default_conf_get,
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
