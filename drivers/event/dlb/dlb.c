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

static int
dlb_init_consume_qe(struct dlb_port *qm_port, char *mz_name)
{
	struct dlb_cq_pop_qe *qe;

	qe = rte_zmalloc(mz_name,
			DLB_NUM_QES_PER_CACHE_LINE *
				sizeof(struct dlb_cq_pop_qe),
			RTE_CACHE_LINE_SIZE);

	if (qe == NULL)	{
		DLB_LOG_ERR("dlb: no memory for consume_qe\n");
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
dlb_init_qe_mem(struct dlb_port *qm_port, char *mz_name)
{
	int ret, sz;

	sz = DLB_NUM_QES_PER_CACHE_LINE * sizeof(struct dlb_enqueue_qe);

	qm_port->qe4 = rte_zmalloc(mz_name, sz, RTE_CACHE_LINE_SIZE);

	if (qm_port->qe4 == NULL) {
		DLB_LOG_ERR("dlb: no qe4 memory\n");
		ret = -ENOMEM;
		goto error_exit;
	}

	ret = dlb_init_consume_qe(qm_port, mz_name);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: dlb_init_consume_qe ret=%d\n", ret);
		goto error_exit;
	}

	return 0;

error_exit:

	dlb_free_qe_mem(qm_port);

	return ret;
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
dlb_hw_create_ldb_port(struct dlb_eventdev *dlb,
		       struct dlb_eventdev_port *ev_port,
		       uint32_t dequeue_depth,
		       uint32_t cq_depth,
		       uint32_t enqueue_depth,
		       uint16_t rsvd_tokens,
		       bool use_rsvd_token_scheme)
{
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_create_ldb_port_args cfg = {0};
	struct dlb_cmd_response response = {0};
	int ret;
	struct dlb_port *qm_port = NULL;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t qm_port_id;

	if (handle == NULL)
		return -EINVAL;

	if (cq_depth < DLB_MIN_LDB_CQ_DEPTH) {
		DLB_LOG_ERR("dlb: invalid cq_depth, must be %d-%d\n",
			DLB_MIN_LDB_CQ_DEPTH, DLB_MAX_INPUT_QUEUE_DEPTH);
		return -EINVAL;
	}

	if (enqueue_depth < DLB_MIN_ENQUEUE_DEPTH) {
		DLB_LOG_ERR("dlb: invalid enqueue_depth, must be at least %d\n",
			    DLB_MIN_ENQUEUE_DEPTH);
		return -EINVAL;
	}

	rte_spinlock_lock(&handle->resource_lock);

	cfg.response = (uintptr_t)&response;

	/* We round up to the next power of 2 if necessary */
	cfg.cq_depth = rte_align32pow2(cq_depth);
	cfg.cq_depth_threshold = rsvd_tokens;

	cfg.cq_history_list_size = DLB_NUM_HIST_LIST_ENTRIES_PER_LDB_PORT;

	/* User controls the LDB high watermark via enqueue depth. The DIR high
	 * watermark is equal, unless the directed credit pool is too small.
	 */
	cfg.ldb_credit_high_watermark = enqueue_depth;

	/* If there are no directed ports, the kernel driver will ignore this
	 * port's directed credit settings. Don't use enqueue_depth if it would
	 * require more directed credits than are available.
	 */
	cfg.dir_credit_high_watermark =
		RTE_MIN(enqueue_depth,
			handle->cfg.num_dir_credits / dlb->num_ports);

	cfg.ldb_credit_quantum = cfg.ldb_credit_high_watermark / 2;
	cfg.ldb_credit_low_watermark = RTE_MIN(16, cfg.ldb_credit_quantum);

	cfg.dir_credit_quantum = cfg.dir_credit_high_watermark / 2;
	cfg.dir_credit_low_watermark = RTE_MIN(16, cfg.dir_credit_quantum);

	/* Per QM values */

	cfg.ldb_credit_pool_id = handle->cfg.ldb_credit_pool_id;
	cfg.dir_credit_pool_id = handle->cfg.dir_credit_pool_id;

	ret = dlb_iface_ldb_port_create(handle, &cfg, dlb->poll_mode);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: dlb_ldb_port_create error, ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		goto error_exit;
	}

	qm_port_id = response.id;

	DLB_LOG_DBG("dlb: ev_port %d uses qm LB port %d <<<<<\n",
		    ev_port->id, qm_port_id);

	qm_port = &ev_port->qm_port;
	qm_port->ev_port = ev_port; /* back ptr */
	qm_port->dlb = dlb; /* back ptr */

	/*
	 * Allocate and init local qe struct(s).
	 * Note: MOVDIR64 requires the enqueue QE (qe4) to be aligned.
	 */

	snprintf(mz_name, sizeof(mz_name), "ldb_port%d",
		 ev_port->id);

	ret = dlb_init_qe_mem(qm_port, mz_name);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: init_qe_mem failed, ret=%d\n", ret);
		goto error_exit;
	}

	qm_port->pp_mmio_base = DLB_LDB_PP_BASE + PAGE_SIZE * qm_port_id;
	qm_port->id = qm_port_id;

	/* The credit window is one high water mark of QEs */
	qm_port->ldb_pushcount_at_credit_expiry = 0;
	qm_port->cached_ldb_credits = cfg.ldb_credit_high_watermark;
	/* The credit window is one high water mark of QEs */
	qm_port->dir_pushcount_at_credit_expiry = 0;
	qm_port->cached_dir_credits = cfg.dir_credit_high_watermark;
	qm_port->cq_depth = cfg.cq_depth;
	/* CQs with depth < 8 use an 8-entry queue, but withhold credits so
	 * the effective depth is smaller.
	 */
	qm_port->cq_depth = cfg.cq_depth <= 8 ? 8 : cfg.cq_depth;
	qm_port->cq_idx = 0;
	qm_port->cq_idx_unmasked = 0;
	if (dlb->poll_mode == DLB_CQ_POLL_MODE_SPARSE)
		qm_port->cq_depth_mask = (qm_port->cq_depth * 4) - 1;
	else
		qm_port->cq_depth_mask = qm_port->cq_depth - 1;

	qm_port->gen_bit_shift = __builtin_popcount(qm_port->cq_depth_mask);
	/* starting value of gen bit - it toggles at wrap time */
	qm_port->gen_bit = 1;

	qm_port->use_rsvd_token_scheme = use_rsvd_token_scheme;
	qm_port->cq_rsvd_token_deficit = rsvd_tokens;
	qm_port->int_armed = false;

	/* Save off for later use in info and lookup APIs. */
	qm_port->qid_mappings = &dlb->qm_ldb_to_ev_queue_id[0];

	qm_port->dequeue_depth = dequeue_depth;

	qm_port->owed_tokens = 0;
	qm_port->issued_releases = 0;

	/* update state */
	qm_port->state = PORT_STARTED; /* enabled at create time */
	qm_port->config_state = DLB_CONFIGURED;

	qm_port->dir_credits = cfg.dir_credit_high_watermark;
	qm_port->ldb_credits = cfg.ldb_credit_high_watermark;

	DLB_LOG_DBG("dlb: created ldb port %d, depth = %d, ldb credits=%d, dir credits=%d\n",
		    qm_port_id,
		    cq_depth,
		    qm_port->ldb_credits,
		    qm_port->dir_credits);

	rte_spinlock_unlock(&handle->resource_lock);

	return 0;

error_exit:
	if (qm_port) {
		dlb_free_qe_mem(qm_port);
		qm_port->pp_mmio_base = 0;
	}

	rte_spinlock_unlock(&handle->resource_lock);

	DLB_LOG_ERR("dlb: create ldb port failed!\n");

	return ret;
}

