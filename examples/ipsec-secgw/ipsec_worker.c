/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 * Copyright (C) 2020 Marvell International Ltd.
 */
#include <rte_acl.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>

#include "event_helper.h"
#include "ipsec.h"
#include "ipsec-secgw.h"
#include "ipsec_worker.h"

struct port_drv_mode_data {
	struct rte_security_session *sess;
	struct rte_security_ctx *ctx;
};

static inline enum pkt_type
process_ipsec_get_pkt_type(struct rte_mbuf *pkt, uint8_t **nlp)
{
	struct rte_ether_hdr *eth;
	uint32_t ptype = pkt->packet_type;

	eth = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	rte_prefetch0(eth);

	if (RTE_ETH_IS_IPV4_HDR(ptype)) {
		*nlp = RTE_PTR_ADD(eth, RTE_ETHER_HDR_LEN +
				offsetof(struct ip, ip_p));
		if ((ptype & RTE_PTYPE_TUNNEL_MASK) == RTE_PTYPE_TUNNEL_ESP)
			return PKT_TYPE_IPSEC_IPV4;
		else
			return PKT_TYPE_PLAIN_IPV4;
	} else if (RTE_ETH_IS_IPV6_HDR(ptype)) {
		*nlp = RTE_PTR_ADD(eth, RTE_ETHER_HDR_LEN +
				offsetof(struct ip6_hdr, ip6_nxt));
		if ((ptype & RTE_PTYPE_TUNNEL_MASK) == RTE_PTYPE_TUNNEL_ESP)
			return PKT_TYPE_IPSEC_IPV6;
		else
			return PKT_TYPE_PLAIN_IPV6;
	}

	/* Unknown/Unsupported type */
	return PKT_TYPE_INVALID;
}

static inline void
update_mac_addrs(struct rte_mbuf *pkt, uint16_t portid)
{
	struct rte_ether_hdr *ethhdr;

	ethhdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	memcpy(&ethhdr->src_addr, &ethaddr_tbl[portid].src, RTE_ETHER_ADDR_LEN);
	memcpy(&ethhdr->dst_addr, &ethaddr_tbl[portid].dst, RTE_ETHER_ADDR_LEN);
}

static inline void
ipsec_event_pre_forward(struct rte_mbuf *m, unsigned int port_id)
{
	/* Save the destination port in the mbuf */
	m->port = port_id;

	/* Save eth queue for Tx */
	rte_event_eth_tx_adapter_txq_set(m, 0);
}

static inline void
ev_vector_attr_init(struct rte_event_vector *vec)
{
	vec->attr_valid = 1;
	vec->port = 0xFFFF;
	vec->queue = 0;
}

static inline void
ev_vector_attr_update(struct rte_event_vector *vec, struct rte_mbuf *pkt)
{
	if (vec->port == 0xFFFF) {
		vec->port = pkt->port;
		return;
	}
	if (vec->attr_valid && (vec->port != pkt->port))
		vec->attr_valid = 0;
}

static inline void
prepare_out_sessions_tbl(struct sa_ctx *sa_out,
			 struct port_drv_mode_data *data,
			 uint16_t size)
{
	struct rte_ipsec_session *pri_sess;
	struct ipsec_sa *sa;
	uint32_t i;

	if (!sa_out)
		return;

	for (i = 0; i < sa_out->nb_sa; i++) {

		sa = &sa_out->sa[i];
		if (!sa)
			continue;

		pri_sess = ipsec_get_primary_session(sa);
		if (!pri_sess)
			continue;

		if (pri_sess->type !=
			RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL) {

			RTE_LOG(ERR, IPSEC, "Invalid session type %d\n",
				pri_sess->type);
			continue;
		}

		if (sa->portid >= size) {
			RTE_LOG(ERR, IPSEC,
				"Port id >= than table size %d, %d\n",
				sa->portid, size);
			continue;
		}

		/* Use only first inline session found for a given port */
		if (data[sa->portid].sess)
			continue;
		data[sa->portid].sess = pri_sess->security.ses;
		data[sa->portid].ctx = pri_sess->security.ctx;
	}
}

