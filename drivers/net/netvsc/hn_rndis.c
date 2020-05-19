/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2009-2018 Microsoft Corp.
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2012 NetApp Inc.
 * All rights reserved.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include <rte_ethdev_driver.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_atomic.h>
#include <rte_alarm.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_dev.h>
#include <rte_bus_vmbus.h>

#include "hn_logs.h"
#include "hn_var.h"
#include "hn_nvs.h"
#include "hn_rndis.h"
#include "ndis.h"

#define RNDIS_TIMEOUT_SEC 5
#define RNDIS_DELAY_MS    10

#define HN_RNDIS_XFER_SIZE		0x4000

#define HN_NDIS_TXCSUM_CAP_IP4		\
	(NDIS_TXCSUM_CAP_IP4 | NDIS_TXCSUM_CAP_IP4OPT)
#define HN_NDIS_TXCSUM_CAP_TCP4		\
	(NDIS_TXCSUM_CAP_TCP4 | NDIS_TXCSUM_CAP_TCP4OPT)
#define HN_NDIS_TXCSUM_CAP_TCP6		\
	(NDIS_TXCSUM_CAP_TCP6 | NDIS_TXCSUM_CAP_TCP6OPT | \
	 NDIS_TXCSUM_CAP_IP6EXT)
#define HN_NDIS_TXCSUM_CAP_UDP6		\
	(NDIS_TXCSUM_CAP_UDP6 | NDIS_TXCSUM_CAP_IP6EXT)
#define HN_NDIS_LSOV2_CAP_IP6		\
	(NDIS_LSOV2_CAP_IP6EXT | NDIS_LSOV2_CAP_TCP6OPT)

/* Get unique request id */
static inline uint32_t
hn_rndis_rid(struct hn_data *hv)
{
	uint32_t rid;

	do {
		rid = rte_atomic32_add_return(&hv->rndis_req_id, 1);
	} while (rid == 0);

	return rid;
}

static void *hn_rndis_alloc(size_t size)
{
	return rte_zmalloc("RNDIS", size, PAGE_SIZE);
}

#ifdef RTE_LIBRTE_NETVSC_DEBUG_DUMP
void hn_rndis_dump(const void *buf)
{
	const union {
		struct rndis_msghdr hdr;
		struct rndis_packet_msg pkt;
		struct rndis_init_req init_request;
		struct rndis_init_comp init_complete;
		struct rndis_halt_req halt;
		struct rndis_query_req query_request;
		struct rndis_query_comp query_complete;
		struct rndis_set_req set_request;
		struct rndis_set_comp set_complete;
		struct rndis_reset_req reset_request;
		struct rndis_reset_comp reset_complete;
		struct rndis_keepalive_req keepalive_request;
		struct rndis_keepalive_comp keepalive_complete;
		struct rndis_status_msg indicate_status;
	} *rndis_msg = buf;

	switch (rndis_msg->hdr.type) {
	case RNDIS_PACKET_MSG: {
		const struct rndis_pktinfo *ppi;
		unsigned int ppi_len;

		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_MSG_PACKET (len %u, data %u:%u, # oob %u %u:%u, pkt %u:%u)\n",
			    rndis_msg->pkt.len,
			    rndis_msg->pkt.dataoffset,
			    rndis_msg->pkt.datalen,
			    rndis_msg->pkt.oobdataelements,
			    rndis_msg->pkt.oobdataoffset,
			    rndis_msg->pkt.oobdatalen,
			    rndis_msg->pkt.pktinfooffset,
			    rndis_msg->pkt.pktinfolen);

		ppi = (const struct rndis_pktinfo *)
			((const char *)buf
			 + RNDIS_PACKET_MSG_OFFSET_ABS(rndis_msg->pkt.pktinfooffset));

		ppi_len = rndis_msg->pkt.pktinfolen;
		while (ppi_len > 0) {
			const void *ppi_data;

			ppi_data = ppi->data;

			rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
				"    PPI (size %u, type %u, offs %u data %#x)\n",
				ppi->size, ppi->type, ppi->offset,
				*(const uint32_t *)ppi_data);
			if (ppi->size == 0)
				break;
			ppi_len -= ppi->size;
			ppi = (const struct rndis_pktinfo *)
				((const char *)ppi + ppi->size);
		}
		break;
	}
	case RNDIS_INITIALIZE_MSG:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_MSG_INIT (len %u id %#x, ver %u.%u max xfer %u)\n",
			    rndis_msg->init_request.len,
			    rndis_msg->init_request.rid,
			    rndis_msg->init_request.ver_major,
			    rndis_msg->init_request.ver_minor,
			    rndis_msg->init_request.max_xfersz);
		break;

	case RNDIS_INITIALIZE_CMPLT:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_MSG_INIT_C (len %u, id %#x, status 0x%x, vers %u.%u, "
			    "flags %d, max xfer %u, max pkts %u, aligned %u)\n",
			    rndis_msg->init_complete.len,
			    rndis_msg->init_complete.rid,
			    rndis_msg->init_complete.status,
			    rndis_msg->init_complete.ver_major,
			    rndis_msg->init_complete.ver_minor,
			    rndis_msg->init_complete.devflags,
			    rndis_msg->init_complete.pktmaxsz,
			    rndis_msg->init_complete.pktmaxcnt,
			    rndis_msg->init_complete.align);
		break;

	case RNDIS_HALT_MSG:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_HALT (len %u id %#x)\n",
			    rndis_msg->halt.len, rndis_msg->halt.rid);
		break;

	case RNDIS_QUERY_MSG:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_QUERY (len %u, id %#x, oid %#x, info %u:%u)\n",
			    rndis_msg->query_request.len,
			    rndis_msg->query_request.rid,
			    rndis_msg->query_request.oid,
			    rndis_msg->query_request.infobuflen,
			    rndis_msg->query_request.infobufoffset);
		break;

	case RNDIS_QUERY_CMPLT:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_MSG_QUERY_C (len %u, id %#x, status 0x%x, buf %u:%u)\n",
			    rndis_msg->query_complete.len,
			    rndis_msg->query_complete.rid,
			    rndis_msg->query_complete.status,
			    rndis_msg->query_complete.infobuflen,
			    rndis_msg->query_complete.infobufoffset);
		break;

	case RNDIS_SET_MSG:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_SET (len %u, id %#x, oid %#x, info %u:%u)\n",
			    rndis_msg->set_request.len,
			    rndis_msg->set_request.rid,
			    rndis_msg->set_request.oid,
			    rndis_msg->set_request.infobuflen,
			    rndis_msg->set_request.infobufoffset);
		break;

	case RNDIS_SET_CMPLT:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_MSG_SET_C (len %u, id 0x%x, status 0x%x)\n",
			    rndis_msg->set_complete.len,
			    rndis_msg->set_complete.rid,
			    rndis_msg->set_complete.status);
		break;

	case RNDIS_INDICATE_STATUS_MSG:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_MSG_INDICATE (len %u, status %#x, buf len %u, buf offset %u)\n",
			    rndis_msg->indicate_status.len,
			    rndis_msg->indicate_status.status,
			    rndis_msg->indicate_status.stbuflen,
			    rndis_msg->indicate_status.stbufoffset);
		break;

	case RNDIS_RESET_MSG:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_RESET (len %u, id %#x)\n",
			    rndis_msg->reset_request.len,
			    rndis_msg->reset_request.rid);
		break;

	case RNDIS_RESET_CMPLT:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_RESET_C (len %u, status %#x address %#x)\n",
			    rndis_msg->reset_complete.len,
			    rndis_msg->reset_complete.status,
			    rndis_msg->reset_complete.adrreset);
		break;

	case RNDIS_KEEPALIVE_MSG:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_KEEPALIVE (len %u, id %#x)\n",
			    rndis_msg->keepalive_request.len,
			    rndis_msg->keepalive_request.rid);
		break;

	case RNDIS_KEEPALIVE_CMPLT:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS_KEEPALIVE_C (len %u, id %#x address %#x)\n",
			    rndis_msg->keepalive_complete.len,
			    rndis_msg->keepalive_complete.rid,
			    rndis_msg->keepalive_complete.status);
		break;

	default:
		rte_log(RTE_LOG_DEBUG, hn_logtype_driver,
			    "RNDIS type %#x len %u\n",
			    rndis_msg->hdr.type,
			    rndis_msg->hdr.len);
		break;
	}
}
#endif

