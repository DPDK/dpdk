/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#include <rte_common.h>
#include <rte_service_component.h>

#include "efx.h"
#include "efx_regs_counters_pkt_format.h"

#include "sfc_ev.h"
#include "sfc.h"
#include "sfc_rx.h"
#include "sfc_mae_counter.h"
#include "sfc_service.h"

/**
 * Approximate maximum number of counters per packet.
 * In fact maximum depends on per-counter data offset which is specified
 * in counter packet header.
 */
#define SFC_MAE_COUNTERS_PER_PACKET_MAX \
	((SFC_MAE_COUNTER_STREAM_PACKET_SIZE - \
	  ER_RX_SL_PACKETISER_HEADER_WORD_SIZE) / \
	  ER_RX_SL_PACKETISER_PAYLOAD_WORD_SIZE)

/**
 * Minimum number of Rx buffers in counters only Rx queue.
 */
#define SFC_MAE_COUNTER_RXQ_BUFS_MIN \
	(SFC_COUNTER_RXQ_RX_DESC_COUNT - SFC_COUNTER_RXQ_REFILL_LEVEL)

/**
 * Approximate number of counter updates fit in counters only Rx queue.
 * The number is inaccurate since SFC_MAE_COUNTERS_PER_PACKET_MAX is
 * inaccurate (see above). However, it provides the gist for a number of
 * counter updates which can fit in an Rx queue after empty poll.
 *
 * The define is not actually used, but provides calculations details.
 */
#define SFC_MAE_COUNTERS_RXQ_SPACE \
	(SFC_MAE_COUNTER_RXQ_BUFS_MIN * SFC_MAE_COUNTERS_PER_PACKET_MAX)

static uint32_t
sfc_mae_counter_get_service_lcore(struct sfc_adapter *sa)
{
	uint32_t cid;

	cid = sfc_get_service_lcore(sa->socket_id);
	if (cid != RTE_MAX_LCORE)
		return cid;

	if (sa->socket_id != SOCKET_ID_ANY)
		cid = sfc_get_service_lcore(SOCKET_ID_ANY);

	if (cid == RTE_MAX_LCORE) {
		sfc_warn(sa, "failed to get service lcore for counter service");
	} else if (sa->socket_id != SOCKET_ID_ANY) {
		sfc_warn(sa,
			"failed to get service lcore for counter service at socket %d, but got at socket %u",
			sa->socket_id, rte_lcore_to_socket_id(cid));
	}
	return cid;
}

bool
sfc_mae_counter_rxq_required(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);

	if (encp->enc_mae_supported == B_FALSE)
		return false;

	return true;
}

int
sfc_mae_counter_fw_rsrc_enable(struct sfc_adapter *sa,
			       struct sfc_mae_counter *counterp)
{
	struct sfc_mae_counter_registry *reg = &sa->mae.counter_registry;
	struct sfc_mae_counter_records *counters;
	struct sfc_mae_counter_record *p;
	efx_counter_t mae_counter;
	uint32_t generation_count;
	uint32_t unused;
	int rc;

	switch (counterp->type) {
	case EFX_COUNTER_TYPE_ACTION:
		counters = &reg->action_counters;
		break;
	case EFX_COUNTER_TYPE_CONNTRACK:
		counters = &reg->conntrack_counters;
		break;
	default:
		rc = EINVAL;
		goto fail_counter_type_check;
	}

	/*
	 * The actual count of counters allocated is ignored since a failure
	 * to allocate a single counter is indicated by non-zero return code.
	 */
	rc = efx_mae_counters_alloc_type(sa->nic, counterp->type, 1, &unused,
					 &mae_counter, &generation_count);
	if (rc != 0) {
		sfc_err(sa, "failed to alloc MAE counter: %s",
			rte_strerror(rc));
		goto fail_mae_counter_alloc;
	}

	if (mae_counter.id >= counters->n_mae_counters) {
		/*
		 * ID of a counter is expected to be within the range
		 * between 0 and the maximum count of counters to always
		 * fit into a pre-allocated array size of maximum counter ID.
		 */
		sfc_err(sa, "MAE counter ID is out of expected range");
		rc = EFAULT;
		goto fail_counter_id_range;
	}

	counterp->fw_rsrc.counter_id.id = mae_counter.id;

	p = &counters->mae_counters[mae_counter.id];

	/*
	 * Ordering is relaxed since it is the only operation on counter value.
	 * And it does not depend on different stores/loads in other threads.
	 * Paired with relaxed ordering in counter increment.
	 */
	__atomic_store(&p->reset.pkts_bytes.int128,
		       &p->value.pkts_bytes.int128, __ATOMIC_RELAXED);
	p->generation_count = generation_count;

