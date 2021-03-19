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
#include <fcntl.h>
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
#include <rte_power_intrinsics.h>
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

static inline uint16_t
dlb_event_enqueue_delayed(void *event_port,
			  const struct rte_event events[]);

static inline uint16_t
dlb_event_enqueue_burst_delayed(void *event_port,
				const struct rte_event events[],
				uint16_t num);

static inline uint16_t
dlb_event_enqueue_new_burst_delayed(void *event_port,
				    const struct rte_event events[],
				    uint16_t num);

static inline uint16_t
dlb_event_enqueue_forward_burst_delayed(void *event_port,
					const struct rte_event events[],
					uint16_t num);

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

	rte_memzone_free(dlb_port[qm_port->id][PORT_TYPE(qm_port)].mz);
	dlb_port[qm_port->id][PORT_TYPE(qm_port)].mz = NULL;
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

	/* When using the reserved token scheme, token_pop_thresh is
	 * initially 2 * dequeue_depth. Once the tokens are reserved,
	 * the enqueue code re-assigns it to dequeue_depth.
	 */
	qm_port->token_pop_thresh = cq_depth;

	/* When the deferred scheduling vdev arg is selected, use deferred pop
	 * for all single-entry CQs.
	 */
	if (cfg.cq_depth == 1 || (cfg.cq_depth == 2 && use_rsvd_token_scheme)) {
		if (dlb->defer_sched)
			qm_port->token_pop_mode = DEFERRED_POP;
	}

	/* The default enqueue functions do not include delayed-pop support for
	 * performance reasons.
	 */
	if (qm_port->token_pop_mode == DELAYED_POP) {
		dlb->event_dev->enqueue = dlb_event_enqueue_delayed;
		dlb->event_dev->enqueue_burst =
			dlb_event_enqueue_burst_delayed;
		dlb->event_dev->enqueue_new_burst =
			dlb_event_enqueue_new_burst_delayed;
		dlb->event_dev->enqueue_forward_burst =
			dlb_event_enqueue_forward_burst_delayed;
	}

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

	/* Directed ports are auto-pop, by default. */
	qm_port->token_pop_mode = AUTO_POP;
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
	struct dlb_cmd_response response = {0};
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

static inline void
dlb_construct_token_pop_qe(struct dlb_port *qm_port, int idx)
{
	struct dlb_cq_pop_qe *qe = (void *)qm_port->qe4;
	int num = qm_port->owed_tokens;

	if (qm_port->use_rsvd_token_scheme) {
		/* Check if there's a deficit of reserved tokens, and return
		 * early if there are no (unreserved) tokens to consume.
		 */
		if (num <= qm_port->cq_rsvd_token_deficit) {
			qm_port->cq_rsvd_token_deficit -= num;
			qm_port->owed_tokens = 0;
			return;
		}
		num -= qm_port->cq_rsvd_token_deficit;
		qm_port->cq_rsvd_token_deficit = 0;
	}

	qe[idx].cmd_byte = DLB_POP_CMD_BYTE;
	qe[idx].tokens = num - 1;
	qm_port->owed_tokens = 0;
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

static inline int
dlb_consume_qe_immediate(struct dlb_port *qm_port, int num)
{
	struct process_local_port_data *port_data;
	struct dlb_cq_pop_qe *qe;

	RTE_ASSERT(qm_port->config_state == DLB_CONFIGURED);

	if (qm_port->use_rsvd_token_scheme) {
		/* Check if there's a deficit of reserved tokens, and return
		 * early if there are no (unreserved) tokens to consume.
		 */
		if (num <= qm_port->cq_rsvd_token_deficit) {
			qm_port->cq_rsvd_token_deficit -= num;
			qm_port->owed_tokens = 0;
			return 0;
		}
		num -= qm_port->cq_rsvd_token_deficit;
		qm_port->cq_rsvd_token_deficit = 0;
	}

	qe = qm_port->consume_qe;

	qe->tokens = num - 1;
	qe->int_arm = 0;

	/* No store fence needed since no pointer is being sent, and CQ token
	 * pops can be safely reordered with other HCWs.
	 */
	port_data = &dlb_port[qm_port->id][PORT_TYPE(qm_port)];

	dlb_movntdq_single(port_data->pp_addr, qe);

	DLB_LOG_DBG("dlb: consume immediate - %d QEs\n", num);

	qm_port->owed_tokens = 0;

	return 0;
}

static inline uint16_t
__dlb_event_enqueue_burst(void *event_port,
			  const struct rte_event events[],
			  uint16_t num,
			  bool use_delayed)
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
			int16_t thresh = qm_port->token_pop_thresh;

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
				dlb_construct_token_pop_qe(qm_port, j);

				/* Reset the releases for the next QE batch */
				qm_port->issued_releases -= thresh;

				/* When using delayed token pop mode, the
				 * initial token threshold is the full CQ
				 * depth. After the first token pop, we need to
				 * reset it to the dequeue_depth.
				 */
				qm_port->token_pop_thresh =
					qm_port->dequeue_depth;

				pop_offs = 1;
				j++;
				break;
			}

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
	return __dlb_event_enqueue_burst(event_port, events, num, false);
}