static int hn_nvs_send_rndis_ctrl(struct vmbus_channel *chan,
				  const void *req, uint32_t reqlen)

{
	struct hn_nvs_rndis nvs_rndis = {
		.type = NVS_TYPE_RNDIS,
		.rndis_mtype = NVS_RNDIS_MTYPE_CTRL,
		.chim_idx = NVS_CHIM_IDX_INVALID,
		.chim_sz = 0
	};
	struct vmbus_gpa sg;
	rte_iova_t addr;

	addr = rte_malloc_virt2iova(req);
	if (unlikely(addr == RTE_BAD_IOVA)) {
		PMD_DRV_LOG(ERR, "RNDIS send request can not get iova");
		return -EINVAL;
	}

	if (unlikely(reqlen > PAGE_SIZE)) {
		PMD_DRV_LOG(ERR, "RNDIS request %u greater than page size",
			    reqlen);
		return -EINVAL;
	}

	sg.page = addr / PAGE_SIZE;
	sg.ofs  = addr & PAGE_MASK;
	sg.len  = reqlen;

	if (sg.ofs + reqlen >  PAGE_SIZE) {
		PMD_DRV_LOG(ERR, "RNDIS request crosses page bounary");
		return -EINVAL;
	}

	hn_rndis_dump(req);

	return hn_nvs_send_sglist(chan, &sg, 1,
				  &nvs_rndis, sizeof(nvs_rndis), 0U, NULL);
}

/*
 * Alarm callback to process link changed notifications.
 * Can not directly since link_status is discovered while reading ring
 */
static void hn_rndis_link_alarm(void *arg)
{
	_rte_eth_dev_callback_process(arg, RTE_ETH_EVENT_INTR_LSC, NULL);
}

void hn_rndis_link_status(struct rte_eth_dev *dev, const void *msg)
{
	const struct rndis_status_msg *indicate = msg;

	hn_rndis_dump(msg);

	PMD_DRV_LOG(DEBUG, "link status %#x", indicate->status);

	switch (indicate->status) {
	case RNDIS_STATUS_NETWORK_CHANGE:
	case RNDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG:
		/* ignore not in DPDK API */
		break;

	case RNDIS_STATUS_LINK_SPEED_CHANGE:
	case RNDIS_STATUS_MEDIA_CONNECT:
	case RNDIS_STATUS_MEDIA_DISCONNECT:
		if (dev->data->dev_conf.intr_conf.lsc)
			rte_eal_alarm_set(10, hn_rndis_link_alarm, dev);
		break;
	default:
		PMD_DRV_LOG(NOTICE, "unknown RNDIS indication: %#x",
			    indicate->status);
	}
}