	p->ft_switch_hit_counter = counterp->ft_switch_hit_counter;

	/*
	 * The flag is set at the very end of add operation and reset
	 * at the beginning of delete operation. Release ordering is
	 * paired with acquire ordering on load in counter increment operation.
	 */
	__atomic_store_n(&p->inuse, true, __ATOMIC_RELEASE);

	sfc_info(sa, "enabled MAE counter 0x%x-#%u with reset pkts=%" PRIu64
		 " bytes=%" PRIu64, counterp->type, mae_counter.id,
		 p->reset.pkts, p->reset.bytes);

	return 0;

fail_counter_id_range:
	(void)efx_mae_counters_free_type(sa->nic, counterp->type, 1, &unused,
					 &mae_counter, NULL);

fail_mae_counter_alloc:
fail_counter_type_check:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));
	return rc;
}

int
sfc_mae_counter_fw_rsrc_disable(struct sfc_adapter *sa,
				struct sfc_mae_counter *counter)
{
	struct sfc_mae_counter_registry *reg = &sa->mae.counter_registry;
	efx_counter_t *mae_counter = &counter->fw_rsrc.counter_id;
	struct sfc_mae_counter_records *counters;
	struct sfc_mae_counter_record *p;
	uint32_t unused;
	int rc;

	switch (counter->type) {
	case EFX_COUNTER_TYPE_ACTION:
		counters = &reg->action_counters;
		break;
	case EFX_COUNTER_TYPE_CONNTRACK:
		counters = &reg->conntrack_counters;
		break;
	default:
		return EINVAL;
	}

	SFC_ASSERT(mae_counter->id < counters->n_mae_counters);
	/*
	 * The flag is set at the very end of add operation and reset
	 * at the beginning of delete operation. Release ordering is
	 * paired with acquire ordering on load in counter increment operation.
	 */
	p = &counters->mae_counters[mae_counter->id];
	__atomic_store_n(&p->inuse, false, __ATOMIC_RELEASE);

	rc = efx_mae_counters_free_type(sa->nic, counter->type, 1, &unused,
					mae_counter, NULL);
	if (rc != 0)
		sfc_err(sa, "failed to free MAE counter 0x%x-#%u: %s",
			counter->type, mae_counter->id, rte_strerror(rc));

	sfc_info(sa, "disabled MAE counter 0x%x-#%u with reset pkts=%" PRIu64
		 " bytes=%" PRIu64, counter->type, mae_counter->id,
		 p->reset.pkts, p->reset.bytes);

	/*
	 * Do this regardless of what efx_mae_counters_free() return value is.
	 * If there's some error, the resulting resource leakage is bad, but
	 * nothing sensible can be done in this case.
	 */
	mae_counter->id = EFX_MAE_RSRC_ID_INVALID;

	return rc;
}

static void
sfc_mae_counter_increment(struct sfc_adapter *sa,
			  struct sfc_mae_counter_records *counters,
			  uint32_t mae_counter_id,
			  uint32_t generation_count,
			  uint64_t pkts, uint64_t bytes)
{
	struct sfc_mae_counter_record *p =
		&counters->mae_counters[mae_counter_id];
	struct sfc_mae_counters_xstats *xstats = &counters->xstats;
	union sfc_pkts_bytes cnt_val;
	bool inuse;

	/*
	 * Acquire ordering is paired with release ordering in counter add
	 * and delete operations.
	 */
	__atomic_load(&p->inuse, &inuse, __ATOMIC_ACQUIRE);
	if (!inuse) {
		/*
		 * Two possible cases include:
		 * 1) Counter is just allocated. Too early counter update
		 *    cannot be processed properly.
		 * 2) Stale update of freed and not reallocated counter.
		 *    There is no point in processing that update.
		 */
		xstats->not_inuse_update++;
		return;
	}

	if (unlikely(generation_count < p->generation_count)) {
		/*
		 * It is a stale update for the reallocated counter
		 * (i.e., freed and the same ID allocated again).
		 */
		xstats->realloc_update++;
		return;
	}

	cnt_val.pkts = p->value.pkts + pkts;
	cnt_val.bytes = p->value.bytes + bytes;

	/*
	 * Ordering is relaxed since it is the only operation on counter value.
	 * And it does not depend on different stores/loads in other threads.
	 * Paired with relaxed ordering on counter reset.
	 */
	__atomic_store(&p->value.pkts_bytes,
		       &cnt_val.pkts_bytes, __ATOMIC_RELAXED);

	if (p->ft_switch_hit_counter != NULL) {
		uint64_t ft_switch_hit_counter;

		ft_switch_hit_counter = *p->ft_switch_hit_counter + pkts;
		__atomic_store_n(p->ft_switch_hit_counter, ft_switch_hit_counter,
				 __ATOMIC_RELAXED);
	}

	sfc_info(sa, "update MAE counter 0x%x-#%u: pkts+%" PRIu64 "=%" PRIu64
		 ", bytes+%" PRIu64 "=%" PRIu64, counters->type, mae_counter_id,
		 pkts, cnt_val.pkts, bytes, cnt_val.bytes);
}