static inline int
check_sp(struct sp_ctx *sp, const uint8_t *nlp, uint32_t *sa_idx)
{
	uint32_t res;

	if (unlikely(sp == NULL))
		return 0;

	rte_acl_classify((struct rte_acl_ctx *)sp, &nlp, &res, 1,
			DEFAULT_MAX_CATEGORIES);

	if (unlikely(res == DISCARD))
		return 0;
	else if (res == BYPASS) {
		*sa_idx = -1;
		return 1;
	}

	*sa_idx = res - 1;
	return 1;
}

static inline void
check_sp_bulk(struct sp_ctx *sp, struct traffic_type *ip,
	      struct traffic_type *ipsec)
{
	uint32_t i, j, res;
	struct rte_mbuf *m;

	if (unlikely(sp == NULL || ip->num == 0))
		return;

	rte_acl_classify((struct rte_acl_ctx *)sp, ip->data, ip->res, ip->num,
			 DEFAULT_MAX_CATEGORIES);

	j = 0;
	for (i = 0; i < ip->num; i++) {
		m = ip->pkts[i];
		res = ip->res[i];
		if (unlikely(res == DISCARD))
			free_pkts(&m, 1);
		else if (res == BYPASS)
			ip->pkts[j++] = m;
		else {
			ipsec->res[ipsec->num] = res - 1;
			ipsec->pkts[ipsec->num++] = m;
		}
	}
	ip->num = j;
}

static inline void
check_sp_sa_bulk(struct sp_ctx *sp, struct sa_ctx *sa_ctx,
		 struct traffic_type *ip)
{
	struct ipsec_sa *sa;
	uint32_t i, j, res;
	struct rte_mbuf *m;

	if (unlikely(sp == NULL || ip->num == 0))
		return;

	rte_acl_classify((struct rte_acl_ctx *)sp, ip->data, ip->res, ip->num,
			 DEFAULT_MAX_CATEGORIES);

	j = 0;
	for (i = 0; i < ip->num; i++) {
		m = ip->pkts[i];
		res = ip->res[i];
		if (unlikely(res == DISCARD))
			free_pkts(&m, 1);
		else if (res == BYPASS)
			ip->pkts[j++] = m;
		else {
			sa = *(struct ipsec_sa **)rte_security_dynfield(m);
			if (sa == NULL) {
				free_pkts(&m, 1);
				continue;
			}

			/* SPI on the packet should match with the one in SA */
			if (unlikely(sa->spi != sa_ctx->sa[res - 1].spi)) {
				free_pkts(&m, 1);
				continue;
			}

			ip->pkts[j++] = m;
		}
	}
	ip->num = j;
}

static inline uint16_t
route4_pkt(struct rte_mbuf *pkt, struct rt_ctx *rt_ctx)
{
	uint32_t dst_ip;
	uint16_t offset;
	uint32_t hop;
	int ret;

	offset = RTE_ETHER_HDR_LEN + offsetof(struct ip, ip_dst);
	dst_ip = *rte_pktmbuf_mtod_offset(pkt, uint32_t *, offset);
	dst_ip = rte_be_to_cpu_32(dst_ip);

	ret = rte_lpm_lookup((struct rte_lpm *)rt_ctx, dst_ip, &hop);

	if (ret == 0) {
		/* We have a hit */
		return hop;
	}

	/* else */
	return RTE_MAX_ETHPORTS;
}

/* TODO: To be tested */
static inline uint16_t
route6_pkt(struct rte_mbuf *pkt, struct rt_ctx *rt_ctx)
{
	uint8_t dst_ip[16];
	uint8_t *ip6_dst;
	uint16_t offset;
	uint32_t hop;
	int ret;

	offset = RTE_ETHER_HDR_LEN + offsetof(struct ip6_hdr, ip6_dst);
	ip6_dst = rte_pktmbuf_mtod_offset(pkt, uint8_t *, offset);
	memcpy(&dst_ip[0], ip6_dst, 16);

	ret = rte_lpm6_lookup((struct rte_lpm6 *)rt_ctx, dst_ip, &hop);

	if (ret == 0) {
		/* We have a hit */
		return hop;
	}

	/* else */
	return RTE_MAX_ETHPORTS;
}