static int
dlb_hw_create_dir_port(struct dlb_eventdev *dlb,
		       struct dlb_eventdev_port *ev_port,
		       uint32_t dequeue_depth,
		       uint32_t cq_depth,
		       uint32_t enqueue_depth,
		       uint16_t rsvd_tokens,
		       bool use_rsvd_token_scheme)
{
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_create_dir_port_args cfg = {0};
	struct dlb_cmd_response response = {0};
	int ret;
	struct dlb_port *qm_port = NULL;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	uint32_t qm_port_id;

	if (dlb == NULL || handle == NULL)
		return -EINVAL;

	if (cq_depth < DLB_MIN_DIR_CQ_DEPTH) {
		DLB_LOG_ERR("dlb: invalid cq_depth, must be at least %d\n",
			    DLB_MIN_DIR_CQ_DEPTH);
		return -EINVAL;
	}

	if (enqueue_depth < DLB_MIN_ENQUEUE_DEPTH) {
		DLB_LOG_ERR("dlb: invalid enqueue_depth, must be at least %d\n",
			    DLB_MIN_ENQUEUE_DEPTH);
		return -EINVAL;
	}

	rte_spinlock_lock(&handle->resource_lock);

	/* Directed queues are configured at link time. */
	cfg.queue_id = -1;

	cfg.response = (uintptr_t)&response;

	/* We round up to the next power of 2 if necessary */
	cfg.cq_depth = rte_align32pow2(cq_depth);
	cfg.cq_depth_threshold = rsvd_tokens;

	/* User controls the LDB high watermark via enqueue depth. The DIR high
	 * watermark is equal, unless the directed credit pool is too small.
	 */
	cfg.ldb_credit_high_watermark = enqueue_depth;

	/* Don't use enqueue_depth if it would require more directed credits
	 * than are available.
	 */
	cfg.dir_credit_high_watermark =
		RTE_MIN(enqueue_depth,
			handle->cfg.num_dir_credits / dlb->num_ports);

	cfg.ldb_credit_quantum = cfg.ldb_credit_high_watermark / 2;
	cfg.ldb_credit_low_watermark = RTE_MIN(16, cfg.ldb_credit_quantum);

	cfg.dir_credit_quantum = cfg.dir_credit_high_watermark / 2;
	cfg.dir_credit_low_watermark = RTE_MIN(16, cfg.dir_credit_quantum);

	/* Per QM values */

	cfg.ldb_credit_pool_id = handle->cfg.ldb_credit_pool_id;
	cfg.dir_credit_pool_id = handle->cfg.dir_credit_pool_id;

	ret = dlb_iface_dir_port_create(handle, &cfg, dlb->poll_mode);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: dlb_dir_port_create error, ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		goto error_exit;
	}

	qm_port_id = response.id;

	DLB_LOG_DBG("dlb: ev_port %d uses qm DIR port %d <<<<<\n",
		    ev_port->id, qm_port_id);

	qm_port = &ev_port->qm_port;
	qm_port->ev_port = ev_port; /* back ptr */
	qm_port->dlb = dlb;  /* back ptr */

	/*
	 * Init local qe struct(s).
	 * Note: MOVDIR64 requires the enqueue QE to be aligned
	 */

	snprintf(mz_name, sizeof(mz_name), "dir_port%d",
		 ev_port->id);

	ret = dlb_init_qe_mem(qm_port, mz_name);

	if (ret < 0) {
		DLB_LOG_ERR("dlb: init_qe_mem failed, ret=%d\n", ret);
		goto error_exit;
	}

	qm_port->pp_mmio_base = DLB_DIR_PP_BASE + PAGE_SIZE * qm_port_id;
	qm_port->id = qm_port_id;

	/* The credit window is one high water mark of QEs */
	qm_port->ldb_pushcount_at_credit_expiry = 0;
	qm_port->cached_ldb_credits = cfg.ldb_credit_high_watermark;
	/* The credit window is one high water mark of QEs */
	qm_port->dir_pushcount_at_credit_expiry = 0;
	qm_port->cached_dir_credits = cfg.dir_credit_high_watermark;
	qm_port->cq_depth = cfg.cq_depth;
	qm_port->cq_idx = 0;
	qm_port->cq_idx_unmasked = 0;
	if (dlb->poll_mode == DLB_CQ_POLL_MODE_SPARSE)
		qm_port->cq_depth_mask = (cfg.cq_depth * 4) - 1;
	else
		qm_port->cq_depth_mask = cfg.cq_depth - 1;

	qm_port->gen_bit_shift = __builtin_popcount(qm_port->cq_depth_mask);
	/* starting value of gen bit - it toggles at wrap time */
	qm_port->gen_bit = 1;

	qm_port->use_rsvd_token_scheme = use_rsvd_token_scheme;
	qm_port->cq_rsvd_token_deficit = rsvd_tokens;
	qm_port->int_armed = false;

	/* Save off for later use in info and lookup APIs. */
	qm_port->qid_mappings = &dlb->qm_dir_to_ev_queue_id[0];

	qm_port->dequeue_depth = dequeue_depth;

	qm_port->owed_tokens = 0;
	qm_port->issued_releases = 0;

	/* update state */
	qm_port->state = PORT_STARTED; /* enabled at create time */
	qm_port->config_state = DLB_CONFIGURED;

	qm_port->dir_credits = cfg.dir_credit_high_watermark;
	qm_port->ldb_credits = cfg.ldb_credit_high_watermark;

	DLB_LOG_DBG("dlb: created dir port %d, depth = %d cr=%d,%d\n",
		    qm_port_id,
		    cq_depth,
		    cfg.dir_credit_high_watermark,
		    cfg.ldb_credit_high_watermark);

	rte_spinlock_unlock(&handle->resource_lock);

	return 0;