static void
sfc_mae_parse_counter_packet(struct sfc_adapter *sa,
			     struct sfc_mae_counter_registry *counter_registry,
			     const struct rte_mbuf *m)
{
	struct sfc_mae_counter_records *counters;
	uint32_t generation_count;
	const efx_xword_t *hdr;
	const efx_oword_t *counters_data;
	unsigned int version;
	unsigned int id;
	unsigned int header_offset;
	unsigned int payload_offset;
	unsigned int counter_count;
	unsigned int required_len;
	unsigned int i;

	if (unlikely(m->nb_segs != 1)) {
		sfc_err(sa, "unexpectedly scattered MAE counters packet (%u segments)",
			m->nb_segs);
		return;
	}

	if (unlikely(m->data_len < ER_RX_SL_PACKETISER_HEADER_WORD_SIZE)) {
		sfc_err(sa, "too short MAE counters packet (%u bytes)",
			m->data_len);
		return;
	}

	/*
	 * The generation count is located in the Rx prefix in the USER_MARK
	 * field which is written into hash.fdir.hi field of an mbuf. See
	 * SF-123581-TC SmartNIC Datapath Offloads section 4.7.5 Counters.
	 */
	generation_count = m->hash.fdir.hi;

	hdr = rte_pktmbuf_mtod(m, const efx_xword_t *);

	version = EFX_XWORD_FIELD(*hdr, ERF_SC_PACKETISER_HEADER_VERSION);
	if (unlikely(version != ERF_SC_PACKETISER_HEADER_VERSION_2)) {
		sfc_err(sa, "unexpected MAE counters packet version %u",
			version);
		return;
	}

	id = EFX_XWORD_FIELD(*hdr, ERF_SC_PACKETISER_HEADER_IDENTIFIER);

	switch (id) {
	case ERF_SC_PACKETISER_HEADER_IDENTIFIER_AR:
		counters = &counter_registry->action_counters;
		break;
	case ERF_SC_PACKETISER_HEADER_IDENTIFIER_CT:
		counters = &counter_registry->conntrack_counters;
		break;
	default:
		sfc_err(sa, "unexpected MAE counters source identifier %u", id);
		return;
	}

	/* Packet layout definitions assume fixed header offset in fact */
	header_offset =
		EFX_XWORD_FIELD(*hdr, ERF_SC_PACKETISER_HEADER_HEADER_OFFSET);
	if (unlikely(header_offset !=
		     ERF_SC_PACKETISER_HEADER_HEADER_OFFSET_DEFAULT)) {
		sfc_err(sa, "unexpected MAE counters packet header offset %u",
			header_offset);
		return;
	}

	payload_offset =
		EFX_XWORD_FIELD(*hdr, ERF_SC_PACKETISER_HEADER_PAYLOAD_OFFSET);

	counter_count = EFX_XWORD_FIELD(*hdr, ERF_SC_PACKETISER_HEADER_COUNT);

	required_len = payload_offset +
			counter_count * sizeof(counters_data[0]);
	if (unlikely(required_len > m->data_len)) {
		sfc_err(sa, "truncated MAE counters packet: %u counters, packet length is %u vs %u required",
			counter_count, m->data_len, required_len);
		/*
		 * In theory it is possible process available counters data,
		 * but such condition is really unexpected and it is
		 * better to treat entire packet as corrupted.
		 */
		return;
	}

	/* Ensure that counters data is 32-bit aligned */
	if (unlikely(payload_offset % sizeof(uint32_t) != 0)) {
		sfc_err(sa, "unsupported MAE counters payload offset %u, must be 32-bit aligned",
			payload_offset);
		return;
	}
	RTE_BUILD_BUG_ON(sizeof(counters_data[0]) !=
			ER_RX_SL_PACKETISER_PAYLOAD_WORD_SIZE);

	counters_data =
		rte_pktmbuf_mtod_offset(m, const efx_oword_t *, payload_offset);

	sfc_info(sa, "update %u MAE counters with gc=%u",
		 counter_count, generation_count);

	for (i = 0; i < counter_count; ++i) {
		uint32_t packet_count_lo;
		uint32_t packet_count_hi;
		uint32_t byte_count_lo;
		uint32_t byte_count_hi;

		/*
		 * Use 32-bit field accessors below since counters data
		 * is not 64-bit aligned.
		 * 32-bit alignment is checked above taking into account
		 * that start of packet data is 32-bit aligned
		 * (cache-line size aligned in fact).
		 */
		packet_count_lo =
			EFX_OWORD_FIELD32(counters_data[i],
				ERF_SC_PACKETISER_PAYLOAD_PACKET_COUNT_LO);
		packet_count_hi =
			EFX_OWORD_FIELD32(counters_data[i],
				ERF_SC_PACKETISER_PAYLOAD_PACKET_COUNT_HI);
		byte_count_lo =
			EFX_OWORD_FIELD32(counters_data[i],
				ERF_SC_PACKETISER_PAYLOAD_BYTE_COUNT_LO);
		byte_count_hi =
			EFX_OWORD_FIELD32(counters_data[i],
				ERF_SC_PACKETISER_PAYLOAD_BYTE_COUNT_HI);

		if (id == ERF_SC_PACKETISER_HEADER_IDENTIFIER_CT) {
			/*
			 * FIXME:
			 *
			 * CT counters are 1-bit saturating counters.
			 * There is no way to express this in DPDK
			 * currently, so increment the hit count
			 * by one to let the application know
			 * that the flow is still effective.
			 */
			packet_count_lo = 1;
			packet_count_hi = 0;
			byte_count_lo = 0;
			byte_count_hi = 0;
		}

		sfc_mae_counter_increment(sa,
			counters,
			EFX_OWORD_FIELD32(counters_data[i],
				ERF_SC_PACKETISER_PAYLOAD_COUNTER_INDEX),
			generation_count,
			(uint64_t)packet_count_lo |
			((uint64_t)packet_count_hi <<
			 ERF_SC_PACKETISER_PAYLOAD_PACKET_COUNT_LO_WIDTH),
			(uint64_t)byte_count_lo |
			((uint64_t)byte_count_hi <<
			 ERF_SC_PACKETISER_PAYLOAD_BYTE_COUNT_LO_WIDTH));
	}
}

