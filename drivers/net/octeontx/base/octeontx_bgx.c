/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <string.h>

#include "octeontx_bgx.h"

int
octeontx_bgx_port_open(int port, octeontx_mbox_bgx_port_conf_t *conf)
{
	struct octeontx_mbox_hdr hdr;
	octeontx_mbox_bgx_port_conf_t bgx_conf;
	int len = sizeof(octeontx_mbox_bgx_port_conf_t);
	int res;

	memset(&bgx_conf, 0, sizeof(octeontx_mbox_bgx_port_conf_t));
	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_OPEN;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, NULL, 0, &bgx_conf, len);
	if (res < 0)
		return -EACCES;

	conf->enable = bgx_conf.enable;
	conf->promisc = bgx_conf.promisc;
	conf->bpen = bgx_conf.bpen;
	conf->node = bgx_conf.node;
	conf->base_chan = bgx_conf.base_chan;
	conf->num_chans = bgx_conf.num_chans;
	conf->mtu = bgx_conf.mtu;
	conf->bgx = bgx_conf.bgx;
	conf->lmac = bgx_conf.lmac;
	conf->mode = bgx_conf.mode;
	conf->pkind = bgx_conf.pkind;
	memcpy(conf->macaddr, bgx_conf.macaddr, 6);

	return res;
}

int
octeontx_bgx_port_close(int port)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_CLOSE;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, NULL, 0, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

int
octeontx_bgx_port_start(int port)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_START;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, NULL, 0, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

int
octeontx_bgx_port_stop(int port)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_STOP;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, NULL, 0, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

int
octeontx_bgx_port_get_config(int port, octeontx_mbox_bgx_port_conf_t *conf)
{
	struct octeontx_mbox_hdr hdr;
	octeontx_mbox_bgx_port_conf_t bgx_conf;
	int len = sizeof(octeontx_mbox_bgx_port_conf_t);
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_GET_CONFIG;
	hdr.vfid = port;

	memset(&bgx_conf, 0, sizeof(octeontx_mbox_bgx_port_conf_t));
	res = octeontx_mbox_send(&hdr, NULL, 0, &bgx_conf, len);
	if (res < 0)
		return -EACCES;

	conf->enable = bgx_conf.enable;
	conf->promisc = bgx_conf.promisc;
	conf->bpen = bgx_conf.bpen;
	conf->node = bgx_conf.node;
	conf->base_chan = bgx_conf.base_chan;
	conf->num_chans = bgx_conf.num_chans;
	conf->mtu = bgx_conf.mtu;
	conf->bgx = bgx_conf.bgx;
	conf->lmac = bgx_conf.lmac;
	conf->mode = bgx_conf.mode;
	conf->pkind = bgx_conf.pkind;
	memcpy(conf->macaddr, bgx_conf.macaddr, 6);

	return res;
}

int
octeontx_bgx_port_status(int port, octeontx_mbox_bgx_port_status_t *stat)
{
	struct octeontx_mbox_hdr hdr;
	octeontx_mbox_bgx_port_status_t bgx_stat;
	int len = sizeof(octeontx_mbox_bgx_port_status_t);
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_GET_STATUS;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, NULL, 0, &bgx_stat, len);
	if (res < 0)
		return -EACCES;

	stat->link_up = bgx_stat.link_up;

	return res;
}

int
octeontx_bgx_port_stats(int port, octeontx_mbox_bgx_port_stats_t *stats)
{
	struct octeontx_mbox_hdr hdr;
	octeontx_mbox_bgx_port_stats_t bgx_stats;
	int len = sizeof(octeontx_mbox_bgx_port_stats_t);
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_GET_STATS;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, NULL, 0, &bgx_stats, len);
	if (res < 0)
		return -EACCES;

	stats->rx_packets = bgx_stats.rx_packets;
	stats->rx_bytes = bgx_stats.rx_bytes;
	stats->rx_dropped = bgx_stats.rx_dropped;
	stats->rx_errors = bgx_stats.rx_errors;
	stats->tx_packets = bgx_stats.tx_packets;
	stats->tx_bytes = bgx_stats.tx_bytes;
	stats->tx_dropped = bgx_stats.tx_dropped;
	stats->tx_errors = bgx_stats.tx_errors;
	return res;
}

int
octeontx_bgx_port_stats_clr(int port)
{
	struct octeontx_mbox_hdr hdr;
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_CLR_STATS;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, NULL, 0, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

int
octeontx_bgx_port_link_status(int port)
{
	struct octeontx_mbox_hdr hdr;
	uint8_t link;
	int len = sizeof(uint8_t);
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_GET_LINK_STATUS;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, NULL, 0, &link, len);
	if (res < 0)
		return -EACCES;

	return link;
}

int
octeontx_bgx_port_promisc_set(int port, int en)
{
	struct octeontx_mbox_hdr hdr;
	uint8_t	prom;
	int res;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_SET_PROMISC;
	hdr.vfid = port;
	prom = en ? 1 : 0;

	res = octeontx_mbox_send(&hdr, &prom, sizeof(prom), NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}

int
octeontx_bgx_port_mac_set(int port, uint8_t *mac_addr)
{
	struct octeontx_mbox_hdr hdr;
	int len = 6;
	int res = 0;

	hdr.coproc = OCTEONTX_BGX_COPROC;
	hdr.msg = MBOX_BGX_PORT_SET_MACADDR;
	hdr.vfid = port;

	res = octeontx_mbox_send(&hdr, mac_addr, len, NULL, 0);
	if (res < 0)
		return -EACCES;

	return res;
}