static inline uint16_t
dlb_event_enqueue_burst_delayed(void *event_port,
				const struct rte_event events[],
				uint16_t num)
{
	return __dlb_event_enqueue_burst(event_port, events, num, true);
}

static inline uint16_t
dlb_event_enqueue(void *event_port,
		  const struct rte_event events[])
{
	return __dlb_event_enqueue_burst(event_port, events, 1, false);
}

static inline uint16_t
dlb_event_enqueue_delayed(void *event_port,
			  const struct rte_event events[])
{
	return __dlb_event_enqueue_burst(event_port, events, 1, true);
}

static uint16_t
dlb_event_enqueue_new_burst(void *event_port,
			    const struct rte_event events[],
			    uint16_t num)
{
	return __dlb_event_enqueue_burst(event_port, events, num, false);
}

static uint16_t
dlb_event_enqueue_new_burst_delayed(void *event_port,
				    const struct rte_event events[],
				    uint16_t num)
{
	return __dlb_event_enqueue_burst(event_port, events, num, true);
}

static uint16_t
dlb_event_enqueue_forward_burst(void *event_port,
				const struct rte_event events[],
				uint16_t num)
{
	return __dlb_event_enqueue_burst(event_port, events, num, false);
}

static uint16_t
dlb_event_enqueue_forward_burst_delayed(void *event_port,
					const struct rte_event events[],
					uint16_t num)
{
	return __dlb_event_enqueue_burst(event_port, events, num, true);
}

static __rte_always_inline int
dlb_recv_qe(struct dlb_port *qm_port, struct dlb_dequeue_qe *qe,
	    uint8_t *offset)
{
	uint8_t xor_mask[2][4] = { {0x0F, 0x0E, 0x0C, 0x08},
				   {0x00, 0x01, 0x03, 0x07} };
	uint8_t and_mask[4] = {0x0F, 0x0E, 0x0C, 0x08};
	volatile struct dlb_dequeue_qe *cq_addr;
	__m128i *qes = (__m128i *)qe;
	uint64_t *cache_line_base;
	uint8_t gen_bits;

	cq_addr = dlb_port[qm_port->id][PORT_TYPE(qm_port)].cq_base;
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

	return __builtin_popcount(gen_bits);
}

static inline void
dlb_inc_cq_idx(struct dlb_port *qm_port, int cnt)
{
	uint16_t idx = qm_port->cq_idx_unmasked + cnt;

	qm_port->cq_idx_unmasked = idx;
	qm_port->cq_idx = idx & qm_port->cq_depth_mask;
	qm_port->gen_bit = (~(idx >> qm_port->gen_bit_shift)) & 0x1;
}

static inline int
dlb_process_dequeue_qes(struct dlb_eventdev_port *ev_port,
			struct dlb_port *qm_port,
			struct rte_event *events,
			struct dlb_dequeue_qe *qes,
			int cnt)
{
	uint8_t *qid_mappings = qm_port->qid_mappings;
	int i, num;

	RTE_SET_USED(ev_port);  /* avoids unused variable error */

	for (i = 0, num = 0; i < cnt; i++) {
		struct dlb_dequeue_qe *qe = &qes[i];
		int sched_type_map[4] = {
			[DLB_SCHED_ATOMIC] = RTE_SCHED_TYPE_ATOMIC,
			[DLB_SCHED_UNORDERED] = RTE_SCHED_TYPE_PARALLEL,
			[DLB_SCHED_ORDERED] = RTE_SCHED_TYPE_ORDERED,
			[DLB_SCHED_DIRECTED] = RTE_SCHED_TYPE_ATOMIC,
		};

		DLB_LOG_DBG("dequeue success, data = 0x%llx, qid=%d, event_type=%d, subevent=%d\npp_id = %d, sched_type = %d, qid = %d, err=%d\n",
			    (long long)qe->data, qe->qid,
			    qe->u.event_type.major,
			    qe->u.event_type.sub,
			    qe->pp_id, qe->sched_type, qe->qid, qe->error);

		/* Fill in event information.
		 * Note that flow_id must be embedded in the data by
		 * the app, such as the mbuf RSS hash field if the data
		 * buffer is a mbuf.
		 */
		if (unlikely(qe->error)) {
			DLB_LOG_ERR("QE error bit ON\n");
			DLB_INC_STAT(ev_port->stats.traffic.rx_drop, 1);
			dlb_consume_qe_immediate(qm_port, 1);
			continue; /* Ignore */
		}

		events[num].u64 = qe->data;
		events[num].queue_id = qid_mappings[qe->qid];
		events[num].priority = DLB_TO_EV_PRIO((uint8_t)qe->priority);
		events[num].event_type = qe->u.event_type.major;
		events[num].sub_event_type = qe->u.event_type.sub;
		events[num].sched_type = sched_type_map[qe->sched_type];
		DLB_INC_STAT(ev_port->stats.rx_sched_cnt[qe->sched_type], 1);
		num++;
	}
	DLB_INC_STAT(ev_port->stats.traffic.rx_ok, num);

	return num;
}

