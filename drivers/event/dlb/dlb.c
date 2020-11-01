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

static int16_t
dlb_hw_unmap_ldb_qid_from_port(struct dlb_hw_dev *handle,
			       uint32_t qm_port_id,
			       uint16_t qm_qid)
{
	struct dlb_unmap_qid_args cfg;
	struct dlb_cmd_response response;
	int32_t ret;

	if (handle == NULL)
		return -EINVAL;

	cfg.response = (uintptr_t)&response;
	cfg.port_id = qm_port_id;
	cfg.qid = qm_qid;

	ret = dlb_iface_unmap_qid(handle, &cfg);
	if (ret < 0)
		DLB_LOG_ERR("dlb: unmap qid error, ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);

	return ret;
}

static int
dlb_event_queue_detach_ldb(struct dlb_eventdev *dlb,
			   struct dlb_eventdev_port *ev_port,
			   struct dlb_eventdev_queue *ev_queue)
{
	int ret, i;

	/* Don't unlink until start time. */
	if (dlb->run_state == DLB_RUN_STATE_STOPPED)
		return 0;

	for (i = 0; i < DLB_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		if (ev_port->link[i].valid &&
		    ev_port->link[i].queue_id == ev_queue->id)
			break; /* found */
	}

	/* This is expected with eventdev API!
	 * It blindly attempts to unmap all queues.
	 */
	if (i == DLB_MAX_NUM_QIDS_PER_LDB_CQ) {
		DLB_LOG_DBG("dlb: ignoring LB QID %d not mapped for qm_port %d.\n",
			    ev_queue->qm_queue.id,
			    ev_port->qm_port.id);
		return 0;
	}

	ret = dlb_hw_unmap_ldb_qid_from_port(&dlb->qm_instance,
					     ev_port->qm_port.id,
					     ev_queue->qm_queue.id);
	if (!ret)
		ev_port->link[i].mapped = false;

	return ret;
}

static int
dlb_eventdev_port_unlink(struct rte_eventdev *dev, void *event_port,
			 uint8_t queues[], uint16_t nb_unlinks)
{
	struct dlb_eventdev_port *ev_port = event_port;
	struct dlb_eventdev *dlb;
	int i;

	RTE_SET_USED(dev);

	if (!ev_port->setup_done) {
		DLB_LOG_ERR("dlb: evport %d is not configured\n",
			    ev_port->id);
		rte_errno = -EINVAL;
		return 0;
	}

	if (queues == NULL || nb_unlinks == 0) {
		DLB_LOG_DBG("dlb: queues is NULL or nb_unlinks is 0\n");
		return 0; /* Ignore and return success */
	}

	if (ev_port->qm_port.is_directed) {
		DLB_LOG_DBG("dlb: ignore unlink from dir port %d\n",
			    ev_port->id);
		rte_errno = 0;
		return nb_unlinks; /* as if success */
	}

	dlb = ev_port->dlb;

	for (i = 0; i < nb_unlinks; i++) {
		struct dlb_eventdev_queue *ev_queue;
		int ret, j;

		if (queues[i] >= dlb->num_queues) {
			DLB_LOG_ERR("dlb: invalid queue id %d\n", queues[i]);
			rte_errno = -EINVAL;
			return i; /* return index of offending queue */
		}

		ev_queue = &dlb->ev_queues[queues[i]];

		/* Does a link exist? */
		for (j = 0; j < DLB_MAX_NUM_QIDS_PER_LDB_CQ; j++)
			if (ev_port->link[j].queue_id == queues[i] &&
			    ev_port->link[j].valid)
				break;

		if (j == DLB_MAX_NUM_QIDS_PER_LDB_CQ)
			continue;

		ret = dlb_event_queue_detach_ldb(dlb, ev_port, ev_queue);
		if (ret) {
			DLB_LOG_ERR("unlink err=%d for port %d queue %d\n",
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
dlb_eventdev_port_unlinks_in_progress(struct rte_eventdev *dev,
				      void *event_port)
{
	struct dlb_eventdev_port *ev_port = event_port;
	struct dlb_eventdev *dlb;
	struct dlb_hw_dev *handle;
	struct dlb_pending_port_unmaps_args cfg;
	struct dlb_cmd_response response;
	int ret;

	RTE_SET_USED(dev);

	if (!ev_port->setup_done) {
		DLB_LOG_ERR("dlb: evport %d is not configured\n",
			    ev_port->id);
		rte_errno = -EINVAL;
		return 0;
	}

	cfg.port_id = ev_port->qm_port.id;
	cfg.response = (uintptr_t)&response;
	dlb = ev_port->dlb;
	handle = &dlb->qm_instance;
	ret = dlb_iface_pending_port_unmaps(handle, &cfg);

	if (ret < 0) {
		DLB_LOG_ERR("dlb: num_unlinks_in_progress ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		return ret;
	}

	return response.id;
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
dlb_eventdev_reapply_configuration(struct rte_eventdev *dev)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);
	int ret, i;

	/* If an event queue or port was previously configured, but hasn't been
	 * reconfigured, reapply its original configuration.
	 */
	for (i = 0; i < dlb->num_queues; i++) {
		struct dlb_eventdev_queue *ev_queue;

		ev_queue = &dlb->ev_queues[i];

		if (ev_queue->qm_queue.config_state != DLB_PREV_CONFIGURED)
			continue;

		ret = dlb_eventdev_queue_setup(dev, i, &ev_queue->conf);
		if (ret < 0) {
			DLB_LOG_ERR("dlb: failed to reconfigure queue %d", i);
			return ret;
		}
	}

	for (i = 0; i < dlb->num_ports; i++) {
		struct dlb_eventdev_port *ev_port = &dlb->ev_ports[i];

		if (ev_port->qm_port.config_state != DLB_PREV_CONFIGURED)
			continue;

		ret = dlb_eventdev_port_setup(dev, i, &ev_port->conf);
		if (ret < 0) {
			DLB_LOG_ERR("dlb: failed to reconfigure ev_port %d",
				    i);
			return ret;
		}
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

static int
dlb_validate_port_link(struct dlb_eventdev_port *ev_port,
		       uint8_t queue_id,
		       bool link_exists,
		       int index)
{
	struct dlb_eventdev *dlb = ev_port->dlb;
	struct dlb_eventdev_queue *ev_queue;
	bool port_is_dir, queue_is_dir;

	if (queue_id > dlb->num_queues) {
		DLB_LOG_ERR("queue_id %d > num queues %d\n",
			    queue_id, dlb->num_queues);
		rte_errno = -EINVAL;
		return -1;
	}

	ev_queue = &dlb->ev_queues[queue_id];

	if (!ev_queue->setup_done &&
	    ev_queue->qm_queue.config_state != DLB_PREV_CONFIGURED) {
		DLB_LOG_ERR("setup not done and not previously configured\n");
		rte_errno = -EINVAL;
		return -1;
	}

	port_is_dir = ev_port->qm_port.is_directed;
	queue_is_dir = ev_queue->qm_queue.is_directed;

	if (port_is_dir != queue_is_dir) {
		DLB_LOG_ERR("%s queue %u can't link to %s port %u\n",
			    queue_is_dir ? "DIR" : "LDB", ev_queue->id,
			    port_is_dir ? "DIR" : "LDB", ev_port->id);

		rte_errno = -EINVAL;
		return -1;
	}

	/* Check if there is space for the requested link */
	if (!link_exists && index == -1) {
		DLB_LOG_ERR("no space for new link\n");
		rte_errno = -ENOSPC;
		return -1;
	}

	/* Check if the directed port is already linked */
	if (ev_port->qm_port.is_directed && ev_port->num_links > 0 &&
	    !link_exists) {
		DLB_LOG_ERR("Can't link DIR port %d to >1 queues\n",
			    ev_port->id);
		rte_errno = -EINVAL;
		return -1;
	}

	/* Check if the directed queue is already linked */
	if (ev_queue->qm_queue.is_directed && ev_queue->num_links > 0 &&
	    !link_exists) {
		DLB_LOG_ERR("Can't link DIR queue %d to >1 ports\n",
			    ev_queue->id);
		rte_errno = -EINVAL;
		return -1;
	}

	return 0;
}

static int32_t
dlb_hw_create_dir_queue(struct dlb_eventdev *dlb, int32_t qm_port_id)
{
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_create_dir_queue_args cfg;
	struct dlb_cmd_response response;
	int32_t ret;

	cfg.response = (uintptr_t)&response;

	/* The directed port is always configured before its queue */
	cfg.port_id = qm_port_id;

	ret = dlb_iface_dir_queue_create(handle, &cfg);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: create DIR event queue error, ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		return -EINVAL;
	}

	return response.id;
}

static int
dlb_eventdev_dir_queue_setup(struct dlb_eventdev *dlb,
			     struct dlb_eventdev_queue *ev_queue,
			     struct dlb_eventdev_port *ev_port)
{
	int32_t qm_qid;

	qm_qid = dlb_hw_create_dir_queue(dlb, ev_port->qm_port.id);

	if (qm_qid < 0) {
		DLB_LOG_ERR("Failed to create the DIR queue\n");
		return qm_qid;
	}

	dlb->qm_dir_to_ev_queue_id[qm_qid] = ev_queue->id;

	ev_queue->qm_queue.id = qm_qid;

	return 0;
}

static int16_t
dlb_hw_map_ldb_qid_to_port(struct dlb_hw_dev *handle,
			   uint32_t qm_port_id,
			   uint16_t qm_qid,
			   uint8_t priority)
{
	struct dlb_map_qid_args cfg;
	struct dlb_cmd_response response;
	int32_t ret;

	if (handle == NULL)
		return -EINVAL;

	/* Build message */
	cfg.response = (uintptr_t)&response;
	cfg.port_id = qm_port_id;
	cfg.qid = qm_qid;
	cfg.priority = EV_TO_DLB_PRIO(priority);

	ret = dlb_iface_map_qid(handle, &cfg);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: map qid error, ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		DLB_LOG_ERR("dlb: device_id=%d grp=%d, qm_port=%d, qm_qid=%d prio=%d\n",
			    handle->device_id,
			    handle->domain_id, cfg.port_id,
			    cfg.qid,
			    cfg.priority);
	} else {
		DLB_LOG_DBG("dlb: mapped queue %d to qm_port %d\n",
			    qm_qid, qm_port_id);
	}

	return ret;
}

static int
dlb_event_queue_join_ldb(struct dlb_eventdev *dlb,
			 struct dlb_eventdev_port *ev_port,
			 struct dlb_eventdev_queue *ev_queue,
			 uint8_t priority)
{
	int first_avail = -1;
	int ret, i;

	for (i = 0; i < DLB_MAX_NUM_QIDS_PER_LDB_CQ; i++) {
		if (ev_port->link[i].valid) {
			if (ev_port->link[i].queue_id == ev_queue->id &&
			    ev_port->link[i].priority == priority) {
				if (ev_port->link[i].mapped)
					return 0; /* already mapped */
				first_avail = i;
			}
		} else {
			if (first_avail == -1)
				first_avail = i;
		}
	}
	if (first_avail == -1) {
		DLB_LOG_ERR("dlb: qm_port %d has no available QID slots.\n",
			    ev_port->qm_port.id);
		return -EINVAL;
	}

	ret = dlb_hw_map_ldb_qid_to_port(&dlb->qm_instance,
					 ev_port->qm_port.id,
					 ev_queue->qm_queue.id,
					 priority);

	if (!ret)
		ev_port->link[first_avail].mapped = true;

	return ret;
}

static int
dlb_do_port_link(struct rte_eventdev *dev,
		 struct dlb_eventdev_queue *ev_queue,
		 struct dlb_eventdev_port *ev_port,
		 uint8_t prio)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);
	int err;

	/* Don't link until start time. */
	if (dlb->run_state == DLB_RUN_STATE_STOPPED)
		return 0;

	if (ev_queue->qm_queue.is_directed)
		err = dlb_eventdev_dir_queue_setup(dlb, ev_queue, ev_port);
	else
		err = dlb_event_queue_join_ldb(dlb, ev_port, ev_queue, prio);

	if (err) {
		DLB_LOG_ERR("port link failure for %s ev_q %d, ev_port %d\n",
			    ev_queue->qm_queue.is_directed ? "DIR" : "LDB",
			    ev_queue->id, ev_port->id);

		rte_errno = err;
		return -1;
	}

	return 0;
}

static int
dlb_eventdev_apply_port_links(struct rte_eventdev *dev)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);
	int i;

	/* Perform requested port->queue links */
	for (i = 0; i < dlb->num_ports; i++) {
		struct dlb_eventdev_port *ev_port = &dlb->ev_ports[i];
		int j;

		for (j = 0; j < DLB_MAX_NUM_QIDS_PER_LDB_CQ; j++) {
			struct dlb_eventdev_queue *ev_queue;
			uint8_t prio, queue_id;

			if (!ev_port->link[j].valid)
				continue;

			prio = ev_port->link[j].priority;
			queue_id = ev_port->link[j].queue_id;

			if (dlb_validate_port_link(ev_port, queue_id, true, j))
				return -EINVAL;

			ev_queue = &dlb->ev_queues[queue_id];

			if (dlb_do_port_link(dev, ev_queue, ev_port, prio))
				return -EINVAL;
		}
	}

	return 0;
}

static int
dlb_eventdev_port_link(struct rte_eventdev *dev, void *event_port,
		       const uint8_t queues[], const uint8_t priorities[],
		       uint16_t nb_links)

{
	struct dlb_eventdev_port *ev_port = event_port;
	struct dlb_eventdev *dlb;
	int i, j;

	RTE_SET_USED(dev);

	if (ev_port == NULL) {
		DLB_LOG_ERR("dlb: evport not setup\n");
		rte_errno = -EINVAL;
		return 0;
	}

	if (!ev_port->setup_done &&
	    ev_port->qm_port.config_state != DLB_PREV_CONFIGURED) {
		DLB_LOG_ERR("dlb: evport not setup\n");
		rte_errno = -EINVAL;
		return 0;
	}

	/* Note: rte_event_port_link() ensures the PMD won't receive a NULL
	 * queues pointer.
	 */
	if (nb_links == 0) {
		DLB_LOG_DBG("dlb: nb_links is 0\n");
		return 0; /* Ignore and return success */
	}

	dlb = ev_port->dlb;

	DLB_LOG_DBG("Linking %u queues to %s port %d\n",
		    nb_links,
		    ev_port->qm_port.is_directed ? "DIR" : "LDB",
		    ev_port->id);

	for (i = 0; i < nb_links; i++) {
		struct dlb_eventdev_queue *ev_queue;
		uint8_t queue_id, prio;
		bool found = false;
		int index = -1;

		queue_id = queues[i];
		prio = priorities[i];

		/* Check if the link already exists. */
		for (j = 0; j < DLB_MAX_NUM_QIDS_PER_LDB_CQ; j++)
			if (ev_port->link[j].valid) {
				if (ev_port->link[j].queue_id == queue_id) {
					found = true;
					index = j;
					break;
				}
			} else {
				if (index == -1)
					index = j;
			}

		/* could not link */
		if (index == -1)
			break;

		/* Check if already linked at the requested priority */
		if (found && ev_port->link[j].priority == prio)
			continue;

		if (dlb_validate_port_link(ev_port, queue_id, found, index))
			break; /* return index of offending queue */

		ev_queue = &dlb->ev_queues[queue_id];

		if (dlb_do_port_link(dev, ev_queue, ev_port, prio))
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

static int
dlb_eventdev_start(struct rte_eventdev *dev)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_start_domain_args cfg;
	struct dlb_cmd_response response;
	int ret, i;

	rte_spinlock_lock(&dlb->qm_instance.resource_lock);
	if (dlb->run_state != DLB_RUN_STATE_STOPPED) {
		DLB_LOG_ERR("bad state %d for dev_start\n",
			    (int)dlb->run_state);
		rte_spinlock_unlock(&dlb->qm_instance.resource_lock);
		return -EINVAL;
	}
	dlb->run_state	= DLB_RUN_STATE_STARTING;
	rte_spinlock_unlock(&dlb->qm_instance.resource_lock);

	/* If the device was configured more than once, some event ports and/or
	 * queues may need to be reconfigured.
	 */
	ret = dlb_eventdev_reapply_configuration(dev);
	if (ret)
		return ret;

	/* The DLB PMD delays port links until the device is started. */
	ret = dlb_eventdev_apply_port_links(dev);
	if (ret)
		return ret;

	cfg.response = (uintptr_t)&response;

	for (i = 0; i < dlb->num_ports; i++) {
		if (!dlb->ev_ports[i].setup_done) {
			DLB_LOG_ERR("dlb: port %d not setup", i);
			return -ESTALE;
		}
	}

	for (i = 0; i < dlb->num_queues; i++) {
		if (dlb->ev_queues[i].num_links == 0) {
			DLB_LOG_ERR("dlb: queue %d is not linked", i);
			return -ENOLINK;
		}
	}

	ret = dlb_iface_sched_domain_start(handle, &cfg);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: sched_domain_start ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		return ret;
	}

	dlb->run_state = DLB_RUN_STATE_STARTED;
	DLB_LOG_DBG("dlb: sched_domain_start completed OK\n");

	return 0;
}

static inline int
dlb_check_enqueue_sw_credits(struct dlb_eventdev *dlb,
			     struct dlb_eventdev_port *ev_port)
{
	uint32_t sw_inflights = __atomic_load_n(&dlb->inflights,
						__ATOMIC_SEQ_CST);
	const int num = 1;

	if (unlikely(ev_port->inflight_max < sw_inflights)) {
		DLB_INC_STAT(ev_port->stats.traffic.tx_nospc_inflight_max, 1);
		rte_errno = -ENOSPC;
		return 1;
	}

	if (ev_port->inflight_credits < num) {
		/* check if event enqueue brings ev_port over max threshold */
		uint32_t credit_update_quanta = ev_port->credit_update_quanta;

		if (sw_inflights + credit_update_quanta >
		    dlb->new_event_limit) {
			DLB_INC_STAT(
				ev_port->stats.traffic.tx_nospc_new_event_limit,
				1);
			rte_errno = -ENOSPC;
			return 1;
		}

		__atomic_fetch_add(&dlb->inflights, credit_update_quanta,
				   __ATOMIC_SEQ_CST);
		ev_port->inflight_credits += (credit_update_quanta);

		if (ev_port->inflight_credits < num) {
			DLB_INC_STAT(
			    ev_port->stats.traffic.tx_nospc_inflight_credits,
			    1);
			rte_errno = -ENOSPC;
			return 1;
		}
	}

	return 0;
}

static inline void
dlb_replenish_sw_credits(struct dlb_eventdev *dlb,
			 struct dlb_eventdev_port *ev_port)
{
	uint16_t quanta = ev_port->credit_update_quanta;

	if (ev_port->inflight_credits >= quanta * 2) {
		/* Replenish credits, saving one quanta for enqueues */
		uint16_t val = ev_port->inflight_credits - quanta;

		__atomic_fetch_sub(&dlb->inflights, val, __ATOMIC_SEQ_CST);
		ev_port->inflight_credits -= val;
	}
}

static __rte_always_inline uint16_t
dlb_read_pc(struct process_local_port_data *port_data, bool ldb)
{
	volatile uint16_t *popcount;

	if (ldb)
		popcount = port_data->ldb_popcount;
	else
		popcount = port_data->dir_popcount;

	return *popcount;
}

static inline int
dlb_check_enqueue_hw_ldb_credits(struct dlb_port *qm_port,
				 struct process_local_port_data *port_data)
{
	if (unlikely(qm_port->cached_ldb_credits == 0)) {
		uint16_t pc;

		pc = dlb_read_pc(port_data, true);

		qm_port->cached_ldb_credits = pc -
			qm_port->ldb_pushcount_at_credit_expiry;
		if (unlikely(qm_port->cached_ldb_credits == 0)) {
			DLB_INC_STAT(
			qm_port->ev_port->stats.traffic.tx_nospc_ldb_hw_credits,
			1);

			DLB_LOG_DBG("ldb credits exhausted\n");
			return 1;
		}
		qm_port->ldb_pushcount_at_credit_expiry +=
			qm_port->cached_ldb_credits;
	}

	return 0;
}

static inline int
dlb_check_enqueue_hw_dir_credits(struct dlb_port *qm_port,
				 struct process_local_port_data *port_data)
{
	if (unlikely(qm_port->cached_dir_credits == 0)) {
		uint16_t pc;

		pc = dlb_read_pc(port_data, false);

		qm_port->cached_dir_credits = pc -
			qm_port->dir_pushcount_at_credit_expiry;

		if (unlikely(qm_port->cached_dir_credits == 0)) {
			DLB_INC_STAT(
			qm_port->ev_port->stats.traffic.tx_nospc_dir_hw_credits,
			1);

			DLB_LOG_DBG("dir credits exhausted\n");
			return 1;
		}
		qm_port->dir_pushcount_at_credit_expiry +=
			qm_port->cached_dir_credits;
	}

	return 0;
}

static inline int
dlb_event_enqueue_prep(struct dlb_eventdev_port *ev_port,
		       struct dlb_port *qm_port,
		       const struct rte_event ev[],
		       struct process_local_port_data *port_data,
		       uint8_t *sched_type,
		       uint8_t *queue_id)
{
	struct dlb_eventdev *dlb = ev_port->dlb;
	struct dlb_eventdev_queue *ev_queue;
	uint16_t *cached_credits = NULL;
	struct dlb_queue *qm_queue;

	ev_queue = &dlb->ev_queues[ev->queue_id];
	qm_queue = &ev_queue->qm_queue;
	*queue_id = qm_queue->id;

	/* Ignore sched_type and hardware credits on release events */
	if (ev->op == RTE_EVENT_OP_RELEASE)
		goto op_check;

	if (!qm_queue->is_directed) {
		/* Load balanced destination queue */

		if (dlb_check_enqueue_hw_ldb_credits(qm_port, port_data)) {
			rte_errno = -ENOSPC;
			return 1;
		}
		cached_credits = &qm_port->cached_ldb_credits;

		switch (ev->sched_type) {
		case RTE_SCHED_TYPE_ORDERED:
			DLB_LOG_DBG("dlb: put_qe: RTE_SCHED_TYPE_ORDERED\n");
			if (qm_queue->sched_type != RTE_SCHED_TYPE_ORDERED) {
				DLB_LOG_ERR("dlb: tried to send ordered event to unordered queue %d\n",
					    *queue_id);
				rte_errno = -EINVAL;
				return 1;
			}
			*sched_type = DLB_SCHED_ORDERED;
			break;
		case RTE_SCHED_TYPE_ATOMIC:
			DLB_LOG_DBG("dlb: put_qe: RTE_SCHED_TYPE_ATOMIC\n");
			*sched_type = DLB_SCHED_ATOMIC;
			break;
		case RTE_SCHED_TYPE_PARALLEL:
			DLB_LOG_DBG("dlb: put_qe: RTE_SCHED_TYPE_PARALLEL\n");
			if (qm_queue->sched_type == RTE_SCHED_TYPE_ORDERED)
				*sched_type = DLB_SCHED_ORDERED;
			else
				*sched_type = DLB_SCHED_UNORDERED;
			break;
		default:
			DLB_LOG_ERR("Unsupported LDB sched type in put_qe\n");
			DLB_INC_STAT(ev_port->stats.tx_invalid, 1);
			rte_errno = -EINVAL;
			return 1;
		}
	} else {
		/* Directed destination queue */

		if (dlb_check_enqueue_hw_dir_credits(qm_port, port_data)) {
			rte_errno = -ENOSPC;
			return 1;
		}
		cached_credits = &qm_port->cached_dir_credits;

		DLB_LOG_DBG("dlb: put_qe: RTE_SCHED_TYPE_DIRECTED\n");

		*sched_type = DLB_SCHED_DIRECTED;
	}

op_check:
	switch (ev->op) {
	case RTE_EVENT_OP_NEW:
		/* Check that a sw credit is available */
		if (dlb_check_enqueue_sw_credits(dlb, ev_port)) {
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
		dlb_replenish_sw_credits(dlb, ev_port);
		break;
	}

	DLB_INC_STAT(ev_port->stats.tx_op_cnt[ev->op], 1);
	DLB_INC_STAT(ev_port->stats.traffic.tx_ok, 1);

#ifndef RTE_LIBRTE_PMD_DLB_QUELL_STATS
	if (ev->op != RTE_EVENT_OP_RELEASE) {
		DLB_INC_STAT(ev_port->stats.enq_ok[ev->queue_id], 1);
		DLB_INC_STAT(ev_port->stats.tx_sched_cnt[*sched_type], 1);
	}
#endif

	return 0;
}

static uint8_t cmd_byte_map[NUM_DLB_PORT_TYPES][DLB_NUM_HW_SCHED_TYPES] = {
	{
		/* Load-balanced cmd bytes */
		[RTE_EVENT_OP_NEW] = DLB_NEW_CMD_BYTE,
		[RTE_EVENT_OP_FORWARD] = DLB_FWD_CMD_BYTE,
		[RTE_EVENT_OP_RELEASE] = DLB_COMP_CMD_BYTE,
	},
	{
		/* Directed cmd bytes */
		[RTE_EVENT_OP_NEW] = DLB_NEW_CMD_BYTE,
		[RTE_EVENT_OP_FORWARD] = DLB_NEW_CMD_BYTE,
		[RTE_EVENT_OP_RELEASE] = DLB_NOOP_CMD_BYTE,
	},
};

static inline void
dlb_event_build_hcws(struct dlb_port *qm_port,
		     const struct rte_event ev[],
		     int num,
		     uint8_t *sched_type,
		     uint8_t *queue_id)
{
	struct dlb_enqueue_qe *qe;
	uint16_t sched_word[4];
	__m128i sse_qe[2];
	int i;

	qe = qm_port->qe4;

	sse_qe[0] = _mm_setzero_si128();
	sse_qe[1] = _mm_setzero_si128();

	switch (num) {
	case 4:
		/* Construct the metadata portion of two HCWs in one 128b SSE
		 * register. HCW metadata is constructed in the SSE registers
		 * like so:
		 * sse_qe[0][63:0]:   qe[0]'s metadata
		 * sse_qe[0][127:64]: qe[1]'s metadata
		 * sse_qe[1][63:0]:   qe[2]'s metadata
		 * sse_qe[1][127:64]: qe[3]'s metadata
		 */

		/* Convert the event operation into a command byte and store it
		 * in the metadata:
		 * sse_qe[0][63:56]   = cmd_byte_map[is_directed][ev[0].op]
		 * sse_qe[0][127:120] = cmd_byte_map[is_directed][ev[1].op]
		 * sse_qe[1][63:56]   = cmd_byte_map[is_directed][ev[2].op]
		 * sse_qe[1][127:120] = cmd_byte_map[is_directed][ev[3].op]
		 */
#define DLB_QE_CMD_BYTE 7
		sse_qe[0] = _mm_insert_epi8(sse_qe[0],
				cmd_byte_map[qm_port->is_directed][ev[0].op],
				DLB_QE_CMD_BYTE);
		sse_qe[0] = _mm_insert_epi8(sse_qe[0],
				cmd_byte_map[qm_port->is_directed][ev[1].op],
				DLB_QE_CMD_BYTE + 8);
		sse_qe[1] = _mm_insert_epi8(sse_qe[1],
				cmd_byte_map[qm_port->is_directed][ev[2].op],
				DLB_QE_CMD_BYTE);
		sse_qe[1] = _mm_insert_epi8(sse_qe[1],
				cmd_byte_map[qm_port->is_directed][ev[3].op],
				DLB_QE_CMD_BYTE + 8);

		/* Store priority, scheduling type, and queue ID in the sched
		 * word array because these values are re-used when the
		 * destination is a directed queue.
		 */
		sched_word[0] = EV_TO_DLB_PRIO(ev[0].priority) << 10 |
				sched_type[0] << 8 |
				queue_id[0];
		sched_word[1] = EV_TO_DLB_PRIO(ev[1].priority) << 10 |
				sched_type[1] << 8 |
				queue_id[1];
		sched_word[2] = EV_TO_DLB_PRIO(ev[2].priority) << 10 |
				sched_type[2] << 8 |
				queue_id[2];
		sched_word[3] = EV_TO_DLB_PRIO(ev[3].priority) << 10 |
				sched_type[3] << 8 |
				queue_id[3];

		/* Store the event priority, scheduling type, and queue ID in
		 * the metadata:
		 * sse_qe[0][31:16] = sched_word[0]
		 * sse_qe[0][95:80] = sched_word[1]
		 * sse_qe[1][31:16] = sched_word[2]
		 * sse_qe[1][95:80] = sched_word[3]
		 */
#define DLB_QE_QID_SCHED_WORD 1
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
					     sched_word[0],
					     DLB_QE_QID_SCHED_WORD);
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
					     sched_word[1],
					     DLB_QE_QID_SCHED_WORD + 4);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
					     sched_word[2],
					     DLB_QE_QID_SCHED_WORD);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
					     sched_word[3],
					     DLB_QE_QID_SCHED_WORD + 4);

		/* If the destination is a load-balanced queue, store the lock
		 * ID. If it is a directed queue, DLB places this field in
		 * bytes 10-11 of the received QE, so we format it accordingly:
		 * sse_qe[0][47:32]  = dir queue ? sched_word[0] : flow_id[0]
		 * sse_qe[0][111:96] = dir queue ? sched_word[1] : flow_id[1]
		 * sse_qe[1][47:32]  = dir queue ? sched_word[2] : flow_id[2]
		 * sse_qe[1][111:96] = dir queue ? sched_word[3] : flow_id[3]
		 */