/* Callback from hn_process_events when response is visible */
void hn_rndis_receive_response(struct hn_data *hv,
			       const void *data, uint32_t len)
{
	const struct rndis_init_comp *hdr = data;

	hn_rndis_dump(data);

	if (len < sizeof(3 * sizeof(uint32_t))) {
		PMD_DRV_LOG(ERR,
			    "missing RNDIS header %u", len);
		return;
	}

	if (len < hdr->len) {
		PMD_DRV_LOG(ERR,
			    "truncated RNDIS response %u", len);
		return;
	}

	if  (len > sizeof(hv->rndis_resp)) {
		PMD_DRV_LOG(NOTICE,
			    "RNDIS response exceeds buffer");
		len = sizeof(hv->rndis_resp);
	}

	if (hdr->rid == 0) {
		PMD_DRV_LOG(NOTICE,
			    "RNDIS response id zero!");
	}

	memcpy(hv->rndis_resp, data, len);

	/* make sure response copied before update */
	rte_smp_wmb();

	if (rte_atomic32_cmpset(&hv->rndis_pending, hdr->rid, 0) == 0) {
		PMD_DRV_LOG(NOTICE,
			    "received id %#x pending id %#x",
			    hdr->rid, (uint32_t)hv->rndis_pending);
	}
}

/* Do request/response transaction */
static int hn_rndis_exec1(struct hn_data *hv,
			  const void *req, uint32_t reqlen,
			  void *comp, uint32_t comp_len)
{
	const struct rndis_halt_req *hdr = req;
	uint32_t rid = hdr->rid;
	struct vmbus_channel *chan = hn_primary_chan(hv);
	int error;

	if (comp_len > sizeof(hv->rndis_resp)) {
		PMD_DRV_LOG(ERR,
			    "Expected completion size %u exceeds buffer %zu",
			    comp_len, sizeof(hv->rndis_resp));
		return -EIO;
	}

	if (rid == 0) {
		PMD_DRV_LOG(ERR, "Invalid request id");
		return -EINVAL;
	}

	if (comp != NULL &&
	    rte_atomic32_cmpset(&hv->rndis_pending, 0, rid) == 0) {
		PMD_DRV_LOG(ERR,
			    "Request already pending");
		return -EBUSY;
	}

	error = hn_nvs_send_rndis_ctrl(chan, req, reqlen);
	if (error) {
		PMD_DRV_LOG(ERR, "RNDIS ctrl send failed: %d", error);
		return error;
	}

	if (comp) {
		time_t start = time(NULL);

		/* Poll primary channel until response received */
		while (hv->rndis_pending == rid) {
			if (hv->closed)
				return -ENETDOWN;

			if (time(NULL) - start > RNDIS_TIMEOUT_SEC) {
				PMD_DRV_LOG(ERR,
					    "RNDIS response timed out");

				rte_atomic32_cmpset(&hv->rndis_pending, rid, 0);
				return -ETIMEDOUT;
			}

			if (rte_vmbus_chan_rx_empty(hv->primary->chan))
				rte_delay_ms(RNDIS_DELAY_MS);

			hn_process_events(hv, 0, 1);
		}

		memcpy(comp, hv->rndis_resp, comp_len);
	}

	return 0;
}

/* Do transaction and validate response */
static int hn_rndis_execute(struct hn_data *hv, uint32_t rid,
			    const void *req, uint32_t reqlen,
			    void *comp, uint32_t comp_len, uint32_t comp_type)
{
	const struct rndis_comp_hdr *hdr = comp;
	int ret;

	memset(comp, 0, comp_len);

	ret = hn_rndis_exec1(hv, req, reqlen, comp, comp_len);
	if (ret < 0)
		return ret;
	/*
	 * Check this RNDIS complete message.
	 */
	if (unlikely(hdr->type != comp_type)) {
		PMD_DRV_LOG(ERR,
			    "unexpected RNDIS response complete %#x expect %#x",
			    hdr->type, comp_type);

		return -ENXIO;
	}
	if (unlikely(hdr->rid != rid)) {
		PMD_DRV_LOG(ERR,
			    "RNDIS comp rid mismatch %#x, expect %#x",
			    hdr->rid, rid);
		return -EINVAL;
	}

	/* All pass! */
	return 0;
}