static inline uint16_t
get_route(struct rte_mbuf *pkt, struct route_table *rt, enum pkt_type type)
{
	if (type == PKT_TYPE_PLAIN_IPV4 || type == PKT_TYPE_IPSEC_IPV4)
		return route4_pkt(pkt, rt->rt4_ctx);
	else if (type == PKT_TYPE_PLAIN_IPV6 || type == PKT_TYPE_IPSEC_IPV6)
		return route6_pkt(pkt, rt->rt6_ctx);

	return RTE_MAX_ETHPORTS;
}

static inline int
process_ipsec_ev_inbound(struct ipsec_ctx *ctx, struct route_table *rt,
		struct rte_event *ev)
{
	struct ipsec_sa *sa = NULL;
	struct rte_mbuf *pkt;
	uint16_t port_id = 0;
	enum pkt_type type;
	uint32_t sa_idx;
	uint8_t *nlp;

	/* Get pkt from event */
	pkt = ev->mbuf;

	/* Check the packet type */
	type = process_ipsec_get_pkt_type(pkt, &nlp);

	switch (type) {
	case PKT_TYPE_PLAIN_IPV4:
		if (pkt->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD) {
			if (unlikely(pkt->ol_flags &
				     RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED)) {
				RTE_LOG(ERR, IPSEC,
					"Inbound security offload failed\n");
				goto drop_pkt_and_exit;
			}
			sa = *(struct ipsec_sa **)rte_security_dynfield(pkt);
		}

		/* Check if we have a match */
		if (check_sp(ctx->sp4_ctx, nlp, &sa_idx) == 0) {
			/* No valid match */
			goto drop_pkt_and_exit;
		}
		break;

	case PKT_TYPE_PLAIN_IPV6:
		if (pkt->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD) {
			if (unlikely(pkt->ol_flags &
				     RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED)) {
				RTE_LOG(ERR, IPSEC,
					"Inbound security offload failed\n");
				goto drop_pkt_and_exit;
			}
			sa = *(struct ipsec_sa **)rte_security_dynfield(pkt);
		}

		/* Check if we have a match */
		if (check_sp(ctx->sp6_ctx, nlp, &sa_idx) == 0) {
			/* No valid match */
			goto drop_pkt_and_exit;
		}
		break;

	default:
		RTE_LOG(ERR, IPSEC, "Unsupported packet type = %d\n", type);
		goto drop_pkt_and_exit;
	}

	/* Check if the packet has to be bypassed */
	if (sa_idx == BYPASS)
		goto route_and_send_pkt;

	/* Validate sa_idx */
	if (sa_idx >= ctx->sa_ctx->nb_sa)
		goto drop_pkt_and_exit;

	/* Else the packet has to be protected with SA */

	/* If the packet was IPsec processed, then SA pointer should be set */
	if (sa == NULL)
		goto drop_pkt_and_exit;

	/* SPI on the packet should match with the one in SA */
	if (unlikely(sa->spi != ctx->sa_ctx->sa[sa_idx].spi))
		goto drop_pkt_and_exit;

route_and_send_pkt:
	port_id = get_route(pkt, rt, type);
	if (unlikely(port_id == RTE_MAX_ETHPORTS)) {
		/* no match */
		goto drop_pkt_and_exit;
	}
	/* else, we have a matching route */

	/* Update mac addresses */
	update_mac_addrs(pkt, port_id);

	/* Update the event with the dest port */
	ipsec_event_pre_forward(pkt, port_id);
	return PKT_FORWARDED;

drop_pkt_and_exit:
	RTE_LOG(ERR, IPSEC, "Inbound packet dropped\n");
	rte_pktmbuf_free(pkt);
	ev->mbuf = NULL;
	return PKT_DROPPED;
}

static inline int
process_ipsec_ev_outbound(struct ipsec_ctx *ctx, struct route_table *rt,
		struct rte_event *ev)
{
	struct rte_ipsec_session *sess;
	struct sa_ctx *sa_ctx;
	struct rte_mbuf *pkt;
	uint16_t port_id = 0;
	struct ipsec_sa *sa;
	enum pkt_type type;
	uint32_t sa_idx;
	uint8_t *nlp;

	/* Get pkt from event */
	pkt = ev->mbuf;

	/* Check the packet type */
	type = process_ipsec_get_pkt_type(pkt, &nlp);