static int32_t
sfc_mae_counter_poll_packets(struct sfc_adapter *sa)
{
	struct sfc_mae_counter_registry *counter_registry =
		&sa->mae.counter_registry;
	struct rte_mbuf *mbufs[SFC_MAE_COUNTER_RX_BURST];
	unsigned int pushed_diff;
	unsigned int pushed;
	unsigned int i;
	uint16_t n;
	int rc;

	n = counter_registry->rx_pkt_burst(counter_registry->rx_dp, mbufs,
					   SFC_MAE_COUNTER_RX_BURST);

	for (i = 0; i < n; i++)
		sfc_mae_parse_counter_packet(sa, counter_registry, mbufs[i]);

	rte_pktmbuf_free_bulk(mbufs, n);

	if (!counter_registry->use_credits)
		return n;

	pushed = sfc_rx_get_pushed(sa, counter_registry->rx_dp);
	pushed_diff = pushed - counter_registry->pushed_n_buffers;

	if (pushed_diff >= SFC_COUNTER_RXQ_REFILL_LEVEL) {
		rc = efx_mae_counters_stream_give_credits(sa->nic, pushed_diff);
		if (rc == 0) {
			counter_registry->pushed_n_buffers = pushed;
		} else {
			/*
			 * FIXME: counters might be important for the
			 * application. Handle the error in order to recover
			 * from the failure
			 */
			SFC_GENERIC_LOG(DEBUG, "Give credits failed: %s",
					rte_strerror(rc));
		}
	}

	return n;
}

static int32_t
sfc_mae_counter_service_routine(void *arg)
{
	struct sfc_adapter *sa = arg;

	/*
	 * We cannot propagate any errors and we don't need to know
	 * the number of packets we've received.
	 */
	(void)sfc_mae_counter_poll_packets(sa);

	return 0;
}