static int
hn_rndis_query(struct hn_data *hv, uint32_t oid,
	       const void *idata, uint32_t idlen,
	       void *odata, uint32_t odlen)
{
	struct rndis_query_req *req;
	struct rndis_query_comp *comp;
	uint32_t reqlen, comp_len;
	int error = -EIO;
	unsigned int ofs;
	uint32_t rid;

	reqlen = sizeof(*req) + idlen;
	req = hn_rndis_alloc(reqlen);
	if (req == NULL)
		return -ENOMEM;

	comp_len = sizeof(*comp) + odlen;
	comp = rte_zmalloc("QUERY", comp_len, PAGE_SIZE);
	if (!comp) {
		error = -ENOMEM;
		goto done;
	}
	comp->status = RNDIS_STATUS_PENDING;

	rid = hn_rndis_rid(hv);

	req->type = RNDIS_QUERY_MSG;
	req->len = reqlen;
	req->rid = rid;
	req->oid = oid;
	req->infobufoffset = RNDIS_QUERY_REQ_INFOBUFOFFSET;
	req->infobuflen = idlen;

	/* Input data immediately follows RNDIS query. */
	memcpy(req + 1, idata, idlen);

	error = hn_rndis_execute(hv, rid, req, reqlen,
				 comp, comp_len, RNDIS_QUERY_CMPLT);

	if (error)
		goto done;

	if (comp->status != RNDIS_STATUS_SUCCESS) {
		PMD_DRV_LOG(ERR, "RNDIS query 0x%08x failed: status 0x%08x",
			    oid, comp->status);
		error = -EINVAL;
		goto done;
	}

	if (comp->infobuflen == 0 || comp->infobufoffset == 0) {
		/* No output data! */
		PMD_DRV_LOG(ERR, "RNDIS query 0x%08x, no data", oid);
		error = 0;
		goto done;
	}

	/*
	 * Check output data length and offset.
	 */
	/* ofs is the offset from the beginning of comp. */
	ofs = RNDIS_QUERY_COMP_INFOBUFOFFSET_ABS(comp->infobufoffset);
	if (ofs < sizeof(*comp) || ofs + comp->infobuflen > comp_len) {
		PMD_DRV_LOG(ERR, "RNDIS query invalid comp ib off/len, %u/%u",
			    comp->infobufoffset, comp->infobuflen);
		error = -EINVAL;
		goto done;
	}

	/* Save output data. */
	if (comp->infobuflen < odlen)
		odlen = comp->infobuflen;

	/* ofs is the offset from the beginning of comp. */
	memcpy(odata, (const char *)comp + ofs, odlen);

	error = 0;
done:
	rte_free(comp);
	rte_free(req);
	return error;
}

static int
hn_rndis_halt(struct hn_data *hv)
{
	struct rndis_halt_req *halt;

	halt = hn_rndis_alloc(sizeof(*halt));
	if (halt == NULL)
		return -ENOMEM;

	halt->type = RNDIS_HALT_MSG;
	halt->len = sizeof(*halt);
	halt->rid = hn_rndis_rid(hv);

	/* No RNDIS completion; rely on NVS message send completion */
	hn_rndis_exec1(hv, halt, sizeof(*halt), NULL, 0);

	rte_free(halt);

	PMD_INIT_LOG(DEBUG, "RNDIS halt done");
	return 0;
}

static int
hn_rndis_query_hwcaps(struct hn_data *hv, struct ndis_offload *caps)
{
	struct ndis_offload in;
	uint32_t caps_len, size;
	int error;

	memset(caps, 0, sizeof(*caps));
	memset(&in, 0, sizeof(in));
	in.ndis_hdr.ndis_type = NDIS_OBJTYPE_OFFLOAD;

	if (hv->ndis_ver >= NDIS_VERSION_6_30) {
		in.ndis_hdr.ndis_rev = NDIS_OFFLOAD_REV_3;
		size = NDIS_OFFLOAD_SIZE;
	} else if (hv->ndis_ver >= NDIS_VERSION_6_1) {
		in.ndis_hdr.ndis_rev = NDIS_OFFLOAD_REV_2;
		size = NDIS_OFFLOAD_SIZE_6_1;
	} else {
		in.ndis_hdr.ndis_rev = NDIS_OFFLOAD_REV_1;
		size = NDIS_OFFLOAD_SIZE_6_0;
	}
	in.ndis_hdr.ndis_size = size;

	caps_len = NDIS_OFFLOAD_SIZE;
	error = hn_rndis_query(hv, OID_TCP_OFFLOAD_HARDWARE_CAPABILITIES,
			       &in, size, caps, caps_len);
	if (error)
		return error;

	/* Preliminary verification. */
	if (caps->ndis_hdr.ndis_type != NDIS_OBJTYPE_OFFLOAD) {
		PMD_DRV_LOG(NOTICE, "invalid NDIS objtype 0x%02x",
			    caps->ndis_hdr.ndis_type);
		return -EINVAL;
	}
	if (caps->ndis_hdr.ndis_rev < NDIS_OFFLOAD_REV_1) {
		PMD_DRV_LOG(NOTICE, "invalid NDIS objrev 0x%02x",
			    caps->ndis_hdr.ndis_rev);
		return -EINVAL;
	}
	if (caps->ndis_hdr.ndis_size > caps_len) {
		PMD_DRV_LOG(NOTICE, "invalid NDIS objsize %u, data size %u",
			    caps->ndis_hdr.ndis_size, caps_len);
		return -EINVAL;
	} else if (caps->ndis_hdr.ndis_size < NDIS_OFFLOAD_SIZE_6_0) {
		PMD_DRV_LOG(NOTICE, "invalid NDIS objsize %u",
			    caps->ndis_hdr.ndis_size);
		return -EINVAL;
	}

	return 0;
}