	switch (type) {
	case PKT_TYPE_PLAIN_IPV4:
		/* Check if we have a match */
		if (check_sp(ctx->sp4_ctx, nlp, &sa_idx) == 0) {
			/* No valid match */
			goto drop_pkt_and_exit;
		}
		break;
	case PKT_TYPE_PLAIN_IPV6:
		/* Check if we have a match */
		if (check_sp(ctx->sp6_ctx, nlp, &sa_idx) == 0) {
			/* No valid match */
			goto drop_pkt_and_exit;
		}
		break;
	default:
		/*
		 * Only plain IPv4 & IPv6 packets are allowed
		 * on protected port. Drop the rest.
		 */
		RTE_LOG(ERR, IPSEC, "Unsupported packet type = %d\n", type);
		goto drop_pkt_and_exit;
	}

	/* Check if the packet has to be bypassed */
	if (sa_idx == BYPASS) {
		port_id = get_route(pkt, rt, type);
		if (unlikely(port_id == RTE_MAX_ETHPORTS)) {
			/* no match */
			goto drop_pkt_and_exit;
		}
		/* else, we have a matching route */
		goto send_pkt;
	}

	/* Validate sa_idx */
	if (unlikely(sa_idx >= ctx->sa_ctx->nb_sa))
		goto drop_pkt_and_exit;

	/* Else the packet has to be protected */

	/* Get SA ctx*/
	sa_ctx = ctx->sa_ctx;

	/* Get SA */
	sa = &(sa_ctx->sa[sa_idx]);

	/* Get IPsec session */
	sess = ipsec_get_primary_session(sa);

	/* Allow only inline protocol for now */
	if (unlikely(sess->type != RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)) {
		RTE_LOG(ERR, IPSEC, "SA type not supported\n");
		goto drop_pkt_and_exit;
	}

	rte_security_set_pkt_metadata(sess->security.ctx,
				      sess->security.ses, pkt, NULL);

	/* Mark the packet for Tx security offload */
	pkt->ol_flags |= RTE_MBUF_F_TX_SEC_OFFLOAD;

	/* Get the port to which this pkt need to be submitted */
	port_id = sa->portid;

send_pkt:
	/* Provide L2 len for Outbound processing */
	pkt->l2_len = RTE_ETHER_HDR_LEN;

	/* Update mac addresses */
	update_mac_addrs(pkt, port_id);

	/* Update the event with the dest port */
	ipsec_event_pre_forward(pkt, port_id);
	return PKT_FORWARDED;

drop_pkt_and_exit:
	RTE_LOG(ERR, IPSEC, "Outbound packet dropped\n");
	rte_pktmbuf_free(pkt);
	ev->mbuf = NULL;
	return PKT_DROPPED;
}

static inline int
ipsec_ev_route_pkts(struct rte_event_vector *vec, struct route_table *rt,
		    struct ipsec_traffic *t, struct sa_ctx *sa_ctx)
{
	struct rte_ipsec_session *sess;
	uint32_t sa_idx, i, j = 0;
	uint16_t port_id = 0;
	struct rte_mbuf *pkt;
	struct ipsec_sa *sa;

	/* Route IPv4 packets */
	for (i = 0; i < t->ip4.num; i++) {
		pkt = t->ip4.pkts[i];
		port_id = route4_pkt(pkt, rt->rt4_ctx);
		if (port_id != RTE_MAX_ETHPORTS) {
			/* Update mac addresses */
			update_mac_addrs(pkt, port_id);
			/* Update the event with the dest port */
			ipsec_event_pre_forward(pkt, port_id);
			ev_vector_attr_update(vec, pkt);
			vec->mbufs[j++] = pkt;
		} else
			free_pkts(&pkt, 1);
	}

	/* Route IPv6 packets */
	for (i = 0; i < t->ip6.num; i++) {
		pkt = t->ip6.pkts[i];
		port_id = route6_pkt(pkt, rt->rt6_ctx);
		if (port_id != RTE_MAX_ETHPORTS) {
			/* Update mac addresses */
			update_mac_addrs(pkt, port_id);
			/* Update the event with the dest port */
			ipsec_event_pre_forward(pkt, port_id);
			ev_vector_attr_update(vec, pkt);
			vec->mbufs[j++] = pkt;
		} else
			free_pkts(&pkt, 1);
	}