#define DLB_QE_LOCK_ID_WORD 2
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
				(sched_type[0] == DLB_SCHED_DIRECTED) ?
					sched_word[0] : ev[0].flow_id,
				DLB_QE_LOCK_ID_WORD);
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
				(sched_type[1] == DLB_SCHED_DIRECTED) ?
					sched_word[1] : ev[1].flow_id,
				DLB_QE_LOCK_ID_WORD + 4);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
				(sched_type[2] == DLB_SCHED_DIRECTED) ?
					sched_word[2] : ev[2].flow_id,
				DLB_QE_LOCK_ID_WORD);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
				(sched_type[3] == DLB_SCHED_DIRECTED) ?
					sched_word[3] : ev[3].flow_id,
				DLB_QE_LOCK_ID_WORD + 4);

		/* Store the event type and sub event type in the metadata:
		 * sse_qe[0][15:0]  = flow_id[0]
		 * sse_qe[0][79:64] = flow_id[1]
		 * sse_qe[1][15:0]  = flow_id[2]
		 * sse_qe[1][79:64] = flow_id[3]
		 */
#define DLB_QE_EV_TYPE_WORD 0
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
					     ev[0].sub_event_type << 8 |
						ev[0].event_type,
					     DLB_QE_EV_TYPE_WORD);
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
					     ev[1].sub_event_type << 8 |
						ev[1].event_type,
					     DLB_QE_EV_TYPE_WORD + 4);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
					     ev[2].sub_event_type << 8 |
						ev[2].event_type,
					     DLB_QE_EV_TYPE_WORD);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
					     ev[3].sub_event_type << 8 |
						ev[3].event_type,
					     DLB_QE_EV_TYPE_WORD + 4);

		/* Store the metadata to memory (use the double-precision
		 * _mm_storeh_pd because there is no integer function for
		 * storing the upper 64b):
		 * qe[0] metadata = sse_qe[0][63:0]
		 * qe[1] metadata = sse_qe[0][127:64]
		 * qe[2] metadata = sse_qe[1][63:0]
		 * qe[3] metadata = sse_qe[1][127:64]
		 */
		_mm_storel_epi64((__m128i *)&qe[0].u.opaque_data, sse_qe[0]);
		_mm_storeh_pd((double *)&qe[1].u.opaque_data,
			      (__m128d) sse_qe[0]);
		_mm_storel_epi64((__m128i *)&qe[2].u.opaque_data, sse_qe[1]);
		_mm_storeh_pd((double *)&qe[3].u.opaque_data,
			      (__m128d) sse_qe[1]);

		qe[0].data = ev[0].u64;
		qe[1].data = ev[1].u64;
		qe[2].data = ev[2].u64;
		qe[3].data = ev[3].u64;

		break;
	case 3:
	case 2:
	case 1:
		for (i = 0; i < num; i++) {
			qe[i].cmd_byte =
				cmd_byte_map[qm_port->is_directed][ev[i].op];
			qe[i].sched_type = sched_type[i];
			qe[i].data = ev[i].u64;
			qe[i].qid = queue_id[i];
			qe[i].priority = EV_TO_DLB_PRIO(ev[i].priority);
			qe[i].lock_id = ev[i].flow_id;
			if (sched_type[i] == DLB_SCHED_DIRECTED) {
				struct dlb_msg_info *info =
					(struct dlb_msg_info *)&qe[i].lock_id;

				info->qid = queue_id[i];
				info->sched_type = DLB_SCHED_DIRECTED;
				info->priority = qe[i].priority;
			}
			qe[i].u.event_type.major = ev[i].event_type;
			qe[i].u.event_type.sub = ev[i].sub_event_type;
		}
		break;
	case 0:
		break;
	}
}