int
hn_rndis_query_rsscaps(struct hn_data *hv,
		       unsigned int *rxr_cnt0)
{
	struct ndis_rss_caps in, caps;
	unsigned int indsz, rxr_cnt;
	uint32_t caps_len;
	int error;

	*rxr_cnt0 = 0;

	if (hv->ndis_ver < NDIS_VERSION_6_20) {
		PMD_DRV_LOG(DEBUG, "RSS not supported on this host");
		return -EOPNOTSUPP;
	}

	memset(&in, 0, sizeof(in));
	in.ndis_hdr.ndis_type = NDIS_OBJTYPE_RSS_CAPS;
	in.ndis_hdr.ndis_rev = NDIS_RSS_CAPS_REV_2;
	in.ndis_hdr.ndis_size = NDIS_RSS_CAPS_SIZE;

	caps_len = NDIS_RSS_CAPS_SIZE;
	error = hn_rndis_query(hv, OID_GEN_RECEIVE_SCALE_CAPABILITIES,
			       &in, NDIS_RSS_CAPS_SIZE,
			       &caps, caps_len);
	if (error)
		return error;

	PMD_INIT_LOG(DEBUG, "RX rings %u indirect %u caps %#x",
		     caps.ndis_nrxr, caps.ndis_nind, caps.ndis_caps);
	/*
	 * Preliminary verification.
	 */
	if (caps.ndis_hdr.ndis_type != NDIS_OBJTYPE_RSS_CAPS) {
		PMD_DRV_LOG(ERR, "invalid NDIS objtype 0x%02x",
			    caps.ndis_hdr.ndis_type);
		return -EINVAL;
	}
	if (caps.ndis_hdr.ndis_rev < NDIS_RSS_CAPS_REV_1) {
		PMD_DRV_LOG(ERR, "invalid NDIS objrev 0x%02x",
			    caps.ndis_hdr.ndis_rev);
		return -EINVAL;
	}
	if (caps.ndis_hdr.ndis_size > caps_len) {
		PMD_DRV_LOG(ERR,
			    "invalid NDIS objsize %u, data size %u",
			    caps.ndis_hdr.ndis_size, caps_len);
		return -EINVAL;
	} else if (caps.ndis_hdr.ndis_size < NDIS_RSS_CAPS_SIZE_6_0) {
		PMD_DRV_LOG(ERR, "invalid NDIS objsize %u",
			    caps.ndis_hdr.ndis_size);
		return -EINVAL;
	}

	/*
	 * Save information for later RSS configuration.
	 */
	if (caps.ndis_nrxr == 0) {
		PMD_DRV_LOG(ERR, "0 RX rings!?");
		return -EINVAL;
	}
	rxr_cnt = caps.ndis_nrxr;

	if (caps.ndis_hdr.ndis_size == NDIS_RSS_CAPS_SIZE &&
	    caps.ndis_hdr.ndis_rev >= NDIS_RSS_CAPS_REV_2) {
		if (caps.ndis_nind > NDIS_HASH_INDCNT) {
			PMD_DRV_LOG(ERR,
				    "too many RSS indirect table entries %u",
				    caps.ndis_nind);
			return -EOPNOTSUPP;
		}
		if (!rte_is_power_of_2(caps.ndis_nind)) {
			PMD_DRV_LOG(ERR,
				    "RSS indirect table size is not power-of-2 %u",
				    caps.ndis_nind);
		}

		indsz = caps.ndis_nind;
	} else {
		indsz = NDIS_HASH_INDCNT;
	}

	if (indsz < rxr_cnt) {
		PMD_DRV_LOG(NOTICE,
			    "# of RX rings (%d) > RSS indirect table size %d",
			    rxr_cnt, indsz);
		rxr_cnt = indsz;
	}

	hv->rss_offloads = 0;
	if (caps.ndis_caps & NDIS_RSS_CAP_IPV4)
		hv->rss_offloads |= ETH_RSS_IPV4
			| ETH_RSS_NONFRAG_IPV4_TCP
			| ETH_RSS_NONFRAG_IPV4_UDP;
	if (caps.ndis_caps & NDIS_RSS_CAP_IPV6)
		hv->rss_offloads |= ETH_RSS_IPV6
			| ETH_RSS_NONFRAG_IPV6_TCP;
	if (caps.ndis_caps & NDIS_RSS_CAP_IPV6_EX)
		hv->rss_offloads |= ETH_RSS_IPV6_EX
			| ETH_RSS_IPV6_TCP_EX;

	/* Commit! */
	*rxr_cnt0 = rxr_cnt;

	return 0;
}

static int
hn_rndis_set(struct hn_data *hv, uint32_t oid, const void *data, uint32_t dlen)
{
	struct rndis_set_req *req;
	struct rndis_set_comp comp;
	uint32_t reqlen, comp_len;
	uint32_t rid;
	int error;

	reqlen = sizeof(*req) + dlen;
	req = rte_zmalloc("RNDIS_SET", reqlen, PAGE_SIZE);
	if (!req)
		return -ENOMEM;

	rid = hn_rndis_rid(hv);
	req->type = RNDIS_SET_MSG;
	req->len = reqlen;
	req->rid = rid;
	req->oid = oid;
	req->infobuflen = dlen;
	req->infobufoffset = RNDIS_SET_REQ_INFOBUFOFFSET;

	/* Data immediately follows RNDIS set. */
	memcpy(req + 1, data, dlen);

	comp_len = sizeof(comp);
	error = hn_rndis_execute(hv, rid, req, reqlen,
				 &comp, comp_len,
				 RNDIS_SET_CMPLT);
	if (error) {
		PMD_DRV_LOG(ERR, "exec RNDIS set %#" PRIx32 " failed",
			    oid);
		error = EIO;
		goto done;
	}

	if (comp.status != RNDIS_STATUS_SUCCESS) {
		PMD_DRV_LOG(ERR,
			    "RNDIS set %#" PRIx32 " failed: status %#" PRIx32,
			    oid, comp.status);
		error = EIO;
		goto done;
	}

done:
	rte_free(req);
	return error;
}