error_exit:
	if (qm_port) {
		qm_port->pp_mmio_base = 0;
		dlb_free_qe_mem(qm_port);
	}

	rte_spinlock_unlock(&handle->resource_lock);

	DLB_LOG_ERR("dlb: create dir port failed!\n");

	return ret;
}

static int32_t
dlb_hw_create_ldb_queue(struct dlb_eventdev *dlb,
			struct dlb_queue *queue,
			const struct rte_event_queue_conf *evq_conf)
{
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_create_ldb_queue_args cfg;
	struct dlb_cmd_response response;
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

	cfg.response = (uintptr_t)&response;
	cfg.num_atomic_inflights = dlb->num_atm_inflights_per_queue;
	cfg.num_sequence_numbers = evq_conf->nb_atomic_order_sequences;
	cfg.num_qid_inflights = evq_conf->nb_atomic_order_sequences;

	if (sched_type != RTE_SCHED_TYPE_ORDERED) {
		cfg.num_sequence_numbers = 0;
		cfg.num_qid_inflights = DLB_DEF_UNORDERED_QID_INFLIGHTS;
	}

	ret = dlb_iface_ldb_queue_create(handle, &cfg);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: create LB event queue error, ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		return -EINVAL;
	}

	qm_qid = response.id;

	/* Save off queue config for debug, resource lookups, and reconfig */
	queue->num_qid_inflights = cfg.num_qid_inflights;
	queue->num_atm_inflights = cfg.num_atomic_inflights;

	queue->sched_type = sched_type;
	queue->config_state = DLB_CONFIGURED;

	DLB_LOG_DBG("Created LB event queue %d, nb_inflights=%d, nb_seq=%d, qid inflights=%d\n",
		    qm_qid,
		    cfg.num_atomic_inflights,
		    cfg.num_sequence_numbers,
		    cfg.num_qid_inflights);

	return qm_qid;
}