static __rte_always_inline void
dlb_pp_write(struct dlb_enqueue_qe *qe4,
	     struct process_local_port_data *port_data)
{
	dlb_movdir64b(port_data->pp_addr, qe4);
}

static inline void
dlb_hw_do_enqueue(struct dlb_port *qm_port,
		  bool do_sfence,
		  struct process_local_port_data *port_data)
{
	DLB_LOG_DBG("dlb: Flushing QE(s) to DLB\n");

	/* Since MOVDIR64B is weakly-ordered, use an SFENCE to ensure that
	 * application writes complete before enqueueing the release HCW.
	 */
	if (do_sfence)
		rte_wmb();

	dlb_pp_write(qm_port->qe4, port_data);
}

static inline uint16_t
__dlb_event_enqueue_burst(void *event_port,
			  const struct rte_event events[],
			  uint16_t num)
{
	struct dlb_eventdev_port *ev_port = event_port;
	struct dlb_port *qm_port = &ev_port->qm_port;
	struct process_local_port_data *port_data;
	int i;

	RTE_ASSERT(ev_port->enq_configured);
	RTE_ASSERT(events != NULL);

	rte_errno = 0;
	i = 0;

	port_data = &dlb_port[qm_port->id][PORT_TYPE(qm_port)];

	while (i < num) {
		uint8_t sched_types[DLB_NUM_QES_PER_CACHE_LINE];
		uint8_t queue_ids[DLB_NUM_QES_PER_CACHE_LINE];
		int pop_offs = 0;
		int j = 0;

		memset(qm_port->qe4,
		       0,
		       DLB_NUM_QES_PER_CACHE_LINE *
		       sizeof(struct dlb_enqueue_qe));

		for (; j < DLB_NUM_QES_PER_CACHE_LINE && (i + j) < num; j++) {
			const struct rte_event *ev = &events[i + j];

			if (dlb_event_enqueue_prep(ev_port, qm_port, ev,
						   port_data, &sched_types[j],
						   &queue_ids[j]))
				break;
		}

		if (j == 0)
			break;

		dlb_event_build_hcws(qm_port, &events[i], j - pop_offs,
				     sched_types, queue_ids);

		dlb_hw_do_enqueue(qm_port, i == 0, port_data);

		/* Don't include the token pop QE in the enqueue count */
		i += j - pop_offs;

		/* Don't interpret j < DLB_NUM_... as out-of-credits if
		 * pop_offs != 0
		 */
		if (j < DLB_NUM_QES_PER_CACHE_LINE && pop_offs == 0)
			break;
	}

	RTE_ASSERT(!((i == 0 && rte_errno != -ENOSPC)));

	return i;
}