int hn_rndis_conf_offload(struct hn_data *hv,
			  uint64_t tx_offloads, uint64_t rx_offloads)
{
	struct ndis_offload_params params;
	struct ndis_offload hwcaps;
	int error;

	error = hn_rndis_query_hwcaps(hv, &hwcaps);
	if (error) {
		PMD_DRV_LOG(ERR, "hwcaps query failed: %d", error);
		return error;
	}

	/* NOTE: 0 means "no change" */
	memset(&params, 0, sizeof(params));

	params.ndis_hdr.ndis_type = NDIS_OBJTYPE_DEFAULT;
	if (hv->ndis_ver < NDIS_VERSION_6_30) {
		params.ndis_hdr.ndis_rev = NDIS_OFFLOAD_PARAMS_REV_2;
		params.ndis_hdr.ndis_size = NDIS_OFFLOAD_PARAMS_SIZE_6_1;
	} else {
		params.ndis_hdr.ndis_rev = NDIS_OFFLOAD_PARAMS_REV_3;
		params.ndis_hdr.ndis_size = NDIS_OFFLOAD_PARAMS_SIZE;
	}

	if (tx_offloads & DEV_TX_OFFLOAD_TCP_CKSUM) {
		if (hwcaps.ndis_csum.ndis_ip4_txcsum & NDIS_TXCSUM_CAP_TCP4)
			params.ndis_tcp4csum = NDIS_OFFLOAD_PARAM_TX;
		else
			goto unsupported;

		if (hwcaps.ndis_csum.ndis_ip6_txcsum & NDIS_TXCSUM_CAP_TCP6)
			params.ndis_tcp6csum = NDIS_OFFLOAD_PARAM_TX;
		else
			goto unsupported;
	}

	if (rx_offloads & DEV_RX_OFFLOAD_TCP_CKSUM) {
		if ((hwcaps.ndis_csum.ndis_ip4_rxcsum & NDIS_RXCSUM_CAP_TCP4)
		    == NDIS_RXCSUM_CAP_TCP4)
			params.ndis_tcp4csum |= NDIS_OFFLOAD_PARAM_RX;
		else
			goto unsupported;

		if ((hwcaps.ndis_csum.ndis_ip6_rxcsum & NDIS_RXCSUM_CAP_TCP6)
		    == NDIS_RXCSUM_CAP_TCP6)
			params.ndis_tcp6csum |= NDIS_OFFLOAD_PARAM_RX;
		else
			goto unsupported;
	}

	if (tx_offloads & DEV_TX_OFFLOAD_UDP_CKSUM) {
		if (hwcaps.ndis_csum.ndis_ip4_txcsum & NDIS_TXCSUM_CAP_UDP4)
			params.ndis_udp4csum = NDIS_OFFLOAD_PARAM_TX;
		else
			goto unsupported;

		if ((hwcaps.ndis_csum.ndis_ip6_txcsum & NDIS_TXCSUM_CAP_UDP6)
		    == NDIS_TXCSUM_CAP_UDP6)
			params.ndis_udp6csum = NDIS_OFFLOAD_PARAM_TX;
		else
			goto unsupported;
	}

	if (rx_offloads & DEV_TX_OFFLOAD_UDP_CKSUM) {
		if (hwcaps.ndis_csum.ndis_ip4_rxcsum & NDIS_RXCSUM_CAP_UDP4)
			params.ndis_udp4csum |= NDIS_OFFLOAD_PARAM_RX;
		else
			goto unsupported;

		if (hwcaps.ndis_csum.ndis_ip6_rxcsum & NDIS_RXCSUM_CAP_UDP6)
			params.ndis_udp6csum |= NDIS_OFFLOAD_PARAM_RX;
		else
			goto unsupported;
	}

	if (tx_offloads & DEV_TX_OFFLOAD_IPV4_CKSUM) {
		if ((hwcaps.ndis_csum.ndis_ip4_txcsum & NDIS_TXCSUM_CAP_IP4)
		    == NDIS_TXCSUM_CAP_IP4)
			params.ndis_ip4csum = NDIS_OFFLOAD_PARAM_TX;
		else
			goto unsupported;
	}
	if (rx_offloads & DEV_RX_OFFLOAD_IPV4_CKSUM) {
		if (hwcaps.ndis_csum.ndis_ip4_rxcsum & NDIS_RXCSUM_CAP_IP4)
			params.ndis_ip4csum |= NDIS_OFFLOAD_PARAM_RX;
		else
			goto unsupported;
	}

	if (tx_offloads & DEV_TX_OFFLOAD_TCP_TSO) {
		if (hwcaps.ndis_lsov2.ndis_ip4_encap & NDIS_OFFLOAD_ENCAP_8023)
			params.ndis_lsov2_ip4 = NDIS_OFFLOAD_LSOV2_ON;
		else
			goto unsupported;

		if ((hwcaps.ndis_lsov2.ndis_ip6_opts & HN_NDIS_LSOV2_CAP_IP6)
		    == HN_NDIS_LSOV2_CAP_IP6)
			params.ndis_lsov2_ip6 = NDIS_OFFLOAD_LSOV2_ON;
		else
			goto unsupported;
	}

	error = hn_rndis_set(hv, OID_TCP_OFFLOAD_PARAMETERS, &params,
			     params.ndis_hdr.ndis_size);
	if (error) {
		PMD_DRV_LOG(ERR, "offload config failed");
		return error;
	}

	return 0;
 unsupported:
	PMD_DRV_LOG(NOTICE,
		    "offload tx:%" PRIx64 " rx:%" PRIx64 " not supported by this version",
		    tx_offloads, rx_offloads);
	return -EINVAL;
}