static void *
sfc_mae_counter_thread(void *data)
{
	struct sfc_adapter *sa = data;
	struct sfc_mae_counter_registry *counter_registry =
		&sa->mae.counter_registry;
	int32_t rc;

	while (__atomic_load_n(&counter_registry->polling.thread.run,
			       __ATOMIC_ACQUIRE)) {
		rc = sfc_mae_counter_poll_packets(sa);
		if (rc == 0) {
			/*
			 * The queue is empty. Do not burn CPU.
			 * An empty queue has just enough space for about
			 * SFC_MAE_COUNTERS_RXQ_SPACE counter updates which is
			 * more than 100K, so we can sleep a bit. The queue uses
			 * a credit-based flow control anyway, so firmware will
			 * not enqueue more counter updates until the host
			 * supplies it with additional credits. The counters are
			 * 48bits wide, so the timeout need only be short enough
			 * to ensure that the counter values do not overflow
			 * before the next counter update. Also we should not
			 * delay counter updates for a long time, otherwise
			 * application may decide that flow is idle and should
			 * be removed.
			 */
			rte_delay_ms(1);
		}
	}

	return NULL;
}

static void
sfc_mae_counter_service_unregister(struct sfc_adapter *sa)
{
	struct sfc_mae_counter_registry *registry =
		&sa->mae.counter_registry;
	const unsigned int wait_ms = 10000;
	unsigned int i;

	rte_service_runstate_set(registry->polling.service.id, 0);
	rte_service_component_runstate_set(registry->polling.service.id, 0);

	/*
	 * Wait for the counter routine to finish the last iteration.
	 * Give up on timeout.
	 */
	for (i = 0; i < wait_ms; i++) {
		if (rte_service_may_be_active(registry->polling.service.id) == 0)
			break;

		rte_delay_ms(1);
	}
	if (i == wait_ms)
		sfc_warn(sa, "failed to wait for counter service to stop");

	rte_service_map_lcore_set(registry->polling.service.id,
				  registry->polling.service.core_id, 0);

	rte_service_component_unregister(registry->polling.service.id);
}

static struct sfc_rxq_info *
sfc_counter_rxq_info_get(struct sfc_adapter *sa)
{
	return &sfc_sa2shared(sa)->rxq_info[sa->counter_rxq.sw_index];
}

static void
sfc_mae_counter_registry_prepare(struct sfc_mae_counter_registry *registry,
				 struct sfc_adapter *sa,
				 uint32_t counter_stream_flags)
{
	registry->rx_pkt_burst = sa->eth_dev->rx_pkt_burst;
	registry->rx_dp = sfc_counter_rxq_info_get(sa)->dp;
	registry->pushed_n_buffers = 0;
	registry->use_credits = counter_stream_flags &
		EFX_MAE_COUNTERS_STREAM_OUT_USES_CREDITS;
}

static int
sfc_mae_counter_service_register(struct sfc_adapter *sa,
				 uint32_t counter_stream_flags)
{
	struct rte_service_spec service;
	char counter_service_name[sizeof(service.name)] = "counter_service";
	struct sfc_mae_counter_registry *counter_registry =
		&sa->mae.counter_registry;
	uint32_t cid;
	uint32_t sid;
	int rc;

	sfc_log_init(sa, "entry");

	/* Prepare service info */
	memset(&service, 0, sizeof(service));
	rte_strscpy(service.name, counter_service_name, sizeof(service.name));
	service.socket_id = sa->socket_id;
	service.callback = sfc_mae_counter_service_routine;
	service.callback_userdata = sa;
	sfc_mae_counter_registry_prepare(counter_registry, sa,
					 counter_stream_flags);

	cid = sfc_get_service_lcore(sa->socket_id);
	if (cid == RTE_MAX_LCORE && sa->socket_id != SOCKET_ID_ANY) {
		/* Warn and try to allocate on any NUMA node */
		sfc_warn(sa,
			"failed to get service lcore for counter service at socket %d",
			sa->socket_id);

		cid = sfc_get_service_lcore(SOCKET_ID_ANY);
	}
	if (cid == RTE_MAX_LCORE) {
		rc = ENOTSUP;
		sfc_err(sa, "failed to get service lcore for counter service");
		goto fail_get_service_lcore;
	}

	/* Service core may be in "stopped" state, start it */
	rc = rte_service_lcore_start(cid);
	if (rc != 0 && rc != -EALREADY) {
		sfc_err(sa, "failed to start service core for counter service: %s",
			rte_strerror(-rc));
		rc = ENOTSUP;
		goto fail_start_core;
	}

	/* Register counter service */
	rc = rte_service_component_register(&service, &sid);
	if (rc != 0) {
		rc = ENOEXEC;
		sfc_err(sa, "failed to register counter service component");
		goto fail_register;
	}

	/* Map the service with the service core */
	rc = rte_service_map_lcore_set(sid, cid, 1);
	if (rc != 0) {
		rc = -rc;
		sfc_err(sa, "failed to map lcore for counter service: %s",
			rte_strerror(rc));
		goto fail_map_lcore;
	}

	/* Run the service */
	rc = rte_service_component_runstate_set(sid, 1);
	if (rc < 0) {
		rc = -rc;
		sfc_err(sa, "failed to run counter service component: %s",
			rte_strerror(rc));
		goto fail_component_runstate_set;
	}
	rc = rte_service_runstate_set(sid, 1);
	if (rc < 0) {
		rc = -rc;
		sfc_err(sa, "failed to run counter service");
		goto fail_runstate_set;
	}

	counter_registry->polling_mode = SFC_MAE_COUNTER_POLLING_SERVICE;
	counter_registry->polling.service.core_id = cid;
	counter_registry->polling.service.id = sid;

	sfc_log_init(sa, "done");

	return 0;

fail_runstate_set:
	rte_service_component_runstate_set(sid, 0);

fail_component_runstate_set:
	rte_service_map_lcore_set(sid, cid, 0);

fail_map_lcore:
	rte_service_component_unregister(sid);

fail_register:
fail_start_core:
fail_get_service_lcore:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));

	return rc;
}

