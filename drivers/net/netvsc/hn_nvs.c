/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018 Microsoft Corp.
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2012 NetApp Inc.
 * All rights reserved.
 */

/*
 * Network Virtualization Service.
 */


#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ether.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <dev_driver.h>
#include <bus_vmbus_driver.h>

#include "hn_logs.h"
#include "hn_var.h"
#include "hn_nvs.h"

static const uint32_t hn_nvs_version[] = {
	NVS_VERSION_61,
	NVS_VERSION_6,
	NVS_VERSION_5,
	NVS_VERSION_4,
	NVS_VERSION_2,
	NVS_VERSION_1
};

static int hn_nvs_req_send(struct hn_data *hv,
			   void *req, uint32_t reqlen)
{
	return rte_vmbus_chan_send(hn_primary_chan(hv),
				   VMBUS_CHANPKT_TYPE_INBAND,
				   req, reqlen, 0,
				   VMBUS_CHANPKT_FLAG_NONE, NULL);
}

static int
__hn_nvs_execute(struct hn_data *hv,
	       void *req, uint32_t reqlen,
	       void *resp, uint32_t resplen,
	       uint32_t type)
{
	struct vmbus_channel *chan = hn_primary_chan(hv);
	char buffer[NVS_RESPSIZE_MAX];
	const struct hn_nvs_hdr *hdr;
	uint64_t xactid;
	uint32_t len;
	int ret;

	/* Send request to ring buffer */
	ret = rte_vmbus_chan_send(chan, VMBUS_CHANPKT_TYPE_INBAND,
				  req, reqlen, 0,
				  VMBUS_CHANPKT_FLAG_RC, NULL);

	if (ret) {
		PMD_DRV_LOG(ERR, "send request failed: %d", ret);
		return ret;
	}

 retry:
	len = sizeof(buffer);
	ret = rte_vmbus_chan_recv(chan, buffer, &len, &xactid);
	if (ret == -EAGAIN) {
		rte_delay_us(HN_CHAN_INTERVAL_US);
		goto retry;
	}

	if (ret < 0) {
		PMD_DRV_LOG(ERR, "recv response failed: %d", ret);
		return ret;
	}

	if (len < sizeof(*hdr)) {
		PMD_DRV_LOG(ERR, "response missing NVS header");
		return -EINVAL;
	}

	hdr = (struct hn_nvs_hdr *)buffer;

	/* Silently drop received packets while waiting for response */
	switch (hdr->type) {
	case NVS_TYPE_RNDIS:
		hn_nvs_ack_rxbuf(chan, xactid);
		/* fallthrough */

	case NVS_TYPE_TXTBL_NOTE:
		PMD_DRV_LOG(DEBUG, "discard packet type 0x%x", hdr->type);
		goto retry;
	}

	if (hdr->type != type) {
		PMD_DRV_LOG(ERR, "unexpected NVS resp %#x, expect %#x",
			    hdr->type, type);
		return -EINVAL;
	}

	if (len < resplen) {
		PMD_DRV_LOG(ERR,
			    "invalid NVS resp len %u (expect %u)",
			    len, resplen);
		return -EINVAL;
	}

	memcpy(resp, buffer, resplen);

	/* All pass! */
	return 0;
}


/*
 * Execute one control command and get the response.
 * Only one command can be active on a channel at once
 * Unlike BSD, DPDK does not have an interrupt context
 * so the polling is required to wait for response.
 */
static int
hn_nvs_execute(struct hn_data *hv,
	       void *req, uint32_t reqlen,
	       void *resp, uint32_t resplen,
	       uint32_t type)
{
	struct hn_rx_queue *rxq = hv->primary;
	int ret;

	rte_spinlock_lock(&rxq->ring_lock);
	ret = __hn_nvs_execute(hv, req, reqlen, resp, resplen, type);
	rte_spinlock_unlock(&rxq->ring_lock);

	return ret;
}

static int
hn_nvs_doinit(struct hn_data *hv, uint32_t nvs_ver)
{
	struct hn_nvs_init init;
	struct hn_nvs_init_resp resp;
	uint32_t status;
	int error;

	memset(&init, 0, sizeof(init));
	init.type = NVS_TYPE_INIT;
	init.ver_min = nvs_ver;
	init.ver_max = nvs_ver;

	error = hn_nvs_execute(hv, &init, sizeof(init),
			       &resp, sizeof(resp),
			       NVS_TYPE_INIT_RESP);
	if (error)
		return error;

	status = resp.status;
	if (status != NVS_STATUS_OK) {
		/* Not fatal, try other versions */
		PMD_INIT_LOG(DEBUG, "nvs init failed for ver 0x%x",
			     nvs_ver);
		return -EINVAL;
	}

	return 0;
}