	/* Route ESP packets */
	for (i = 0; i < t->ipsec.num; i++) {
		/* Validate sa_idx */
		sa_idx = t->ipsec.res[i];
		pkt = t->ipsec.pkts[i];
		if (unlikely(sa_idx >= sa_ctx->nb_sa))
			free_pkts(&pkt, 1);
		else {
			/* Else the packet has to be protected */
			sa = &(sa_ctx->sa[sa_idx]);
			/* Get IPsec session */
			sess = ipsec_get_primary_session(sa);
			/* Allow only inline protocol for now */
			if (unlikely(sess->type !=
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)) {
				RTE_LOG(ERR, IPSEC, "SA type not supported\n");
				free_pkts(&pkt, 1);
				continue;
			}
			rte_security_set_pkt_metadata(sess->security.ctx,
						sess->security.ses, pkt, NULL);

			pkt->ol_flags |= RTE_MBUF_F_TX_SEC_OFFLOAD;
			port_id = sa->portid;
			update_mac_addrs(pkt, port_id);
			ipsec_event_pre_forward(pkt, port_id);
			ev_vector_attr_update(vec, pkt);
			vec->mbufs[j++] = pkt;
		}
	}

	return j;
}

static inline void
classify_pkt(struct rte_mbuf *pkt, struct ipsec_traffic *t)
{
	enum pkt_type type;
	uint8_t *nlp;

	/* Check the packet type */
	type = process_ipsec_get_pkt_type(pkt, &nlp);

	switch (type) {
	case PKT_TYPE_PLAIN_IPV4:
		t->ip4.data[t->ip4.num] = nlp;
		t->ip4.pkts[(t->ip4.num)++] = pkt;
		break;
	case PKT_TYPE_PLAIN_IPV6:
		t->ip6.data[t->ip6.num] = nlp;
		t->ip6.pkts[(t->ip6.num)++] = pkt;
		break;
	default:
		RTE_LOG(ERR, IPSEC, "Unsupported packet type = %d\n", type);
		free_pkts(&pkt, 1);
		break;
	}
}

static inline int
process_ipsec_ev_inbound_vector(struct ipsec_ctx *ctx, struct route_table *rt,
				struct rte_event_vector *vec)
{
	struct ipsec_traffic t;
	struct rte_mbuf *pkt;
	int i;

	t.ip4.num = 0;
	t.ip6.num = 0;
	t.ipsec.num = 0;

	for (i = 0; i < vec->nb_elem; i++) {
		/* Get pkt from event */
		pkt = vec->mbufs[i];

		if (pkt->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD) {
			if (unlikely(pkt->ol_flags &
				     RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED)) {
				RTE_LOG(ERR, IPSEC,
					"Inbound security offload failed\n");
				free_pkts(&pkt, 1);
				continue;
			}
		}

		classify_pkt(pkt, &t);
	}

	check_sp_sa_bulk(ctx->sp4_ctx, ctx->sa_ctx, &t.ip4);
	check_sp_sa_bulk(ctx->sp6_ctx, ctx->sa_ctx, &t.ip6);

	return ipsec_ev_route_pkts(vec, rt, &t, ctx->sa_ctx);
}

static inline int
process_ipsec_ev_outbound_vector(struct ipsec_ctx *ctx, struct route_table *rt,
				 struct rte_event_vector *vec)
{
	struct ipsec_traffic t;
	struct rte_mbuf *pkt;
	uint32_t i;

	t.ip4.num = 0;
	t.ip6.num = 0;
	t.ipsec.num = 0;

	for (i = 0; i < vec->nb_elem; i++) {
		/* Get pkt from event */
		pkt = vec->mbufs[i];

		classify_pkt(pkt, &t);

		/* Provide L2 len for Outbound processing */
		pkt->l2_len = RTE_ETHER_HDR_LEN;
	}

	check_sp_bulk(ctx->sp4_ctx, &t.ip4, &t.ipsec);
	check_sp_bulk(ctx->sp6_ctx, &t.ip6, &t.ipsec);

	return ipsec_ev_route_pkts(vec, rt, &t, ctx->sa_ctx);
}