static void
sfc_mae_counter_thread_stop(struct sfc_adapter *sa)
{
	struct sfc_mae_counter_registry *counter_registry =
		&sa->mae.counter_registry;
	int rc;

	/* Ensure that flag is set before attempting to join thread */
	__atomic_store_n(&counter_registry->polling.thread.run, false,
			 __ATOMIC_RELEASE);

	rc = pthread_join(counter_registry->polling.thread.id, NULL);
	if (rc != 0)
		sfc_err(sa, "failed to join the MAE counter polling thread");

	counter_registry->polling_mode = SFC_MAE_COUNTER_POLLING_OFF;
}

static int
sfc_mae_counter_thread_spawn(struct sfc_adapter *sa,
			     uint32_t counter_stream_flags)
{
	struct sfc_mae_counter_registry *counter_registry =
		&sa->mae.counter_registry;
	int rc;

	sfc_log_init(sa, "entry");

	sfc_mae_counter_registry_prepare(counter_registry, sa,
					 counter_stream_flags);

	counter_registry->polling_mode = SFC_MAE_COUNTER_POLLING_THREAD;
	counter_registry->polling.thread.run = true;

	rc = rte_ctrl_thread_create(&sa->mae.counter_registry.polling.thread.id,
				    "mae_counter_thread", NULL,
				    sfc_mae_counter_thread, sa);

	return rc;
}

int
sfc_mae_counters_init(struct sfc_mae_counter_records *counters,
		      uint32_t nb_counters_max)
{
	int rc;

	SFC_GENERIC_LOG(DEBUG, "%s: entry", __func__);

	counters->mae_counters = rte_zmalloc("sfc_mae_counters",
		sizeof(*counters->mae_counters) * nb_counters_max, 0);
	if (counters->mae_counters == NULL) {
		rc = ENOMEM;
		SFC_GENERIC_LOG(ERR, "%s: failed: %s", __func__,
				rte_strerror(rc));
		return rc;
	}

	counters->n_mae_counters = nb_counters_max;

	SFC_GENERIC_LOG(DEBUG, "%s: done", __func__);

	return 0;
}

void
sfc_mae_counters_fini(struct sfc_mae_counter_records *counters)
{
	rte_free(counters->mae_counters);
	counters->mae_counters = NULL;
}