static int32_t
dlb_get_sn_allocation(struct dlb_eventdev *dlb, int group)
{
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_get_sn_allocation_args cfg;
	struct dlb_cmd_response response;
	int ret;

	cfg.group = group;
	cfg.response = (uintptr_t)&response;

	ret = dlb_iface_get_sn_allocation(handle, &cfg);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: get_sn_allocation ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		return ret;
	}

	return response.id;
}

static int
dlb_set_sn_allocation(struct dlb_eventdev *dlb, int group, int num)
{
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_set_sn_allocation_args cfg;
	struct dlb_cmd_response response;
	int ret;

	cfg.num = num;
	cfg.group = group;
	cfg.response = (uintptr_t)&response;

	ret = dlb_iface_set_sn_allocation(handle, &cfg);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: set_sn_allocation ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		return ret;
	}

	return ret;
}

static int32_t
dlb_get_sn_occupancy(struct dlb_eventdev *dlb, int group)
{
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_get_sn_occupancy_args cfg;
	struct dlb_cmd_response response;
	int ret;

	cfg.group = group;
	cfg.response = (uintptr_t)&response;

	ret = dlb_iface_get_sn_occupancy(handle, &cfg);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: get_sn_occupancy ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		return ret;
	}

	return response.id;
}

/* Query the current sequence number allocations and, if they conflict with the
 * requested LDB queue configuration, attempt to re-allocate sequence numbers.
 * This is best-effort; if it fails, the PMD will attempt to configure the
 * load-balanced queue and return an error.
 */