static int
hn_nvs_conn_rxbuf(struct hn_data *hv)
{
	struct hn_nvs_rxbuf_conn conn;
	struct hn_nvs_rxbuf_connresp resp;
	uint32_t status;
	int error;

	/* Kernel has already setup RXBUF on primary channel. */

	/*
	 * Connect RXBUF to NVS.
	 */
	conn.type = NVS_TYPE_RXBUF_CONN;
	conn.gpadl = hv->rxbuf_res.phys_addr;
	conn.sig = NVS_RXBUF_SIG;
	PMD_DRV_LOG(DEBUG, "connect rxbuff va=%p gpad=%#" PRIx64,
		    hv->rxbuf_res.addr,
		    hv->rxbuf_res.phys_addr);

	error = hn_nvs_execute(hv, &conn, sizeof(conn),
			       &resp, sizeof(resp),
			       NVS_TYPE_RXBUF_CONNRESP);
	if (error) {
		PMD_DRV_LOG(ERR,
			    "exec nvs rxbuf conn failed: %d",
			    error);
		return error;
	}

	status = resp.status;
	if (status != NVS_STATUS_OK) {
		PMD_DRV_LOG(ERR,
			    "nvs rxbuf conn failed: %x", status);
		return -EIO;
	}
	if (resp.nsect != 1) {
		PMD_DRV_LOG(ERR,
			    "nvs rxbuf response num sections %u != 1",
			    resp.nsect);
		return -EIO;
	}

	PMD_DRV_LOG(INFO,
		    "receive buffer size %u count %u",
		    resp.nvs_sect[0].slotsz,
		    resp.nvs_sect[0].slotcnt);
	hv->rxbuf_section_cnt = resp.nvs_sect[0].slotcnt;

	/*
	 * Primary queue's rxbuf_info is not allocated at creation time.
	 * Now we can allocate it after we figure out the slotcnt.
	 */
	hv->primary->rxbuf_info = rte_calloc("HN_RXBUF_INFO",
			hv->rxbuf_section_cnt,
			sizeof(*hv->primary->rxbuf_info),
			RTE_CACHE_LINE_SIZE);
	if (!hv->primary->rxbuf_info) {
		PMD_DRV_LOG(ERR,
			    "could not allocate rxbuf info");
		return -ENOMEM;
	}

	return 0;
}

static void
hn_nvs_disconn_rxbuf(struct hn_data *hv)
{
	struct hn_nvs_rxbuf_disconn disconn;
	int error;

	/*
	 * Disconnect RXBUF from NVS.
	 */
	memset(&disconn, 0, sizeof(disconn));
	disconn.type = NVS_TYPE_RXBUF_DISCONN;
	disconn.sig = NVS_RXBUF_SIG;

	/* NOTE: No response. */
	error = hn_nvs_req_send(hv, &disconn, sizeof(disconn));
	if (error) {
		PMD_DRV_LOG(ERR,
			    "send nvs rxbuf disconn failed: %d",
			    error);
	}

	/*
	 * Linger long enough for NVS to disconnect RXBUF.
	 */
	rte_delay_ms(200);
}

static void
hn_nvs_disconn_chim(struct hn_data *hv)
{
	int error;

	if (hv->chim_cnt != 0) {
		struct hn_nvs_chim_disconn disconn;

		/* Disconnect chimney sending buffer from NVS. */
		memset(&disconn, 0, sizeof(disconn));
		disconn.type = NVS_TYPE_CHIM_DISCONN;
		disconn.sig = NVS_CHIM_SIG;

		/* NOTE: No response. */
		error = hn_nvs_req_send(hv, &disconn, sizeof(disconn));

		if (error) {
			PMD_DRV_LOG(ERR,
				    "send nvs chim disconn failed: %d", error);
		}

		hv->chim_cnt = 0;
		/*
		 * Linger long enough for NVS to disconnect chimney
		 * sending buffer.
		 */
		rte_delay_ms(200);
	}
}