static inline int
dlb_process_dequeue_four_qes(struct dlb_eventdev_port *ev_port,
			     struct dlb_port *qm_port,
			     struct rte_event *events,
			     struct dlb_dequeue_qe *qes)
{
	int sched_type_map[] = {
		[DLB_SCHED_ATOMIC] = RTE_SCHED_TYPE_ATOMIC,
		[DLB_SCHED_UNORDERED] = RTE_SCHED_TYPE_PARALLEL,
		[DLB_SCHED_ORDERED] = RTE_SCHED_TYPE_ORDERED,
		[DLB_SCHED_DIRECTED] = RTE_SCHED_TYPE_ATOMIC,
	};
	const int num_events = DLB_NUM_QES_PER_CACHE_LINE;
	uint8_t *qid_mappings = qm_port->qid_mappings;
	__m128i sse_evt[2];
	int i;

	/* In the unlikely case that any of the QE error bits are set, process
	 * them one at a time.
	 */
	if (unlikely(qes[0].error || qes[1].error ||
		     qes[2].error || qes[3].error))
		return dlb_process_dequeue_qes(ev_port, qm_port, events,
					       qes, num_events);

	for (i = 0; i < DLB_NUM_QES_PER_CACHE_LINE; i++) {
		DLB_LOG_DBG("dequeue success, data = 0x%llx, qid=%d, event_type=%d, subevent=%d\npp_id = %d, sched_type = %d, qid = %d, err=%d\n",
			    (long long)qes[i].data, qes[i].qid,
			    qes[i].u.event_type.major,
			    qes[i].u.event_type.sub,
			    qes[i].pp_id, qes[i].sched_type, qes[i].qid,
			    qes[i].error);
	}

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
	 * the metadata:
	 * sse_evt[0][55:48]   = DLB_TO_EV_PRIO(qes[0].priority)
	 * sse_evt[0][119:112] = DLB_TO_EV_PRIO(qes[1].priority)
	 * sse_evt[1][55:48]   = DLB_TO_EV_PRIO(qes[2].priority)
	 * sse_evt[1][119:112] = DLB_TO_EV_PRIO(qes[3].priority)
	 */
#define DLB_EVENT_PRIO_BYTE 6
	sse_evt[0] = _mm_insert_epi8(sse_evt[0],
				     DLB_TO_EV_PRIO((uint8_t)qes[0].priority),
				     DLB_EVENT_PRIO_BYTE);
	sse_evt[0] = _mm_insert_epi8(sse_evt[0],
				     DLB_TO_EV_PRIO((uint8_t)qes[1].priority),
				     DLB_EVENT_PRIO_BYTE + 8);
	sse_evt[1] = _mm_insert_epi8(sse_evt[1],
				     DLB_TO_EV_PRIO((uint8_t)qes[2].priority),
				     DLB_EVENT_PRIO_BYTE);
	sse_evt[1] = _mm_insert_epi8(sse_evt[1],
				     DLB_TO_EV_PRIO((uint8_t)qes[3].priority),
				     DLB_EVENT_PRIO_BYTE + 8);