static inline int
process_ipsec_ev_drv_mode_outbound_vector(struct rte_event_vector *vec,
					  struct port_drv_mode_data *data)
{
	struct rte_mbuf *pkt;
	int16_t port_id;
	uint32_t i;
	int j = 0;

	for (i = 0; i < vec->nb_elem; i++) {
		pkt = vec->mbufs[i];
		port_id = pkt->port;

		if (unlikely(!data[port_id].sess)) {
			free_pkts(&pkt, 1);
			continue;
		}
		ipsec_event_pre_forward(pkt, port_id);
		/* Save security session */
		rte_security_set_pkt_metadata(data[port_id].ctx,
					      data[port_id].sess, pkt,
					      NULL);

		/* Mark the packet for Tx security offload */
		pkt->ol_flags |= RTE_MBUF_F_TX_SEC_OFFLOAD;

		/* Provide L2 len for Outbound processing */
		pkt->l2_len = RTE_ETHER_HDR_LEN;

		vec->mbufs[j++] = pkt;
	}

	return j;
}

static inline void
ipsec_ev_vector_process(struct lcore_conf_ev_tx_int_port_wrkr *lconf,
			struct eh_event_link_info *links,
			struct rte_event *ev)
{
	struct rte_event_vector *vec = ev->vec;
	struct rte_mbuf *pkt;
	int ret;

	pkt = vec->mbufs[0];

	ev_vector_attr_init(vec);
	if (is_unprotected_port(pkt->port))
		ret = process_ipsec_ev_inbound_vector(&lconf->inbound,
						      &lconf->rt, vec);
	else
		ret = process_ipsec_ev_outbound_vector(&lconf->outbound,
						       &lconf->rt, vec);

	if (likely(ret > 0)) {
		vec->nb_elem = ret;
		rte_event_eth_tx_adapter_enqueue(links[0].eventdev_id,
						 links[0].event_port_id,
						 ev, 1, 0);
	} else {
		rte_mempool_put(rte_mempool_from_obj(vec), vec);
	}
}

static inline void
ipsec_ev_vector_drv_mode_process(struct eh_event_link_info *links,
				 struct rte_event *ev,
				 struct port_drv_mode_data *data)
{
	struct rte_event_vector *vec = ev->vec;
	struct rte_mbuf *pkt;

	pkt = vec->mbufs[0];

	if (!is_unprotected_port(pkt->port))
		vec->nb_elem = process_ipsec_ev_drv_mode_outbound_vector(vec,
									 data);
	if (vec->nb_elem > 0)
		rte_event_eth_tx_adapter_enqueue(links[0].eventdev_id,
						 links[0].event_port_id,
						 ev, 1, 0);
	else
		rte_mempool_put(rte_mempool_from_obj(vec), vec);
}

/*
 * Event mode exposes various operating modes depending on the
 * capabilities of the event device and the operating mode
 * selected.
 */

/* Workers registered */
#define IPSEC_EVENTMODE_WORKERS		2

/*
 * Event mode worker
 * Operating parameters : non-burst - Tx internal port - driver mode
 */