static int
hn_nvs_conn_chim(struct hn_data *hv)
{
	struct hn_nvs_chim_conn chim;
	struct hn_nvs_chim_connresp resp;
	uint32_t sectsz;
	unsigned long len = hv->chim_res.len;
	int error;

	/* Connect chimney sending buffer to NVS */
	memset(&chim, 0, sizeof(chim));
	chim.type = NVS_TYPE_CHIM_CONN;
	chim.gpadl = hv->chim_res.phys_addr;
	chim.sig = NVS_CHIM_SIG;
	PMD_DRV_LOG(DEBUG, "connect send buf va=%p gpad=%#" PRIx64,
		    hv->chim_res.addr,
		    hv->chim_res.phys_addr);

	error = hn_nvs_execute(hv, &chim, sizeof(chim),
			       &resp, sizeof(resp),
			       NVS_TYPE_CHIM_CONNRESP);
	if (error) {
		PMD_DRV_LOG(ERR, "exec nvs chim conn failed");
		return error;
	}

	if (resp.status != NVS_STATUS_OK) {
		PMD_DRV_LOG(ERR, "nvs chim conn failed: %x",
			    resp.status);
		return -EIO;
	}

	sectsz = resp.sectsz;
	if (sectsz == 0 || sectsz & (sizeof(uint32_t) - 1)) {
		/* Can't use chimney sending buffer; done! */
		PMD_DRV_LOG(NOTICE,
			    "invalid chimney sending buffer section size: %u",
			    sectsz);
		error = -EINVAL;
		goto cleanup;
	}

	hv->chim_szmax = sectsz;
	hv->chim_cnt = len / sectsz;

	PMD_DRV_LOG(INFO, "send buffer %lu section size:%u, count:%u",
		    len, hv->chim_szmax, hv->chim_cnt);

	/* Done! */
	return 0;

cleanup:
	hn_nvs_disconn_chim(hv);
	return error;
}

/*
 * Configure MTU and enable VLAN.
 */
static int
hn_nvs_conf_ndis(struct hn_data *hv, unsigned int mtu)
{
	struct hn_nvs_ndis_conf conf;
	int error;

	memset(&conf, 0, sizeof(conf));
	conf.type = NVS_TYPE_NDIS_CONF;
	conf.mtu = mtu + RTE_ETHER_HDR_LEN;
	conf.caps = NVS_NDIS_CONF_VLAN;

	/* enable SRIOV */
	if (hv->nvs_ver >= NVS_VERSION_5)
		conf.caps |= NVS_NDIS_CONF_SRIOV;

	/* NOTE: No response. */
	error = hn_nvs_req_send(hv, &conf, sizeof(conf));
	if (error) {
		PMD_DRV_LOG(ERR,
			    "send nvs ndis conf failed: %d", error);
		return error;
	}

	return 0;
}

static int
hn_nvs_init_ndis(struct hn_data *hv)
{
	struct hn_nvs_ndis_init ndis;
	int error;

	memset(&ndis, 0, sizeof(ndis));
	ndis.type = NVS_TYPE_NDIS_INIT;
	ndis.ndis_major = NDIS_VERSION_MAJOR(hv->ndis_ver);
	ndis.ndis_minor = NDIS_VERSION_MINOR(hv->ndis_ver);

	/* NOTE: No response. */
	error = hn_nvs_req_send(hv, &ndis, sizeof(ndis));
	if (error)
		PMD_DRV_LOG(ERR,
			    "send nvs ndis init failed: %d", error);

	return error;
}

static int
hn_nvs_init(struct hn_data *hv)
{
	unsigned int i;
	int error;

	/*
	 * Find the supported NVS version and set NDIS version accordingly.
	 */
	for (i = 0; i < RTE_DIM(hn_nvs_version); ++i) {
		error = hn_nvs_doinit(hv, hn_nvs_version[i]);
		if (error) {
			PMD_INIT_LOG(DEBUG, "version %#x error %d",
				     hn_nvs_version[i], error);
			continue;
		}

		hv->nvs_ver = hn_nvs_version[i];

		/* Set NDIS version according to NVS version. */
		hv->ndis_ver = NDIS_VERSION_6_30;
		if (hv->nvs_ver <= NVS_VERSION_4)
			hv->ndis_ver = NDIS_VERSION_6_1;

		PMD_INIT_LOG(DEBUG,
			     "NVS version %#x, NDIS version %u.%u",
			     hv->nvs_ver, NDIS_VERSION_MAJOR(hv->ndis_ver),
			     NDIS_VERSION_MINOR(hv->ndis_ver));
		return 0;
	}

	PMD_DRV_LOG(ERR,
		    "no NVS compatible version available");
	return -ENXIO;
}