int
sfc_mae_counter_rxq_attach(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	char name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool *mp;
	unsigned int n_elements;
	unsigned int cache_size;
	/* The mempool is internal and private area is not required */
	const uint16_t priv_size = 0;
	const uint16_t data_room_size = RTE_PKTMBUF_HEADROOM +
		SFC_MAE_COUNTER_STREAM_PACKET_SIZE;
	int rc;

	sfc_log_init(sa, "entry");

	if (!sas->counters_rxq_allocated) {
		sfc_log_init(sa, "counter queue is not supported - skip");
		return 0;
	}

	/*
	 * At least one element in the ring is always unused to distinguish
	 * between empty and full ring cases.
	 */
	n_elements = SFC_COUNTER_RXQ_RX_DESC_COUNT - 1;

	/*
	 * The cache must have sufficient space to put received buckets
	 * before they're reused on refill.
	 */
	cache_size = rte_align32pow2(SFC_COUNTER_RXQ_REFILL_LEVEL +
				     SFC_MAE_COUNTER_RX_BURST - 1);

	if (snprintf(name, sizeof(name), "counter_rxq-pool-%u", sas->port_id) >=
	    (int)sizeof(name)) {
		sfc_err(sa, "failed: counter RxQ mempool name is too long");
		rc = ENAMETOOLONG;
		goto fail_long_name;
	}

	/*
	 * It could be single-producer single-consumer ring mempool which
	 * requires minimal barriers. However, cache size and refill/burst
	 * policy are aligned, therefore it does not matter which
	 * mempool backend is chosen since backend is unused.
	 */
	mp = rte_pktmbuf_pool_create(name, n_elements, cache_size,
				     priv_size, data_room_size, sa->socket_id);
	if (mp == NULL) {
		sfc_err(sa, "failed to create counter RxQ mempool");
		rc = rte_errno;
		goto fail_mp_create;
	}

	sa->counter_rxq.sw_index = sfc_counters_rxq_sw_index(sas);
	sa->counter_rxq.mp = mp;
	sa->counter_rxq.state |= SFC_COUNTER_RXQ_ATTACHED;

	sfc_log_init(sa, "done");

	return 0;

fail_mp_create:
fail_long_name:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));

	return rc;
}

void
sfc_mae_counter_rxq_detach(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);

	sfc_log_init(sa, "entry");

	if (!sas->counters_rxq_allocated) {
		sfc_log_init(sa, "counter queue is not supported - skip");
		return;
	}

	if ((sa->counter_rxq.state & SFC_COUNTER_RXQ_ATTACHED) == 0) {
		sfc_log_init(sa, "counter queue is not attached - skip");
		return;
	}

	rte_mempool_free(sa->counter_rxq.mp);
	sa->counter_rxq.mp = NULL;
	sa->counter_rxq.state &= ~SFC_COUNTER_RXQ_ATTACHED;

	sfc_log_init(sa, "done");
}

int
sfc_mae_counter_rxq_init(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	const struct rte_eth_rxconf rxconf = {
		.rx_free_thresh = SFC_COUNTER_RXQ_REFILL_LEVEL,
		.rx_drop_en = 1,
	};
	uint16_t nb_rx_desc = SFC_COUNTER_RXQ_RX_DESC_COUNT;
	int rc;

	sfc_log_init(sa, "entry");

	if (!sas->counters_rxq_allocated) {
		sfc_log_init(sa, "counter queue is not supported - skip");
		return 0;
	}

	if ((sa->counter_rxq.state & SFC_COUNTER_RXQ_ATTACHED) == 0) {
		sfc_log_init(sa, "counter queue is not attached - skip");
		return 0;
	}

	nb_rx_desc = RTE_MIN(nb_rx_desc, sa->rxq_max_entries);
	nb_rx_desc = RTE_MAX(nb_rx_desc, sa->rxq_min_entries);

	rc = sfc_rx_qinit_info(sa, sa->counter_rxq.sw_index,
			       EFX_RXQ_FLAG_USER_MARK);
	if (rc != 0)
		goto fail_counter_rxq_init_info;

	rc = sfc_rx_qinit(sa, sa->counter_rxq.sw_index, nb_rx_desc,
			  sa->socket_id, &rxconf, sa->counter_rxq.mp);
	if (rc != 0) {
		sfc_err(sa, "failed to init counter RxQ");
		goto fail_counter_rxq_init;
	}

	sa->counter_rxq.state |= SFC_COUNTER_RXQ_INITIALIZED;

	sfc_log_init(sa, "done");

	return 0;

fail_counter_rxq_init:
fail_counter_rxq_init_info:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));

	return rc;
}

void
sfc_mae_counter_rxq_fini(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);

	sfc_log_init(sa, "entry");

	if (!sas->counters_rxq_allocated) {
		sfc_log_init(sa, "counter queue is not supported - skip");
		return;
	}

	if ((sa->counter_rxq.state & SFC_COUNTER_RXQ_INITIALIZED) == 0) {
		sfc_log_init(sa, "counter queue is not initialized - skip");
		return;
	}

	sfc_rx_qfini(sa, sa->counter_rxq.sw_index);

	sfc_log_init(sa, "done");
}