static void
dlb_program_sn_allocation(struct dlb_eventdev *dlb,
			  const struct rte_event_queue_conf *queue_conf)
{
	int grp_occupancy[DLB_NUM_SN_GROUPS];
	int grp_alloc[DLB_NUM_SN_GROUPS];
	int i, sequence_numbers;

	sequence_numbers = (int)queue_conf->nb_atomic_order_sequences;

	for (i = 0; i < DLB_NUM_SN_GROUPS; i++) {
		int total_slots;

		grp_alloc[i] = dlb_get_sn_allocation(dlb, i);
		if (grp_alloc[i] < 0)
			return;

		total_slots = DLB_MAX_LDB_SN_ALLOC / grp_alloc[i];

		grp_occupancy[i] = dlb_get_sn_occupancy(dlb, i);
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
	for (i = 0; i < DLB_NUM_SN_GROUPS; i++) {
		if (grp_occupancy[i] == 0)
			break;
	}

	if (i == DLB_NUM_SN_GROUPS) {
		DLB_LOG_ERR("[%s()] No groups with %d sequence_numbers are available or have free slots\n",
		       __func__, sequence_numbers);
		return;
	}

	/* Attempt to configure slot i with the requested number of sequence
	 * numbers. Ignore the return value -- if this fails, the error will be
	 * caught during subsequent queue configuration.
	 */
	dlb_set_sn_allocation(dlb, i, sequence_numbers);
}

static int
dlb_eventdev_ldb_queue_setup(struct rte_eventdev *dev,
			     struct dlb_eventdev_queue *ev_queue,
			     const struct rte_event_queue_conf *queue_conf)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);
	int32_t qm_qid;

	if (queue_conf->nb_atomic_order_sequences)
		dlb_program_sn_allocation(dlb, queue_conf);

	qm_qid = dlb_hw_create_ldb_queue(dlb,
					 &ev_queue->qm_queue,
					 queue_conf);
	if (qm_qid < 0) {
		DLB_LOG_ERR("Failed to create the load-balanced queue\n");

		return qm_qid;
	}

	dlb->qm_ldb_to_ev_queue_id[qm_qid] = ev_queue->id;

	ev_queue->qm_queue.id = qm_qid;

	return 0;
}

static int dlb_num_dir_queues_setup(struct dlb_eventdev *dlb)
{
	int i, num = 0;

	for (i = 0; i < dlb->num_queues; i++) {
		if (dlb->ev_queues[i].setup_done &&
		    dlb->ev_queues[i].qm_queue.is_directed)
			num++;
	}

	return num;
}

static void
dlb_queue_link_teardown(struct dlb_eventdev *dlb,
			struct dlb_eventdev_queue *ev_queue)
{
	struct dlb_eventdev_port *ev_port;
	int i, j;

	for (i = 0; i < dlb->num_ports; i++) {
		ev_port = &dlb->ev_ports[i];

		for (j = 0; j < DLB_MAX_NUM_QIDS_PER_LDB_CQ; j++) {
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
dlb_eventdev_queue_setup(struct rte_eventdev *dev,
			 uint8_t ev_qid,
			 const struct rte_event_queue_conf *queue_conf)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);
	struct dlb_eventdev_queue *ev_queue;
	int ret;

	if (queue_conf == NULL)
		return -EINVAL;

	if (ev_qid >= dlb->num_queues)
		return -EINVAL;

	ev_queue = &dlb->ev_queues[ev_qid];

	ev_queue->qm_queue.is_directed = queue_conf->event_queue_cfg &
		RTE_EVENT_QUEUE_CFG_SINGLE_LINK;
	ev_queue->id = ev_qid;
	ev_queue->conf = *queue_conf;

	if (!ev_queue->qm_queue.is_directed) {
		ret = dlb_eventdev_ldb_queue_setup(dev, ev_queue, queue_conf);
	} else {
		/* The directed queue isn't setup until link time, at which
		 * point we know its directed port ID. Directed queue setup
		 * will only fail if this queue is already setup or there are
		 * no directed queues left to configure.
		 */
		ret = 0;

		ev_queue->qm_queue.config_state = DLB_NOT_CONFIGURED;

		if (ev_queue->setup_done ||
		    dlb_num_dir_queues_setup(dlb) == dlb->num_dir_queues)
			ret = -EINVAL;
	}

	/* Tear down pre-existing port->queue links */
	if (!ret && dlb->run_state == DLB_RUN_STATE_STOPPED)
		dlb_queue_link_teardown(dlb, ev_queue);

	if (!ret)
		ev_queue->setup_done = true;

	return ret;
}

static void
dlb_port_link_teardown(struct dlb_eventdev *dlb,
		       struct dlb_eventdev_port *ev_port)
{
	struct dlb_eventdev_queue *ev_queue;
	int i;