static inline uint16_t
dlb_event_enqueue_burst(void *event_port,
			const struct rte_event events[],
			uint16_t num)
{
	return __dlb_event_enqueue_burst(event_port, events, num);
}

static inline uint16_t
dlb_event_enqueue(void *event_port,
		  const struct rte_event events[])
{
	return __dlb_event_enqueue_burst(event_port, events, 1);
}

static uint16_t
dlb_event_enqueue_new_burst(void *event_port,
			    const struct rte_event events[],
			    uint16_t num)
{
	return __dlb_event_enqueue_burst(event_port, events, num);
}

static uint16_t
dlb_event_enqueue_forward_burst(void *event_port,
				const struct rte_event events[],
				uint16_t num)
{
	return __dlb_event_enqueue_burst(event_port, events, num);
}

void
dlb_entry_points_init(struct rte_eventdev *dev)
{
	static struct rte_eventdev_ops dlb_eventdev_entry_ops = {
		.dev_infos_get    = dlb_eventdev_info_get,
		.dev_configure    = dlb_eventdev_configure,
		.dev_start        = dlb_eventdev_start,
		.queue_def_conf   = dlb_eventdev_queue_default_conf_get,
		.port_def_conf    = dlb_eventdev_port_default_conf_get,
		.queue_setup      = dlb_eventdev_queue_setup,
		.port_setup       = dlb_eventdev_port_setup,
		.port_link        = dlb_eventdev_port_link,
		.port_unlink      = dlb_eventdev_port_unlink,
		.port_unlinks_in_progress =
				    dlb_eventdev_port_unlinks_in_progress,
		.dump             = dlb_eventdev_dump,
		.xstats_get       = dlb_eventdev_xstats_get,
		.xstats_get_names = dlb_eventdev_xstats_get_names,
		.xstats_get_by_name = dlb_eventdev_xstats_get_by_name,
		.xstats_reset	    = dlb_eventdev_xstats_reset,
	};

	/* Expose PMD's eventdev interface */
	dev->dev_ops = &dlb_eventdev_entry_ops;

	dev->enqueue = dlb_event_enqueue;
	dev->enqueue_burst = dlb_event_enqueue_burst;
	dev->enqueue_new_burst = dlb_event_enqueue_new_burst;
	dev->enqueue_forward_burst = dlb_event_enqueue_forward_burst;
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