	/* Write the event type and sub event type to the event metadata. Leave
	 * flow ID unspecified, since the hardware does not maintain it during
	 * scheduling:
	 * sse_evt[0][31:0]   = qes[0].u.event_type.major << 28 |
	 *			qes[0].u.event_type.sub << 20;
	 * sse_evt[0][95:64]  = qes[1].u.event_type.major << 28 |
	 *			qes[1].u.event_type.sub << 20;
	 * sse_evt[1][31:0]   = qes[2].u.event_type.major << 28 |
	 *			qes[2].u.event_type.sub << 20;
	 * sse_evt[1][95:64]  = qes[3].u.event_type.major << 28 |
	 *			qes[3].u.event_type.sub << 20;
	 */
#define DLB_EVENT_EV_TYPE_DW 0
#define DLB_EVENT_EV_TYPE_SHIFT 28
#define DLB_EVENT_SUB_EV_TYPE_SHIFT 20
	sse_evt[0] = _mm_insert_epi32(sse_evt[0],
			qes[0].u.event_type.major << DLB_EVENT_EV_TYPE_SHIFT |
			qes[0].u.event_type.sub << DLB_EVENT_SUB_EV_TYPE_SHIFT,
			DLB_EVENT_EV_TYPE_DW);
	sse_evt[0] = _mm_insert_epi32(sse_evt[0],
			qes[1].u.event_type.major << DLB_EVENT_EV_TYPE_SHIFT |
			qes[1].u.event_type.sub <<  DLB_EVENT_SUB_EV_TYPE_SHIFT,
			DLB_EVENT_EV_TYPE_DW + 2);
	sse_evt[1] = _mm_insert_epi32(sse_evt[1],
			qes[2].u.event_type.major << DLB_EVENT_EV_TYPE_SHIFT |
			qes[2].u.event_type.sub <<  DLB_EVENT_SUB_EV_TYPE_SHIFT,
			DLB_EVENT_EV_TYPE_DW);
	sse_evt[1] = _mm_insert_epi32(sse_evt[1],
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

	DLB_INC_STAT(ev_port->stats.rx_sched_cnt[qes[0].sched_type], 1);
	DLB_INC_STAT(ev_port->stats.rx_sched_cnt[qes[1].sched_type], 1);
	DLB_INC_STAT(ev_port->stats.rx_sched_cnt[qes[2].sched_type], 1);
	DLB_INC_STAT(ev_port->stats.rx_sched_cnt[qes[3].sched_type], 1);

	DLB_INC_STAT(ev_port->stats.traffic.rx_ok, num_events);

	return num_events;
}

static inline int
dlb_dequeue_wait(struct dlb_eventdev *dlb,
		 struct dlb_eventdev_port *ev_port,
		 struct dlb_port *qm_port,
		 uint64_t timeout,
		 uint64_t start_ticks)
{
	struct process_local_port_data *port_data;
	uint64_t elapsed_ticks;

	port_data = &dlb_port[qm_port->id][PORT_TYPE(qm_port)];

	elapsed_ticks = rte_get_timer_cycles() - start_ticks;

	/* Wait/poll time expired */
	if (elapsed_ticks >= timeout) {
		/* Interrupts not supported by PF PMD */
		return 1;
	} else if (dlb->umwait_allowed) {
		volatile struct dlb_dequeue_qe *cq_base;
		union {
			uint64_t raw_qe[2];
			struct dlb_dequeue_qe qe;
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

		rte_power_monitor(monitor_addr, expected_value,
				  qe_mask.raw_qe[1], timeout + start_ticks,
				  sizeof(uint64_t));

		DLB_INC_STAT(ev_port->stats.traffic.rx_umonitor_umwait, 1);
	} else {
		uint64_t poll_interval = RTE_LIBRTE_PMD_DLB_POLL_INTERVAL;
		uint64_t curr_ticks = rte_get_timer_cycles();
		uint64_t init_ticks = curr_ticks;

		while ((curr_ticks - start_ticks < timeout) &&
		       (curr_ticks - init_ticks < poll_interval))
			curr_ticks = rte_get_timer_cycles();
	}

	return 0;
}

static inline int16_t
dlb_hw_dequeue(struct dlb_eventdev *dlb,
	       struct dlb_eventdev_port *ev_port,
	       struct rte_event *events,
	       uint16_t max_num,
	       uint64_t dequeue_timeout_ticks)
{
	uint64_t timeout;
	uint64_t start_ticks = 0ULL;
	struct dlb_port *qm_port;
	int num = 0;

	qm_port = &ev_port->qm_port;

	/* If configured for per dequeue wait, then use wait value provided
	 * to this API. Otherwise we must use the global
	 * value from eventdev config time.
	 */
	if (!dlb->global_dequeue_wait)
		timeout = dequeue_timeout_ticks;
	else
		timeout = dlb->global_dequeue_wait_ticks;

	if (timeout)
		start_ticks = rte_get_timer_cycles();

	while (num < max_num) {
		struct dlb_dequeue_qe qes[DLB_NUM_QES_PER_CACHE_LINE];
		uint8_t offset;
		int num_avail;

		/* Copy up to 4 QEs from the current cache line into qes */
		num_avail = dlb_recv_qe(qm_port, qes, &offset);

		/* But don't process more than the user requested */
		num_avail = RTE_MIN(num_avail, max_num - num);

		dlb_inc_cq_idx(qm_port, num_avail);

		if (num_avail == DLB_NUM_QES_PER_CACHE_LINE)
			num += dlb_process_dequeue_four_qes(ev_port,
							     qm_port,
							     &events[num],
							     &qes[offset]);
		else if (num_avail)
			num += dlb_process_dequeue_qes(ev_port,
							qm_port,
							&events[num],
							&qes[offset],
							num_avail);
		else if ((timeout == 0) || (num > 0))
			/* Not waiting in any form, or 1+ events received? */
			break;
		else if (dlb_dequeue_wait(dlb, ev_port, qm_port,
					  timeout, start_ticks))
			break;
	}

	qm_port->owed_tokens += num;

	if (num && qm_port->token_pop_mode == AUTO_POP)
		dlb_consume_qe_immediate(qm_port, num);

	ev_port->outstanding_releases += num;

	return num;
}

static __rte_always_inline int
dlb_recv_qe_sparse(struct dlb_port *qm_port, struct dlb_dequeue_qe *qe)
{
	volatile struct dlb_dequeue_qe *cq_addr;
	uint8_t xor_mask[2] = {0x0F, 0x00};
	const uint8_t and_mask = 0x0F;
	__m128i *qes = (__m128i *)qe;
	uint8_t gen_bits, gen_bit;
	uintptr_t addr[4];
	uint16_t idx;

	cq_addr = dlb_port[qm_port->id][PORT_TYPE(qm_port)].cq_base;

	idx = qm_port->cq_idx;

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

	return __builtin_popcount(gen_bits);
}

static inline int16_t
dlb_hw_dequeue_sparse(struct dlb_eventdev *dlb,
		      struct dlb_eventdev_port *ev_port,
		      struct rte_event *events,
		      uint16_t max_num,
		      uint64_t dequeue_timeout_ticks)
{
	uint64_t timeout;
	uint64_t start_ticks = 0ULL;
	struct dlb_port *qm_port;
	int num = 0;

	qm_port = &ev_port->qm_port;

	/* If configured for per dequeue wait, then use wait value provided
	 * to this API. Otherwise we must use the global
	 * value from eventdev config time.
	 */
	if (!dlb->global_dequeue_wait)
		timeout = dequeue_timeout_ticks;
	else
		timeout = dlb->global_dequeue_wait_ticks;

	if (timeout)
		start_ticks = rte_get_timer_cycles();

	while (num < max_num) {
		struct dlb_dequeue_qe qes[DLB_NUM_QES_PER_CACHE_LINE];
		int num_avail;

		/* Copy up to 4 QEs from the current cache line into qes */
		num_avail = dlb_recv_qe_sparse(qm_port, qes);

		/* But don't process more than the user requested */
		num_avail = RTE_MIN(num_avail, max_num - num);

		dlb_inc_cq_idx(qm_port, num_avail << 2);

		if (num_avail == DLB_NUM_QES_PER_CACHE_LINE)
			num += dlb_process_dequeue_four_qes(ev_port,
							     qm_port,
							     &events[num],
							     &qes[0]);
		else if (num_avail)
			num += dlb_process_dequeue_qes(ev_port,
							qm_port,
							&events[num],
							&qes[0],
							num_avail);
		else if ((timeout == 0) || (num > 0))
			/* Not waiting in any form, or 1+ events received? */
			break;
		else if (dlb_dequeue_wait(dlb, ev_port, qm_port,
					  timeout, start_ticks))
			break;
	}

	qm_port->owed_tokens += num;

	if (num && qm_port->token_pop_mode == AUTO_POP)
		dlb_consume_qe_immediate(qm_port, num);

	ev_port->outstanding_releases += num;

	return num;
}

static int
dlb_event_release(struct dlb_eventdev *dlb, uint8_t port_id, int n)
{
	struct process_local_port_data *port_data;
	struct dlb_eventdev_port *ev_port;
	struct dlb_port *qm_port;
	int i;

	if (port_id > dlb->num_ports) {
		DLB_LOG_ERR("Invalid port id %d in dlb-event_release\n",
			    port_id);
		rte_errno = -EINVAL;
		return rte_errno;
	}

	ev_port = &dlb->ev_ports[port_id];
	qm_port = &ev_port->qm_port;
	port_data = &dlb_port[qm_port->id][PORT_TYPE(qm_port)];

	i = 0;

	if (qm_port->is_directed) {
		i = n;
		goto sw_credit_update;
	}

	while (i < n) {
		int pop_offs = 0;
		int j = 0;

		/* Zero-out QEs */
		qm_port->qe4[0].cmd_byte = 0;
		qm_port->qe4[1].cmd_byte = 0;
		qm_port->qe4[2].cmd_byte = 0;
		qm_port->qe4[3].cmd_byte = 0;

		for (; j < DLB_NUM_QES_PER_CACHE_LINE && (i + j) < n; j++) {
			int16_t thresh = qm_port->token_pop_thresh;

			if (qm_port->token_pop_mode == DELAYED_POP &&
			    qm_port->issued_releases >= thresh - 1) {
				/* Insert the token pop QE */
				dlb_construct_token_pop_qe(qm_port, j);

				/* Reset the releases for the next QE batch */
				qm_port->issued_releases -= thresh;

				/* When using delayed token pop mode, the
				 * initial token threshold is the full CQ
				 * depth. After the first token pop, we need to
				 * reset it to the dequeue_depth.
				 */
				qm_port->token_pop_thresh =
					qm_port->dequeue_depth;

				pop_offs = 1;
				j++;
				break;
			}

			qm_port->qe4[j].cmd_byte = DLB_COMP_CMD_BYTE;
			qm_port->issued_releases++;
		}

		dlb_hw_do_enqueue(qm_port, i == 0, port_data);

		/* Don't include the token pop QE in the release count */
		i += j - pop_offs;
	}

sw_credit_update:
	/* each release returns one credit */
	if (!ev_port->outstanding_releases) {
		DLB_LOG_ERR("Unrecoverable application error. Outstanding releases underflowed.\n");
		rte_errno = -ENOTRECOVERABLE;
		return rte_errno;
	}

	ev_port->outstanding_releases -= i;
	ev_port->inflight_credits += i;

	/* Replenish s/w credits if enough releases are performed */
	dlb_replenish_sw_credits(dlb, ev_port);
	return 0;
}

static uint16_t
dlb_event_dequeue_burst(void *event_port, struct rte_event *ev, uint16_t num,
			uint64_t wait)
{
	struct dlb_eventdev_port *ev_port = event_port;
	struct dlb_port *qm_port = &ev_port->qm_port;
	struct dlb_eventdev *dlb = ev_port->dlb;
	uint16_t cnt;
	int ret;

	rte_errno = 0;

	RTE_ASSERT(ev_port->setup_done);
	RTE_ASSERT(ev != NULL);

	if (ev_port->implicit_release && ev_port->outstanding_releases > 0) {
		uint16_t out_rels = ev_port->outstanding_releases;

		ret = dlb_event_release(dlb, ev_port->id, out_rels);
		if (ret)
			return(ret);

		DLB_INC_STAT(ev_port->stats.tx_implicit_rel, out_rels);
	}

	if (qm_port->token_pop_mode == DEFERRED_POP &&
			qm_port->owed_tokens)
		dlb_consume_qe_immediate(qm_port, qm_port->owed_tokens);

	cnt = dlb_hw_dequeue(dlb, ev_port, ev, num, wait);

	DLB_INC_STAT(ev_port->stats.traffic.total_polls, 1);
	DLB_INC_STAT(ev_port->stats.traffic.zero_polls, ((cnt == 0) ? 1 : 0));
	return cnt;
}

static uint16_t
dlb_event_dequeue(void *event_port, struct rte_event *ev, uint64_t wait)
{
	return dlb_event_dequeue_burst(event_port, ev, 1, wait);
}

static uint16_t
dlb_event_dequeue_burst_sparse(void *event_port, struct rte_event *ev,
			       uint16_t num, uint64_t wait)
{
	struct dlb_eventdev_port *ev_port = event_port;
	struct dlb_port *qm_port = &ev_port->qm_port;
	struct dlb_eventdev *dlb = ev_port->dlb;
	uint16_t cnt;
	int ret;

	rte_errno = 0;

	RTE_ASSERT(ev_port->setup_done);
	RTE_ASSERT(ev != NULL);

	if (ev_port->implicit_release && ev_port->outstanding_releases > 0) {
		uint16_t out_rels = ev_port->outstanding_releases;

		ret = dlb_event_release(dlb, ev_port->id, out_rels);
		if (ret)
			return(ret);

		DLB_INC_STAT(ev_port->stats.tx_implicit_rel, out_rels);
	}

	if (qm_port->token_pop_mode == DEFERRED_POP &&
	    qm_port->owed_tokens)
		dlb_consume_qe_immediate(qm_port, qm_port->owed_tokens);

	cnt = dlb_hw_dequeue_sparse(dlb, ev_port, ev, num, wait);

	DLB_INC_STAT(ev_port->stats.traffic.total_polls, 1);
	DLB_INC_STAT(ev_port->stats.traffic.zero_polls, ((cnt == 0) ? 1 : 0));
	return cnt;
}

static uint16_t
dlb_event_dequeue_sparse(void *event_port, struct rte_event *ev, uint64_t wait)
{
	return dlb_event_dequeue_burst_sparse(event_port, ev, 1, wait);
}

static uint32_t
dlb_get_ldb_queue_depth(struct dlb_eventdev *dlb,
			struct dlb_eventdev_queue *queue)
{
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_get_ldb_queue_depth_args cfg;
	struct dlb_cmd_response response = {0};
	int ret;

	cfg.queue_id = queue->qm_queue.id;
	cfg.response = (uintptr_t)&response;

	ret = dlb_iface_get_ldb_queue_depth(handle, &cfg);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: get_ldb_queue_depth ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		return ret;
	}

	return response.id;
}

static uint32_t
dlb_get_dir_queue_depth(struct dlb_eventdev *dlb,
			struct dlb_eventdev_queue *queue)
{
	struct dlb_hw_dev *handle = &dlb->qm_instance;
	struct dlb_get_dir_queue_depth_args cfg;
	struct dlb_cmd_response response = {0};
	int ret;