	for (i = 0; i < DLB_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		if (!ev_port->link[i].valid)
			continue;

		ev_queue = &dlb->ev_queues[ev_port->link[i].queue_id];

		ev_port->link[i].valid = false;
		ev_port->num_links--;
		ev_queue->num_links--;
	}
}

static int
dlb_eventdev_port_setup(struct rte_eventdev *dev,
			uint8_t ev_port_id,
			const struct rte_event_port_conf *port_conf)
{
	struct dlb_eventdev *dlb;
	struct dlb_eventdev_port *ev_port;
	bool use_rsvd_token_scheme;
	uint32_t adj_cq_depth;
	uint16_t rsvd_tokens;
	int ret;

	if (dev == NULL || port_conf == NULL) {
		DLB_LOG_ERR("Null parameter\n");
		return -EINVAL;
	}

	dlb = dlb_pmd_priv(dev);

	if (ev_port_id >= DLB_MAX_NUM_PORTS)
		return -EINVAL;

	if (port_conf->dequeue_depth >
		evdev_dlb_default_info.max_event_port_dequeue_depth ||
	    port_conf->enqueue_depth >
		evdev_dlb_default_info.max_event_port_enqueue_depth)
		return -EINVAL;

	ev_port = &dlb->ev_ports[ev_port_id];
	/* configured? */
	if (ev_port->setup_done) {
		DLB_LOG_ERR("evport %d is already configured\n", ev_port_id);
		return -EINVAL;
	}

	/* The reserved token interrupt arming scheme requires that one or more
	 * CQ tokens be reserved by the PMD. This limits the amount of CQ space
	 * usable by the DLB, so in order to give an *effective* CQ depth equal
	 * to the user-requested value, we double CQ depth and reserve half of
	 * its tokens. If the user requests the max CQ depth (256) then we
	 * cannot double it, so we reserve one token and give an effective
	 * depth of 255 entries.
	 */
	use_rsvd_token_scheme = true;
	rsvd_tokens = 1;
	adj_cq_depth = port_conf->dequeue_depth;

	if (use_rsvd_token_scheme && adj_cq_depth < 256) {
		rsvd_tokens = adj_cq_depth;
		adj_cq_depth *= 2;
	}

	ev_port->qm_port.is_directed = port_conf->event_port_cfg &
		RTE_EVENT_PORT_CFG_SINGLE_LINK;

	if (!ev_port->qm_port.is_directed) {
		ret = dlb_hw_create_ldb_port(dlb,
					     ev_port,
					     port_conf->dequeue_depth,
					     adj_cq_depth,
					     port_conf->enqueue_depth,
					     rsvd_tokens,
					     use_rsvd_token_scheme);
		if (ret < 0) {
			DLB_LOG_ERR("Failed to create the lB port ve portId=%d\n",
				    ev_port_id);
			return ret;
		}
	} else {
		ret = dlb_hw_create_dir_port(dlb,
					     ev_port,
					     port_conf->dequeue_depth,
					     adj_cq_depth,
					     port_conf->enqueue_depth,
					     rsvd_tokens,
					     use_rsvd_token_scheme);
		if (ret < 0) {
			DLB_LOG_ERR("Failed to create the DIR port\n");
			return ret;
		}
	}

	/* Save off port config for reconfig */
	dlb->ev_ports[ev_port_id].conf = *port_conf;

	dlb->ev_ports[ev_port_id].id = ev_port_id;
	dlb->ev_ports[ev_port_id].enq_configured = true;
	dlb->ev_ports[ev_port_id].setup_done = true;
	dlb->ev_ports[ev_port_id].inflight_max =
		port_conf->new_event_threshold;
	dlb->ev_ports[ev_port_id].implicit_release =
		!(port_conf->event_port_cfg &
		  RTE_EVENT_PORT_CFG_DISABLE_IMPL_REL);
	dlb->ev_ports[ev_port_id].outstanding_releases = 0;
	dlb->ev_ports[ev_port_id].inflight_credits = 0;
	dlb->ev_ports[ev_port_id].credit_update_quanta =
		RTE_LIBRTE_PMD_DLB_SW_CREDIT_QUANTA;
	dlb->ev_ports[ev_port_id].dlb = dlb; /* reverse link */

	/* Tear down pre-existing port->queue links */
	if (dlb->run_state == DLB_RUN_STATE_STOPPED)
		dlb_port_link_teardown(dlb, &dlb->ev_ports[ev_port_id]);

	dev->data->ports[ev_port_id] = &dlb->ev_ports[ev_port_id];

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
		.dev_infos_get    = dlb_eventdev_info_get,
		.dev_configure    = dlb_eventdev_configure,
		.queue_def_conf   = dlb_eventdev_queue_default_conf_get,
		.port_def_conf    = dlb_eventdev_port_default_conf_get,
		.queue_setup      = dlb_eventdev_queue_setup,
		.port_setup       = dlb_eventdev_port_setup,
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