int hn_rndis_get_offload(struct hn_data *hv,
			 struct rte_eth_dev_info *dev_info)
{
	struct ndis_offload hwcaps;
	int error;

	memset(&hwcaps, 0, sizeof(hwcaps));

	error = hn_rndis_query_hwcaps(hv, &hwcaps);
	if (error) {
		PMD_DRV_LOG(ERR, "hwcaps query failed: %d", error);
		return error;
	}

	dev_info->tx_offload_capa = DEV_TX_OFFLOAD_MULTI_SEGS |
				    DEV_TX_OFFLOAD_VLAN_INSERT;

	if ((hwcaps.ndis_csum.ndis_ip4_txcsum & HN_NDIS_TXCSUM_CAP_IP4)
	    == HN_NDIS_TXCSUM_CAP_IP4)
		dev_info->tx_offload_capa |= DEV_TX_OFFLOAD_IPV4_CKSUM;

	if ((hwcaps.ndis_csum.ndis_ip4_txcsum & HN_NDIS_TXCSUM_CAP_TCP4)
	    == HN_NDIS_TXCSUM_CAP_TCP4 &&
	    (hwcaps.ndis_csum.ndis_ip6_txcsum & HN_NDIS_TXCSUM_CAP_TCP6)
	    == HN_NDIS_TXCSUM_CAP_TCP6)
		dev_info->tx_offload_capa |= DEV_TX_OFFLOAD_TCP_CKSUM;

	if ((hwcaps.ndis_csum.ndis_ip4_txcsum & NDIS_TXCSUM_CAP_UDP4) &&
	    (hwcaps.ndis_csum.ndis_ip6_txcsum & NDIS_TXCSUM_CAP_UDP6))
		dev_info->tx_offload_capa |= DEV_TX_OFFLOAD_UDP_CKSUM;

	if ((hwcaps.ndis_lsov2.ndis_ip4_encap & NDIS_OFFLOAD_ENCAP_8023) &&
	    (hwcaps.ndis_lsov2.ndis_ip6_opts & HN_NDIS_LSOV2_CAP_IP6)
	    == HN_NDIS_LSOV2_CAP_IP6)
		dev_info->tx_offload_capa |= DEV_TX_OFFLOAD_TCP_TSO;

	dev_info->rx_offload_capa = DEV_RX_OFFLOAD_VLAN_STRIP |
				    DEV_RX_OFFLOAD_RSS_HASH;

	if (hwcaps.ndis_csum.ndis_ip4_rxcsum & NDIS_RXCSUM_CAP_IP4)
		dev_info->rx_offload_capa |= DEV_RX_OFFLOAD_IPV4_CKSUM;

	if ((hwcaps.ndis_csum.ndis_ip4_rxcsum & NDIS_RXCSUM_CAP_TCP4) &&
	    (hwcaps.ndis_csum.ndis_ip6_rxcsum & NDIS_RXCSUM_CAP_TCP6))
		dev_info->rx_offload_capa |= DEV_RX_OFFLOAD_TCP_CKSUM;

	if ((hwcaps.ndis_csum.ndis_ip4_rxcsum & NDIS_RXCSUM_CAP_UDP4) &&
	    (hwcaps.ndis_csum.ndis_ip6_rxcsum & NDIS_RXCSUM_CAP_UDP6))
		dev_info->rx_offload_capa |= DEV_RX_OFFLOAD_UDP_CKSUM;

	return 0;
}

uint32_t
hn_rndis_get_ptypes(struct hn_data *hv)
{
	struct ndis_offload hwcaps;
	uint32_t ptypes;
	int error;

	memset(&hwcaps, 0, sizeof(hwcaps));

	error = hn_rndis_query_hwcaps(hv, &hwcaps);
	if (error) {
		PMD_DRV_LOG(ERR, "hwcaps query failed: %d", error);
		return RTE_PTYPE_L2_ETHER;
	}

	ptypes = RTE_PTYPE_L2_ETHER;

	if (hwcaps.ndis_csum.ndis_ip4_rxcsum & NDIS_RXCSUM_CAP_IP4)
		ptypes |= RTE_PTYPE_L3_IPV4;

	if ((hwcaps.ndis_csum.ndis_ip4_rxcsum & NDIS_RXCSUM_CAP_TCP4) ||
	    (hwcaps.ndis_csum.ndis_ip6_rxcsum & NDIS_RXCSUM_CAP_TCP6))
		ptypes |= RTE_PTYPE_L4_TCP;

	if ((hwcaps.ndis_csum.ndis_ip4_rxcsum & NDIS_RXCSUM_CAP_UDP4) ||
	    (hwcaps.ndis_csum.ndis_ip6_rxcsum & NDIS_RXCSUM_CAP_UDP6))
		ptypes |= RTE_PTYPE_L4_UDP;

	return ptypes;
}

int
hn_rndis_set_rxfilter(struct hn_data *hv, uint32_t filter)
{
	int error;

	error = hn_rndis_set(hv, OID_GEN_CURRENT_PACKET_FILTER,
			     &filter, sizeof(filter));
	if (error) {
		PMD_DRV_LOG(ERR, "set RX filter %#" PRIx32 " failed: %d",
			    filter, error);
	} else {
		PMD_DRV_LOG(DEBUG, "set RX filter %#" PRIx32 " done", filter);
	}

	return error;
}