	cfg.queue_id = queue->qm_queue.id;
	cfg.response = (uintptr_t)&response;

	ret = dlb_iface_get_dir_queue_depth(handle, &cfg);
	if (ret < 0) {
		DLB_LOG_ERR("dlb: get_dir_queue_depth ret=%d (driver status: %s)\n",
			    ret, dlb_error_strings[response.status]);
		return ret;
	}

	return response.id;
}

uint32_t
dlb_get_queue_depth(struct dlb_eventdev *dlb,
		    struct dlb_eventdev_queue *queue)
{
	if (queue->qm_queue.is_directed)
		return dlb_get_dir_queue_depth(dlb, queue);
	else
		return dlb_get_ldb_queue_depth(dlb, queue);
}

static bool
dlb_queue_is_empty(struct dlb_eventdev *dlb,
		   struct dlb_eventdev_queue *queue)
{
	return dlb_get_queue_depth(dlb, queue) == 0;
}

static bool
dlb_linked_queues_empty(struct dlb_eventdev *dlb)
{
	int i;

	for (i = 0; i < dlb->num_queues; i++) {
		if (dlb->ev_queues[i].num_links == 0)
			continue;
		if (!dlb_queue_is_empty(dlb, &dlb->ev_queues[i]))
			return false;
	}

	return true;
}

static bool
dlb_queues_empty(struct dlb_eventdev *dlb)
{
	int i;

	for (i = 0; i < dlb->num_queues; i++) {
		if (!dlb_queue_is_empty(dlb, &dlb->ev_queues[i]))
			return false;
	}

	return true;
}

static void
dlb_flush_port(struct rte_eventdev *dev, int port_id)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);
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

		if (dlb->ev_ports[port_id].qm_port.is_directed)
			continue;

		ev.op = RTE_EVENT_OP_RELEASE;

		rte_event_enqueue_burst(dev_id, port_id, &ev, 1);
	}

	/* Enqueue any additional outstanding releases */
	ev.op = RTE_EVENT_OP_RELEASE;

	for (i = dlb->ev_ports[port_id].outstanding_releases; i > 0; i--)
		rte_event_enqueue_burst(dev_id, port_id, &ev, 1);
}