static void
ipsec_wrkr_non_burst_int_port_drv_mode(struct eh_event_link_info *links,
		uint8_t nb_links)
{
	struct port_drv_mode_data data[RTE_MAX_ETHPORTS];
	unsigned int nb_rx = 0;
	struct rte_mbuf *pkt;
	struct rte_event ev;
	uint32_t lcore_id;
	int32_t socket_id;
	int16_t port_id;

	/* Check if we have links registered for this lcore */
	if (nb_links == 0) {
		/* No links registered - exit */
		return;
	}

	memset(&data, 0, sizeof(struct port_drv_mode_data));

	/* Get core ID */
	lcore_id = rte_lcore_id();

	/* Get socket ID */
	socket_id = rte_lcore_to_socket_id(lcore_id);

	/*
	 * Prepare security sessions table. In outbound driver mode
	 * we always use first session configured for a given port
	 */
	prepare_out_sessions_tbl(socket_ctx[socket_id].sa_out, data,
				 RTE_MAX_ETHPORTS);

	RTE_LOG(INFO, IPSEC,
		"Launching event mode worker (non-burst - Tx internal port - "
		"driver mode) on lcore %d\n", lcore_id);

	/* We have valid links */

	/* Check if it's single link */
	if (nb_links != 1) {
		RTE_LOG(INFO, IPSEC,
			"Multiple links not supported. Using first link\n");
	}

	RTE_LOG(INFO, IPSEC, " -- lcoreid=%u event_port_id=%u\n", lcore_id,
			links[0].event_port_id);
	while (!force_quit) {
		/* Read packet from event queues */
		nb_rx = rte_event_dequeue_burst(links[0].eventdev_id,
				links[0].event_port_id,
				&ev,	/* events */
				1,	/* nb_events */
				0	/* timeout_ticks */);

		if (nb_rx == 0)
			continue;

		switch (ev.event_type) {
		case RTE_EVENT_TYPE_ETH_RX_ADAPTER_VECTOR:
		case RTE_EVENT_TYPE_ETHDEV_VECTOR:
			ipsec_ev_vector_drv_mode_process(links, &ev, data);
			continue;
		case RTE_EVENT_TYPE_ETHDEV:
			break;
		default:
			RTE_LOG(ERR, IPSEC, "Invalid event type %u",
				ev.event_type);
			continue;
		}

		pkt = ev.mbuf;
		port_id = pkt->port;

		rte_prefetch0(rte_pktmbuf_mtod(pkt, void *));

		/* Process packet */
		ipsec_event_pre_forward(pkt, port_id);

		if (!is_unprotected_port(port_id)) {

			if (unlikely(!data[port_id].sess)) {
				rte_pktmbuf_free(pkt);
				continue;
			}

			/* Save security session */
			rte_security_set_pkt_metadata(data[port_id].ctx,
						      data[port_id].sess, pkt,
						      NULL);

			/* Mark the packet for Tx security offload */
			pkt->ol_flags |= RTE_MBUF_F_TX_SEC_OFFLOAD;

			/* Provide L2 len for Outbound processing */
			pkt->l2_len = RTE_ETHER_HDR_LEN;
		}

		/*
		 * Since tx internal port is available, events can be
		 * directly enqueued to the adapter and it would be
		 * internally submitted to the eth device.
		 */
		rte_event_eth_tx_adapter_enqueue(links[0].eventdev_id,
				links[0].event_port_id,
				&ev,	/* events */
				1,	/* nb_events */
				0	/* flags */);
	}
}

/*
 * Event mode worker
 * Operating parameters : non-burst - Tx internal port - app mode
 */