void
sfc_mae_counter_stop(struct sfc_adapter *sa)
{
	struct sfc_mae *mae = &sa->mae;

	sfc_log_init(sa, "entry");

	if (!mae->counter_rxq_running) {
		sfc_log_init(sa, "counter queue is not running - skip");
		return;
	}

	SFC_ASSERT(mae->counter_registry.polling_mode !=
			SFC_MAE_COUNTER_POLLING_OFF);

	if (mae->counter_registry.polling_mode ==
			SFC_MAE_COUNTER_POLLING_SERVICE)
		sfc_mae_counter_service_unregister(sa);
	else
		sfc_mae_counter_thread_stop(sa);

	efx_mae_counters_stream_stop(sa->nic, sa->counter_rxq.sw_index, NULL);

	mae->counter_rxq_running = false;

	sfc_log_init(sa, "done");
}

int
sfc_mae_counter_start(struct sfc_adapter *sa)
{
	struct sfc_mae *mae = &sa->mae;
	uint32_t flags;
	int rc;

	SFC_ASSERT(sa->counter_rxq.state & SFC_COUNTER_RXQ_ATTACHED);

	if (mae->counter_rxq_running)
		return 0;

	sfc_log_init(sa, "entry");

	rc = efx_mae_counters_stream_start(sa->nic, sa->counter_rxq.sw_index,
					   SFC_MAE_COUNTER_STREAM_PACKET_SIZE,
					   0 /* No flags required */, &flags);
	if (rc != 0) {
		sfc_err(sa, "failed to start MAE counters stream: %s",
			rte_strerror(rc));
		goto fail_counter_stream;
	}

	sfc_log_init(sa, "stream start flags: 0x%x", flags);

	if (sfc_mae_counter_get_service_lcore(sa) != RTE_MAX_LCORE) {
		rc = sfc_mae_counter_service_register(sa, flags);
		if (rc != 0)
			goto fail_service_register;
	} else {
		rc = sfc_mae_counter_thread_spawn(sa, flags);
		if (rc != 0)
			goto fail_thread_spawn;
	}

	mae->counter_rxq_running = true;

	return 0;

fail_service_register:
fail_thread_spawn:
	efx_mae_counters_stream_stop(sa->nic, sa->counter_rxq.sw_index, NULL);

fail_counter_stream:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));

	return rc;
}

int
sfc_mae_counter_get(struct sfc_adapter *sa,
		    const struct sfc_mae_counter *counter,
		    struct rte_flow_query_count *data)
{
	struct sfc_ft_ctx *ft_ctx = counter->ft_ctx;
	struct sfc_mae_counter_records *counters;
	uint64_t non_reset_tunnel_hit_counter;
	struct sfc_mae_counter_record *p;
	union sfc_pkts_bytes value;
	bool need_byte_count;

	switch (counter->type) {
	case EFX_COUNTER_TYPE_ACTION:
		counters = &sa->mae.counter_registry.action_counters;
		need_byte_count = true;
		break;
	case EFX_COUNTER_TYPE_CONNTRACK:
		counters = &sa->mae.counter_registry.conntrack_counters;
		need_byte_count = false;
		break;
	default:
		return EINVAL;
	}

	SFC_ASSERT(counter->fw_rsrc.counter_id.id < counters->n_mae_counters);
	p = &counters->mae_counters[counter->fw_rsrc.counter_id.id];

	/*
	 * Ordering is relaxed since it is the only operation on counter value.
	 * And it does not depend on different stores/loads in other threads.
	 * Paired with relaxed ordering in counter increment.
	 */
	value.pkts_bytes.int128 = __atomic_load_n(&p->value.pkts_bytes.int128,
						  __ATOMIC_RELAXED);

	data->hits_set = 1;
	data->hits = value.pkts - p->reset.pkts;

	if (ft_ctx != NULL) {
		data->hits += ft_ctx->switch_hit_counter;
		non_reset_tunnel_hit_counter = data->hits;
		data->hits -= ft_ctx->reset_tunnel_hit_counter;
	} else if (need_byte_count) {
		data->bytes_set = 1;
		data->bytes = value.bytes - p->reset.bytes;
	}

	if (data->reset != 0) {
		if (ft_ctx != NULL) {
			ft_ctx->reset_tunnel_hit_counter =
				non_reset_tunnel_hit_counter;
		} else {
			p->reset.pkts = value.pkts;

			if (need_byte_count)
				p->reset.bytes = value.bytes;
		}
	}

	return 0;
}

bool
sfc_mae_counter_stream_enabled(struct sfc_adapter *sa)
{
	if ((sa->counter_rxq.state & SFC_COUNTER_RXQ_INITIALIZED) == 0 ||
	    sfc_get_service_lcore(SOCKET_ID_ANY) == RTE_MAX_LCORE)
		return B_FALSE;
	else
		return B_TRUE;
}