static void
dlb_drain(struct rte_eventdev *dev)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);
	struct dlb_eventdev_port *ev_port = NULL;
	uint8_t dev_id;
	int i;

	dev_id = dev->data->dev_id;

	while (!dlb_linked_queues_empty(dlb)) {
		/* Flush all the ev_ports, which will drain all their connected
		 * queues.
		 */
		for (i = 0; i < dlb->num_ports; i++)
			dlb_flush_port(dev, i);
	}

	/* The queues are empty, but there may be events left in the ports. */
	for (i = 0; i < dlb->num_ports; i++)
		dlb_flush_port(dev, i);

	/* If the domain's queues are empty, we're done. */
	if (dlb_queues_empty(dlb))
		return;

	/* Else, there must be at least one unlinked load-balanced queue.
	 * Select a load-balanced port with which to drain the unlinked
	 * queue(s).
	 */
	for (i = 0; i < dlb->num_ports; i++) {
		ev_port = &dlb->ev_ports[i];

		if (!ev_port->qm_port.is_directed)
			break;
	}

	if (i == dlb->num_ports) {
		DLB_LOG_ERR("internal error: no LDB ev_ports\n");
		return;
	}

	rte_errno = 0;
	rte_event_port_unlink(dev_id, ev_port->id, NULL, 0);

	if (rte_errno) {
		DLB_LOG_ERR("internal error: failed to unlink ev_port %d\n",
			    ev_port->id);
		return;
	}

	for (i = 0; i < dlb->num_queues; i++) {
		uint8_t qid, prio;
		int ret;

		if (dlb_queue_is_empty(dlb, &dlb->ev_queues[i]))
			continue;

		qid = i;
		prio = 0;

		/* Link the ev_port to the queue */
		ret = rte_event_port_link(dev_id, ev_port->id, &qid, &prio, 1);
		if (ret != 1) {
			DLB_LOG_ERR("internal error: failed to link ev_port %d to queue %d\n",
				    ev_port->id, qid);
			return;
		}

		/* Flush the queue */
		while (!dlb_queue_is_empty(dlb, &dlb->ev_queues[i]))
			dlb_flush_port(dev, ev_port->id);

		/* Drain any extant events in the ev_port. */
		dlb_flush_port(dev, ev_port->id);

		/* Unlink the ev_port from the queue */
		ret = rte_event_port_unlink(dev_id, ev_port->id, &qid, 1);
		if (ret != 1) {
			DLB_LOG_ERR("internal error: failed to unlink ev_port %d to queue %d\n",
				    ev_port->id, qid);
			return;
		}
	}
}