int
hn_nvs_attach(struct hn_data *hv, unsigned int mtu)
{
	int error;

	/*
	 * Initialize NVS.
	 */
	error = hn_nvs_init(hv);
	if (error)
		return error;

	/** Configure NDIS before initializing it. */
	if (hv->nvs_ver >= NVS_VERSION_2) {
		error = hn_nvs_conf_ndis(hv, mtu);
		if (error)
			return error;
	}

	/*
	 * Initialize NDIS.
	 */
	error = hn_nvs_init_ndis(hv);
	if (error)
		return error;

	/*
	 * Connect RXBUF.
	 */
	error = hn_nvs_conn_rxbuf(hv);
	if (error)
		return error;

	/*
	 * Connect chimney sending buffer.
	 */
	error = hn_nvs_conn_chim(hv);
	if (error) {
		hn_nvs_disconn_rxbuf(hv);
		return error;
	}

	return 0;
}

void
hn_nvs_detach(struct hn_data *hv __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	/* NOTE: there are no requests to stop the NVS. */
	hn_nvs_disconn_rxbuf(hv);
	hn_nvs_disconn_chim(hv);
}

/*
 * Ack the consumed RXBUF associated w/ this channel packet,
 * so that this RXBUF can be recycled by the hypervisor.
 */
void
hn_nvs_ack_rxbuf(struct vmbus_channel *chan, uint64_t tid)
{
	unsigned int retries = 0;
	struct hn_nvs_rndis_ack ack = {
		.type = NVS_TYPE_RNDIS_ACK,
		.status = NVS_STATUS_OK,
	};
	int error;

	PMD_RX_LOG(DEBUG, "ack RX id %" PRIu64, tid);

 again:
	error = rte_vmbus_chan_send(chan, VMBUS_CHANPKT_TYPE_COMP,
				    &ack, sizeof(ack), tid,
				    VMBUS_CHANPKT_FLAG_NONE, NULL);

	if (error == 0)
		return;

	if (error == -EAGAIN) {
		/*
		 * NOTE:
		 * This should _not_ happen in real world, since the
		 * consumption of the TX bufring from the TX path is
		 * controlled.
		 */
		PMD_RX_LOG(NOTICE, "RXBUF ack retry");
		if (++retries < 10) {
			rte_delay_ms(1);
			goto again;
		}
	}
	/* RXBUF leaks! */
	PMD_DRV_LOG(ERR, "RXBUF ack failed");
}

int
hn_nvs_alloc_subchans(struct hn_data *hv, uint32_t *nsubch)
{
	struct hn_nvs_subch_req req;
	struct hn_nvs_subch_resp resp;
	int error;

	memset(&req, 0, sizeof(req));
	req.type = NVS_TYPE_SUBCH_REQ;
	req.op = NVS_SUBCH_OP_ALLOC;
	req.nsubch = *nsubch;

	error = hn_nvs_execute(hv, &req, sizeof(req),
			       &resp, sizeof(resp),
			       NVS_TYPE_SUBCH_RESP);
	if (error)
		return error;

	if (resp.status != NVS_STATUS_OK) {
		PMD_INIT_LOG(ERR,
			     "nvs subch alloc failed: %#x",
			     resp.status);
		return -EIO;
	}

	if (resp.nsubch > *nsubch) {
		PMD_INIT_LOG(NOTICE,
			     "%u subchans are allocated, requested %u",
			     resp.nsubch, *nsubch);
	}
	*nsubch = resp.nsubch;

	return 0;
}

int
hn_nvs_set_datapath(struct hn_data *hv, uint32_t path)
{
	struct hn_nvs_datapath dp;
	int error;

	PMD_DRV_LOG(DEBUG, "set datapath %s",
		    path ? "VF" : "Synthetic");

	memset(&dp, 0, sizeof(dp));
	dp.type = NVS_TYPE_SET_DATAPATH;
	dp.active_path = path;

	error = hn_nvs_req_send(hv, &dp, sizeof(dp));
	if (error) {
		PMD_DRV_LOG(ERR,
			    "send set datapath failed: %d",
			    error);
	}

	return error;
}