static void
ipsec_wrkr_non_burst_int_port_app_mode(struct eh_event_link_info *links,
		uint8_t nb_links)
{
	struct lcore_conf_ev_tx_int_port_wrkr lconf;
	unsigned int nb_rx = 0;
	struct rte_event ev;
	uint32_t lcore_id;
	int32_t socket_id;
	int ret;

	/* Check if we have links registered for this lcore */
	if (nb_links == 0) {
		/* No links registered - exit */
		return;
	}

	/* We have valid links */

	/* Get core ID */
	lcore_id = rte_lcore_id();

	/* Get socket ID */
	socket_id = rte_lcore_to_socket_id(lcore_id);

	/* Save routing table */
	lconf.rt.rt4_ctx = socket_ctx[socket_id].rt_ip4;
	lconf.rt.rt6_ctx = socket_ctx[socket_id].rt_ip6;
	lconf.inbound.sp4_ctx = socket_ctx[socket_id].sp_ip4_in;
	lconf.inbound.sp6_ctx = socket_ctx[socket_id].sp_ip6_in;
	lconf.inbound.sa_ctx = socket_ctx[socket_id].sa_in;
	lconf.inbound.session_pool = socket_ctx[socket_id].session_pool;
	lconf.inbound.session_priv_pool =
			socket_ctx[socket_id].session_priv_pool;
	lconf.outbound.sp4_ctx = socket_ctx[socket_id].sp_ip4_out;
	lconf.outbound.sp6_ctx = socket_ctx[socket_id].sp_ip6_out;
	lconf.outbound.sa_ctx = socket_ctx[socket_id].sa_out;
	lconf.outbound.session_pool = socket_ctx[socket_id].session_pool;
	lconf.outbound.session_priv_pool =
			socket_ctx[socket_id].session_priv_pool;

	RTE_LOG(INFO, IPSEC,
		"Launching event mode worker (non-burst - Tx internal port - "
		"app mode) on lcore %d\n", lcore_id);

	/* Check if it's single link */
	if (nb_links != 1) {
		RTE_LOG(INFO, IPSEC,
			"Multiple links not supported. Using first link\n");
	}

	RTE_LOG(INFO, IPSEC, " -- lcoreid=%u event_port_id=%u\n", lcore_id,
		links[0].event_port_id);

	while (!force_quit) {
		/* Read packet from event queues */
		nb_rx = rte_event_dequeue_burst(links[0].eventdev_id,
				links[0].event_port_id,
				&ev,     /* events */
				1,       /* nb_events */
				0        /* timeout_ticks */);

		if (nb_rx == 0)
			continue;

		switch (ev.event_type) {
		case RTE_EVENT_TYPE_ETH_RX_ADAPTER_VECTOR:
		case RTE_EVENT_TYPE_ETHDEV_VECTOR:
			ipsec_ev_vector_process(&lconf, links, &ev);
			continue;
		case RTE_EVENT_TYPE_ETHDEV:
			break;
		default:
			RTE_LOG(ERR, IPSEC, "Invalid event type %u",
				ev.event_type);
			continue;
		}

		if (is_unprotected_port(ev.mbuf->port))
			ret = process_ipsec_ev_inbound(&lconf.inbound,
							&lconf.rt, &ev);
		else
			ret = process_ipsec_ev_outbound(&lconf.outbound,
							&lconf.rt, &ev);
		if (ret != 1)
			/* The pkt has been dropped */
			continue;

		/*
		 * Since tx internal port is available, events can be
		 * directly enqueued to the adapter and it would be
		 * internally submitted to the eth device.
		 */
		rte_event_eth_tx_adapter_enqueue(links[0].eventdev_id,
				links[0].event_port_id,
				&ev,	/* events */
				1,	/* nb_events */
				0	/* flags */);
	}
}

static uint8_t
ipsec_eventmode_populate_wrkr_params(struct eh_app_worker_params *wrkrs)
{
	struct eh_app_worker_params *wrkr;
	uint8_t nb_wrkr_param = 0;

	/* Save workers */
	wrkr = wrkrs;

	/* Non-burst - Tx internal port - driver mode */
	wrkr->cap.burst = EH_RX_TYPE_NON_BURST;
	wrkr->cap.tx_internal_port = EH_TX_TYPE_INTERNAL_PORT;
	wrkr->cap.ipsec_mode = EH_IPSEC_MODE_TYPE_DRIVER;
	wrkr->worker_thread = ipsec_wrkr_non_burst_int_port_drv_mode;
	wrkr++;
	nb_wrkr_param++;

	/* Non-burst - Tx internal port - app mode */
	wrkr->cap.burst = EH_RX_TYPE_NON_BURST;
	wrkr->cap.tx_internal_port = EH_TX_TYPE_INTERNAL_PORT;
	wrkr->cap.ipsec_mode = EH_IPSEC_MODE_TYPE_APP;
	wrkr->worker_thread = ipsec_wrkr_non_burst_int_port_app_mode;
	nb_wrkr_param++;

	return nb_wrkr_param;
}

static void
ipsec_eventmode_worker(struct eh_conf *conf)
{
	struct eh_app_worker_params ipsec_wrkr[IPSEC_EVENTMODE_WORKERS] = {
					{{{0} }, NULL } };
	uint8_t nb_wrkr_param;

	/* Populate l2fwd_wrkr params */
	nb_wrkr_param = ipsec_eventmode_populate_wrkr_params(ipsec_wrkr);

	/*
	 * Launch correct worker after checking
	 * the event device's capabilities.
	 */
	eh_launch_worker(conf, ipsec_wrkr, nb_wrkr_param);
}

int ipsec_launch_one_lcore(void *args)
{
	struct eh_conf *conf;

	conf = (struct eh_conf *)args;

	if (conf->mode == EH_PKT_TRANSFER_MODE_POLL) {
		/* Run in poll mode */
		ipsec_poll_mode_worker();
	} else if (conf->mode == EH_PKT_TRANSFER_MODE_EVENT) {
		/* Run in event mode */
		ipsec_eventmode_worker(conf);
	}
	return 0;
}