static void
dlb_eventdev_stop(struct rte_eventdev *dev)
{
	struct dlb_eventdev *dlb = dlb_pmd_priv(dev);

	rte_spinlock_lock(&dlb->qm_instance.resource_lock);

	if (dlb->run_state == DLB_RUN_STATE_STOPPED) {
		DLB_LOG_DBG("Internal error: already stopped\n");
		rte_spinlock_unlock(&dlb->qm_instance.resource_lock);
		return;
	} else if (dlb->run_state != DLB_RUN_STATE_STARTED) {
		DLB_LOG_ERR("Internal error: bad state %d for dev_stop\n",
			    (int)dlb->run_state);
		rte_spinlock_unlock(&dlb->qm_instance.resource_lock);
		return;
	}

	dlb->run_state = DLB_RUN_STATE_STOPPING;

	rte_spinlock_unlock(&dlb->qm_instance.resource_lock);

	dlb_drain(dev);

	dlb->run_state = DLB_RUN_STATE_STOPPED;
}

static int
dlb_eventdev_close(struct rte_eventdev *dev)
{
	dlb_hw_reset_sched_domain(dev, false);

	return 0;
}

static void
dlb_eventdev_port_release(void *port)
{
	struct dlb_eventdev_port *ev_port = port;

	if (ev_port) {
		struct dlb_port *qm_port = &ev_port->qm_port;

		if (qm_port->config_state == DLB_CONFIGURED)
			dlb_free_qe_mem(qm_port);
	}
}