int hn_rndis_conf_rss(struct hn_data *hv, uint32_t flags)
{
	struct ndis_rssprm_toeplitz rssp;
	struct ndis_rss_params *prm = &rssp.rss_params;
	unsigned int i;
	int error;

	memset(&rssp, 0, sizeof(rssp));

	prm->ndis_hdr.ndis_type = NDIS_OBJTYPE_RSS_PARAMS;
	prm->ndis_hdr.ndis_rev = NDIS_RSS_PARAMS_REV_2;
	prm->ndis_hdr.ndis_size = sizeof(*prm);
	prm->ndis_flags = flags;
	prm->ndis_hash = hv->rss_hash;
	prm->ndis_indsize = sizeof(rssp.rss_ind[0]) * NDIS_HASH_INDCNT;
	prm->ndis_indoffset = offsetof(struct ndis_rssprm_toeplitz, rss_ind[0]);
	prm->ndis_keysize = NDIS_HASH_KEYSIZE_TOEPLITZ;
	prm->ndis_keyoffset = offsetof(struct ndis_rssprm_toeplitz, rss_key[0]);

	for (i = 0; i < NDIS_HASH_INDCNT; i++)
		rssp.rss_ind[i] = hv->rss_ind[i];

	/* Set hask key values */
	memcpy(&rssp.rss_key, hv->rss_key, NDIS_HASH_KEYSIZE_TOEPLITZ);

	error = hn_rndis_set(hv, OID_GEN_RECEIVE_SCALE_PARAMETERS,
			     &rssp, sizeof(rssp));
	if (error != 0) {
		PMD_DRV_LOG(ERR,
			    "RSS config num queues=%u failed: %d",
			    hv->num_queues, error);
	}
	return error;
}

static int hn_rndis_init(struct hn_data *hv)
{
	struct rndis_init_req *req;
	struct rndis_init_comp comp;
	uint32_t comp_len, rid;
	int error;

	req = hn_rndis_alloc(sizeof(*req));
	if (!req) {
		PMD_DRV_LOG(ERR, "no memory for RNDIS init");
		return -ENXIO;
	}

	rid = hn_rndis_rid(hv);
	req->type = RNDIS_INITIALIZE_MSG;
	req->len = sizeof(*req);
	req->rid = rid;
	req->ver_major = RNDIS_VERSION_MAJOR;
	req->ver_minor = RNDIS_VERSION_MINOR;
	req->max_xfersz = HN_RNDIS_XFER_SIZE;

	comp_len = RNDIS_INIT_COMP_SIZE_MIN;
	error = hn_rndis_execute(hv, rid, req, sizeof(*req),
				 &comp, comp_len,
				 RNDIS_INITIALIZE_CMPLT);
	if (error)
		goto done;

	if (comp.status != RNDIS_STATUS_SUCCESS) {
		PMD_DRV_LOG(ERR, "RNDIS init failed: status 0x%08x",
			    comp.status);
		error = -EIO;
		goto done;
	}

	hv->rndis_agg_size = comp.pktmaxsz;
	hv->rndis_agg_pkts = comp.pktmaxcnt;
	hv->rndis_agg_align = 1U << comp.align;

	if (hv->rndis_agg_align < sizeof(uint32_t)) {
		/*
		 * The RNDIS packet message encap assumes that the RNDIS
		 * packet message is at least 4 bytes aligned.  Fix up the
		 * alignment here, if the remote side sets the alignment
		 * too low.
		 */
		PMD_DRV_LOG(NOTICE,
			    "fixup RNDIS aggpkt align: %u -> %zu",
			    hv->rndis_agg_align, sizeof(uint32_t));
		hv->rndis_agg_align = sizeof(uint32_t);
	}

	PMD_INIT_LOG(INFO,
		     "RNDIS ver %u.%u, aggpkt size %u, aggpkt cnt %u, aggpkt align %u",
		     comp.ver_major, comp.ver_minor,
		     hv->rndis_agg_size, hv->rndis_agg_pkts,
		     hv->rndis_agg_align);
	error = 0;
done:
	rte_free(req);
	return error;
}

int
hn_rndis_get_eaddr(struct hn_data *hv, uint8_t *eaddr)
{
	uint32_t eaddr_len;
	int error;

	eaddr_len = RTE_ETHER_ADDR_LEN;
	error = hn_rndis_query(hv, OID_802_3_PERMANENT_ADDRESS, NULL, 0,
			       eaddr, eaddr_len);
	if (error)
		return error;

	PMD_DRV_LOG(INFO, "MAC address %02x:%02x:%02x:%02x:%02x:%02x",
		    eaddr[0], eaddr[1], eaddr[2],
		    eaddr[3], eaddr[4], eaddr[5]);
	return 0;
}

int
hn_rndis_get_linkstatus(struct hn_data *hv)
{
	return hn_rndis_query(hv, OID_GEN_MEDIA_CONNECT_STATUS, NULL, 0,
			      &hv->link_status, sizeof(uint32_t));
}

int
hn_rndis_get_linkspeed(struct hn_data *hv)
{
	return hn_rndis_query(hv, OID_GEN_LINK_SPEED, NULL, 0,
			      &hv->link_speed, sizeof(uint32_t));
}

int
hn_rndis_attach(struct hn_data *hv)
{
	/* Initialize RNDIS. */
	return hn_rndis_init(hv);
}

void
hn_rndis_detach(struct hn_data *hv)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hv->port_id];

	rte_eal_alarm_cancel(hn_rndis_link_alarm, dev);

	/* Halt the RNDIS. */
	hn_rndis_halt(hv);
}