static void
dlb_eventdev_queue_release(struct rte_eventdev *dev, uint8_t id)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(id);

	/* This function intentionally left blank. */
}

static int
dlb_eventdev_timeout_ticks(struct rte_eventdev *dev, uint64_t ns,
			   uint64_t *timeout_ticks)
{
	RTE_SET_USED(dev);
	uint64_t cycles_per_ns = rte_get_timer_hz() / 1E9;

	*timeout_ticks = ns * cycles_per_ns;

	return 0;
}

void
dlb_entry_points_init(struct rte_eventdev *dev)
{
	struct dlb_eventdev *dlb;

	static struct rte_eventdev_ops dlb_eventdev_entry_ops = {
		.dev_infos_get    = dlb_eventdev_info_get,
		.dev_configure    = dlb_eventdev_configure,
		.dev_start        = dlb_eventdev_start,
		.dev_stop         = dlb_eventdev_stop,
		.dev_close        = dlb_eventdev_close,
		.queue_def_conf   = dlb_eventdev_queue_default_conf_get,
		.port_def_conf    = dlb_eventdev_port_default_conf_get,
		.queue_setup      = dlb_eventdev_queue_setup,
		.queue_release    = dlb_eventdev_queue_release,
		.port_setup       = dlb_eventdev_port_setup,
		.port_release     = dlb_eventdev_port_release,
		.port_link        = dlb_eventdev_port_link,
		.port_unlink      = dlb_eventdev_port_unlink,
		.port_unlinks_in_progress =
				    dlb_eventdev_port_unlinks_in_progress,
		.timeout_ticks    = dlb_eventdev_timeout_ticks,
		.dump             = dlb_eventdev_dump,
		.xstats_get       = dlb_eventdev_xstats_get,
		.xstats_get_names = dlb_eventdev_xstats_get_names,
		.xstats_get_by_name = dlb_eventdev_xstats_get_by_name,
		.xstats_reset	    = dlb_eventdev_xstats_reset,
		.dev_selftest     = test_dlb_eventdev,
	};

	/* Expose PMD's eventdev interface */
	dev->dev_ops = &dlb_eventdev_entry_ops;

	dev->enqueue = dlb_event_enqueue;
	dev->enqueue_burst = dlb_event_enqueue_burst;
	dev->enqueue_new_burst = dlb_event_enqueue_new_burst;
	dev->enqueue_forward_burst = dlb_event_enqueue_forward_burst;
	dev->dequeue = dlb_event_dequeue;
	dev->dequeue_burst = dlb_event_dequeue_burst;

	dlb = dev->data->dev_private;

	if (dlb->poll_mode == DLB_CQ_POLL_MODE_SPARSE) {
		dev->dequeue = dlb_event_dequeue_sparse;
		dev->dequeue_burst = dlb_event_dequeue_burst_sparse;
	}
}

int
dlb_primary_eventdev_probe(struct rte_eventdev *dev,
			   const char *name,
			   struct dlb_devargs *dlb_args)
{
	struct dlb_eventdev *dlb;
	int err, i;

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

	/* Initialize each port's token pop mode */
	for (i = 0; i < DLB_MAX_NUM_PORTS; i++)
		dlb->ev_ports[i].qm_port.token_pop_mode = AUTO_POP;

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
