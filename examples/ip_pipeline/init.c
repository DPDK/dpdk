/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#ifdef RTE_EXEC_ENV_LINUXAPP
#include <linux/if.h>
#include <linux/if_tun.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_eal.h>
#include <rte_malloc.h>

#include "app.h"
#include "pipeline.h"
#include "pipeline_common_fe.h"
#include "pipeline_master.h"
#include "pipeline_passthrough.h"
#include "pipeline_firewall.h"
#include "pipeline_flow_classification.h"
#include "pipeline_flow_actions.h"
#include "pipeline_routing.h"
#include "thread_fe.h"

#define APP_NAME_SIZE	32

#define APP_RETA_SIZE_MAX     (ETH_RSS_RETA_SIZE_512 / RTE_RETA_GROUP_SIZE)

static void
app_init_core_map(struct app_params *app)
{
	APP_LOG(app, HIGH, "Initializing CPU core map ...");
	app->core_map = cpu_core_map_init(RTE_MAX_NUMA_NODES, RTE_MAX_LCORE,
				4, 0);

	if (app->core_map == NULL)
		rte_panic("Cannot create CPU core map\n");

	if (app->log_level >= APP_LOG_LEVEL_LOW)
		cpu_core_map_print(app->core_map);
}

/* Core Mask String in Hex Representation */
#define APP_CORE_MASK_STRING_SIZE ((64 * APP_CORE_MASK_SIZE) / 8 * 2 + 1)

static void
app_init_core_mask(struct app_params *app)
{
	uint32_t i;
	char core_mask_str[APP_CORE_MASK_STRING_SIZE];

	for (i = 0; i < app->n_pipelines; i++) {
		struct app_pipeline_params *p = &app->pipeline_params[i];
		int lcore_id;

		lcore_id = cpu_core_map_get_lcore_id(app->core_map,
			p->socket_id,
			p->core_id,
			p->hyper_th_id);

		if (lcore_id < 0)
			rte_panic("Cannot create CPU core mask\n");

		app_core_enable_in_core_mask(app, lcore_id);
	}

	app_core_build_core_mask_string(app, core_mask_str);
	APP_LOG(app, HIGH, "CPU core mask = 0x%s", core_mask_str);
}

static void
app_init_eal(struct app_params *app)
{
	char buffer[256];
	char core_mask_str[APP_CORE_MASK_STRING_SIZE];
	struct app_eal_params *p = &app->eal_params;
	uint32_t n_args = 0;
	uint32_t i;
	int status;

	app->eal_argv[n_args++] = strdup(app->app_name);

	app_core_build_core_mask_string(app, core_mask_str);
	snprintf(buffer, sizeof(buffer), "-c%s", core_mask_str);
	app->eal_argv[n_args++] = strdup(buffer);

	if (p->coremap) {
		snprintf(buffer, sizeof(buffer), "--lcores=%s", p->coremap);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->master_lcore_present) {
		snprintf(buffer,
			sizeof(buffer),
			"--master-lcore=%" PRIu32,
			p->master_lcore);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	snprintf(buffer, sizeof(buffer), "-n%" PRIu32, p->channels);
	app->eal_argv[n_args++] = strdup(buffer);

	if (p->memory_present) {
		snprintf(buffer, sizeof(buffer), "-m%" PRIu32, p->memory);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->ranks_present) {
		snprintf(buffer, sizeof(buffer), "-r%" PRIu32, p->ranks);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	for (i = 0; i < APP_MAX_LINKS; i++) {
		if (p->pci_blacklist[i] == NULL)
			break;

		snprintf(buffer,
			sizeof(buffer),
			"--pci-blacklist=%s",
			p->pci_blacklist[i]);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (app->port_mask != 0)
		for (i = 0; i < APP_MAX_LINKS; i++) {
			if (p->pci_whitelist[i] == NULL)
				break;

			snprintf(buffer,
				sizeof(buffer),
				"--pci-whitelist=%s",
				p->pci_whitelist[i]);
			app->eal_argv[n_args++] = strdup(buffer);
		}
	else
		for (i = 0; i < app->n_links; i++) {
			char *pci_bdf = app->link_params[i].pci_bdf;

			snprintf(buffer,
				sizeof(buffer),
				"--pci-whitelist=%s",
				pci_bdf);
			app->eal_argv[n_args++] = strdup(buffer);
		}

	for (i = 0; i < APP_MAX_LINKS; i++) {
		if (p->vdev[i] == NULL)
			break;

		snprintf(buffer,
			sizeof(buffer),
			"--vdev=%s",
			p->vdev[i]);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if ((p->vmware_tsc_map_present) && p->vmware_tsc_map) {
		snprintf(buffer, sizeof(buffer), "--vmware-tsc-map");
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->proc_type) {
		snprintf(buffer,
			sizeof(buffer),
			"--proc-type=%s",
			p->proc_type);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->syslog) {
		snprintf(buffer, sizeof(buffer), "--syslog=%s", p->syslog);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->log_level_present) {
		snprintf(buffer,
			sizeof(buffer),
			"--log-level=%" PRIu32,
			p->log_level);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if ((p->version_present) && p->version) {
		snprintf(buffer, sizeof(buffer), "-v");
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if ((p->help_present) && p->help) {
		snprintf(buffer, sizeof(buffer), "--help");
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if ((p->no_huge_present) && p->no_huge) {
		snprintf(buffer, sizeof(buffer), "--no-huge");
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if ((p->no_pci_present) && p->no_pci) {
		snprintf(buffer, sizeof(buffer), "--no-pci");
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if ((p->no_hpet_present) && p->no_hpet) {
		snprintf(buffer, sizeof(buffer), "--no-hpet");
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if ((p->no_shconf_present) && p->no_shconf) {
		snprintf(buffer, sizeof(buffer), "--no-shconf");
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->add_driver) {
		snprintf(buffer, sizeof(buffer), "-d%s", p->add_driver);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->socket_mem) {
		snprintf(buffer,
			sizeof(buffer),
			"--socket-mem=%s",
			p->socket_mem);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->huge_dir) {
		snprintf(buffer, sizeof(buffer), "--huge-dir=%s", p->huge_dir);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->file_prefix) {
		snprintf(buffer,
			sizeof(buffer),
			"--file-prefix=%s",
			p->file_prefix);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->base_virtaddr) {
		snprintf(buffer,
			sizeof(buffer),
			"--base-virtaddr=%s",
			p->base_virtaddr);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if ((p->create_uio_dev_present) && p->create_uio_dev) {
		snprintf(buffer, sizeof(buffer), "--create-uio-dev");
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->vfio_intr) {
		snprintf(buffer,
			sizeof(buffer),
			"--vfio-intr=%s",
			p->vfio_intr);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if ((p->xen_dom0_present) && (p->xen_dom0)) {
		snprintf(buffer, sizeof(buffer), "--xen-dom0");
		app->eal_argv[n_args++] = strdup(buffer);
	}

	snprintf(buffer, sizeof(buffer), "--");
	app->eal_argv[n_args++] = strdup(buffer);

	app->eal_argc = n_args;

	APP_LOG(app, HIGH, "Initializing EAL ...");
	if (app->log_level >= APP_LOG_LEVEL_LOW) {
		int i;

		fprintf(stdout, "[APP] EAL arguments: \"");
		for (i = 1; i < app->eal_argc; i++)
			fprintf(stdout, "%s ", app->eal_argv[i]);
		fprintf(stdout, "\"\n");
	}

	status = rte_eal_init(app->eal_argc, app->eal_argv);
	if (status < 0)
		rte_panic("EAL init error\n");
}

static void
app_init_mempool(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_mempools; i++) {
		struct app_mempool_params *p = &app->mempool_params[i];

		APP_LOG(app, HIGH, "Initializing %s ...", p->name);
		app->mempool[i] = rte_pktmbuf_pool_create(
			p->name,
			p->pool_size,
			p->cache_size,
			0, /* priv_size */
			p->buffer_size -
				sizeof(struct rte_mbuf), /* mbuf data size */
			p->cpu_socket_id);

		if (app->mempool[i] == NULL)
			rte_panic("%s init error\n", p->name);
	}
}

static inline int
app_link_filter_arp_add(struct app_link_params *link)
{
	struct rte_eth_ethertype_filter filter = {
		.ether_type = ETHER_TYPE_ARP,
		.flags = 0,
		.queue = link->arp_q,
	};

	return rte_eth_dev_filter_ctrl(link->pmd_id,
		RTE_ETH_FILTER_ETHERTYPE,
		RTE_ETH_FILTER_ADD,
		&filter);
}

static inline int
app_link_filter_tcp_syn_add(struct app_link_params *link)
{
	struct rte_eth_syn_filter filter = {
		.hig_pri = 1,
		.queue = link->tcp_syn_q,
	};

	return rte_eth_dev_filter_ctrl(link->pmd_id,
		RTE_ETH_FILTER_SYN,
		RTE_ETH_FILTER_ADD,
		&filter);
}

static inline int
app_link_filter_ip_add(struct app_link_params *l1, struct app_link_params *l2)
{
	struct rte_eth_ntuple_filter filter = {
		.flags = RTE_5TUPLE_FLAGS,
		.dst_ip = rte_bswap32(l2->ip),
		.dst_ip_mask = UINT32_MAX, /* Enable */
		.src_ip = 0,
		.src_ip_mask = 0, /* Disable */
		.dst_port = 0,
		.dst_port_mask = 0, /* Disable */
		.src_port = 0,
		.src_port_mask = 0, /* Disable */
		.proto = 0,
		.proto_mask = 0, /* Disable */
		.tcp_flags = 0,
		.priority = 1, /* Lowest */
		.queue = l1->ip_local_q,
	};

	return rte_eth_dev_filter_ctrl(l1->pmd_id,
		RTE_ETH_FILTER_NTUPLE,
		RTE_ETH_FILTER_ADD,
		&filter);
}

static inline int
app_link_filter_ip_del(struct app_link_params *l1, struct app_link_params *l2)
{
	struct rte_eth_ntuple_filter filter = {
		.flags = RTE_5TUPLE_FLAGS,
		.dst_ip = rte_bswap32(l2->ip),
		.dst_ip_mask = UINT32_MAX, /* Enable */
		.src_ip = 0,
		.src_ip_mask = 0, /* Disable */
		.dst_port = 0,
		.dst_port_mask = 0, /* Disable */
		.src_port = 0,
		.src_port_mask = 0, /* Disable */
		.proto = 0,
		.proto_mask = 0, /* Disable */
		.tcp_flags = 0,
		.priority = 1, /* Lowest */
		.queue = l1->ip_local_q,
	};

	return rte_eth_dev_filter_ctrl(l1->pmd_id,
		RTE_ETH_FILTER_NTUPLE,
		RTE_ETH_FILTER_DELETE,
		&filter);
}

static inline int
app_link_filter_tcp_add(struct app_link_params *l1, struct app_link_params *l2)
{
	struct rte_eth_ntuple_filter filter = {
		.flags = RTE_5TUPLE_FLAGS,
		.dst_ip = rte_bswap32(l2->ip),
		.dst_ip_mask = UINT32_MAX, /* Enable */
		.src_ip = 0,
		.src_ip_mask = 0, /* Disable */
		.dst_port = 0,
		.dst_port_mask = 0, /* Disable */
		.src_port = 0,
		.src_port_mask = 0, /* Disable */
		.proto = IPPROTO_TCP,
		.proto_mask = UINT8_MAX, /* Enable */
		.tcp_flags = 0,
		.priority = 2, /* Higher priority than IP */
		.queue = l1->tcp_local_q,
	};

	return rte_eth_dev_filter_ctrl(l1->pmd_id,
		RTE_ETH_FILTER_NTUPLE,
		RTE_ETH_FILTER_ADD,
		&filter);
}

static inline int
app_link_filter_tcp_del(struct app_link_params *l1, struct app_link_params *l2)
{
	struct rte_eth_ntuple_filter filter = {
		.flags = RTE_5TUPLE_FLAGS,
		.dst_ip = rte_bswap32(l2->ip),
		.dst_ip_mask = UINT32_MAX, /* Enable */
		.src_ip = 0,
		.src_ip_mask = 0, /* Disable */
		.dst_port = 0,
		.dst_port_mask = 0, /* Disable */
		.src_port = 0,
		.src_port_mask = 0, /* Disable */
		.proto = IPPROTO_TCP,
		.proto_mask = UINT8_MAX, /* Enable */
		.tcp_flags = 0,
		.priority = 2, /* Higher priority than IP */
		.queue = l1->tcp_local_q,
	};

	return rte_eth_dev_filter_ctrl(l1->pmd_id,
		RTE_ETH_FILTER_NTUPLE,
		RTE_ETH_FILTER_DELETE,
		&filter);
}

static inline int
app_link_filter_udp_add(struct app_link_params *l1, struct app_link_params *l2)
{
	struct rte_eth_ntuple_filter filter = {
		.flags = RTE_5TUPLE_FLAGS,
		.dst_ip = rte_bswap32(l2->ip),
		.dst_ip_mask = UINT32_MAX, /* Enable */
		.src_ip = 0,
		.src_ip_mask = 0, /* Disable */
		.dst_port = 0,
		.dst_port_mask = 0, /* Disable */
		.src_port = 0,
		.src_port_mask = 0, /* Disable */
		.proto = IPPROTO_UDP,
		.proto_mask = UINT8_MAX, /* Enable */
		.tcp_flags = 0,
		.priority = 2, /* Higher priority than IP */
		.queue = l1->udp_local_q,
	};

	return rte_eth_dev_filter_ctrl(l1->pmd_id,
		RTE_ETH_FILTER_NTUPLE,
		RTE_ETH_FILTER_ADD,
		&filter);
}

static inline int
app_link_filter_udp_del(struct app_link_params *l1, struct app_link_params *l2)
{
	struct rte_eth_ntuple_filter filter = {
		.flags = RTE_5TUPLE_FLAGS,
		.dst_ip = rte_bswap32(l2->ip),
		.dst_ip_mask = UINT32_MAX, /* Enable */
		.src_ip = 0,
		.src_ip_mask = 0, /* Disable */
		.dst_port = 0,
		.dst_port_mask = 0, /* Disable */
		.src_port = 0,
		.src_port_mask = 0, /* Disable */
		.proto = IPPROTO_UDP,
		.proto_mask = UINT8_MAX, /* Enable */
		.tcp_flags = 0,
		.priority = 2, /* Higher priority than IP */
		.queue = l1->udp_local_q,
	};

	return rte_eth_dev_filter_ctrl(l1->pmd_id,
		RTE_ETH_FILTER_NTUPLE,
		RTE_ETH_FILTER_DELETE,
		&filter);
}

static inline int
app_link_filter_sctp_add(struct app_link_params *l1, struct app_link_params *l2)
{
	struct rte_eth_ntuple_filter filter = {
		.flags = RTE_5TUPLE_FLAGS,
		.dst_ip = rte_bswap32(l2->ip),
		.dst_ip_mask = UINT32_MAX, /* Enable */
		.src_ip = 0,
		.src_ip_mask = 0, /* Disable */
		.dst_port = 0,
		.dst_port_mask = 0, /* Disable */
		.src_port = 0,
		.src_port_mask = 0, /* Disable */
		.proto = IPPROTO_SCTP,
		.proto_mask = UINT8_MAX, /* Enable */
		.tcp_flags = 0,
		.priority = 2, /* Higher priority than IP */
		.queue = l1->sctp_local_q,
	};

	return rte_eth_dev_filter_ctrl(l1->pmd_id,
		RTE_ETH_FILTER_NTUPLE,
		RTE_ETH_FILTER_ADD,
		&filter);
}

static inline int
app_link_filter_sctp_del(struct app_link_params *l1, struct app_link_params *l2)
{
	struct rte_eth_ntuple_filter filter = {
		.flags = RTE_5TUPLE_FLAGS,
		.dst_ip = rte_bswap32(l2->ip),
		.dst_ip_mask = UINT32_MAX, /* Enable */
		.src_ip = 0,
		.src_ip_mask = 0, /* Disable */
		.dst_port = 0,
		.dst_port_mask = 0, /* Disable */
		.src_port = 0,
		.src_port_mask = 0, /* Disable */
		.proto = IPPROTO_SCTP,
		.proto_mask = UINT8_MAX, /* Enable */
		.tcp_flags = 0,
		.priority = 2, /* Higher priority than IP */
		.queue = l1->sctp_local_q,
	};

	return rte_eth_dev_filter_ctrl(l1->pmd_id,
		RTE_ETH_FILTER_NTUPLE,
		RTE_ETH_FILTER_DELETE,
		&filter);
}

static void
app_link_set_arp_filter(struct app_params *app, struct app_link_params *cp)
{
	if (cp->arp_q != 0) {
		int status = app_link_filter_arp_add(cp);

		APP_LOG(app, LOW, "%s (%" PRIu32 "): "
			"Adding ARP filter (queue = %" PRIu32 ")",
			cp->name, cp->pmd_id, cp->arp_q);

		if (status)
			rte_panic("%s (%" PRIu32 "): "
				"Error adding ARP filter "
				"(queue = %" PRIu32 ") (%" PRId32 ")\n",
				cp->name, cp->pmd_id, cp->arp_q, status);
	}
}

static void
app_link_set_tcp_syn_filter(struct app_params *app, struct app_link_params *cp)
{
	if (cp->tcp_syn_q != 0) {
		int status = app_link_filter_tcp_syn_add(cp);

		APP_LOG(app, LOW, "%s (%" PRIu32 "): "
			"Adding TCP SYN filter (queue = %" PRIu32 ")",
			cp->name, cp->pmd_id, cp->tcp_syn_q);

		if (status)
			rte_panic("%s (%" PRIu32 "): "
				"Error adding TCP SYN filter "
				"(queue = %" PRIu32 ") (%" PRId32 ")\n",
				cp->name, cp->pmd_id, cp->tcp_syn_q,
				status);
	}
}

void
app_link_up_internal(struct app_params *app, struct app_link_params *cp)
{
	uint32_t i;
	int status;

	/* For each link, add filters for IP of current link */
	if (cp->ip != 0) {
		for (i = 0; i < app->n_links; i++) {
			struct app_link_params *p = &app->link_params[i];

			/* IP */
			if (p->ip_local_q != 0) {
				int status = app_link_filter_ip_add(p, cp);

				APP_LOG(app, LOW, "%s (%" PRIu32 "): "
					"Adding IP filter (queue= %" PRIu32
					", IP = 0x%08" PRIx32 ")",
					p->name, p->pmd_id, p->ip_local_q,
					cp->ip);

				if (status)
					rte_panic("%s (%" PRIu32 "): "
						"Error adding IP "
						"filter (queue= %" PRIu32 ", "
						"IP = 0x%08" PRIx32
						") (%" PRId32 ")\n",
						p->name, p->pmd_id,
						p->ip_local_q, cp->ip, status);
			}

			/* TCP */
			if (p->tcp_local_q != 0) {
				int status = app_link_filter_tcp_add(p, cp);

				APP_LOG(app, LOW, "%s (%" PRIu32 "): "
					"Adding TCP filter "
					"(queue = %" PRIu32
					", IP = 0x%08" PRIx32 ")",
					p->name, p->pmd_id, p->tcp_local_q,
					cp->ip);

				if (status)
					rte_panic("%s (%" PRIu32 "): "
						"Error adding TCP "
						"filter (queue = %" PRIu32 ", "
						"IP = 0x%08" PRIx32
						") (%" PRId32 ")\n",
						p->name, p->pmd_id,
						p->tcp_local_q, cp->ip, status);
			}

			/* UDP */
			if (p->udp_local_q != 0) {
				int status = app_link_filter_udp_add(p, cp);

				APP_LOG(app, LOW, "%s (%" PRIu32 "): "
					"Adding UDP filter "
					"(queue = %" PRIu32
					", IP = 0x%08" PRIx32 ")",
					p->name, p->pmd_id, p->udp_local_q,
					cp->ip);

				if (status)
					rte_panic("%s (%" PRIu32 "): "
						"Error adding UDP "
						"filter (queue = %" PRIu32 ", "
						"IP = 0x%08" PRIx32
						") (%" PRId32 ")\n",
						p->name, p->pmd_id,
						p->udp_local_q, cp->ip, status);
			}

			/* SCTP */
			if (p->sctp_local_q != 0) {
				int status = app_link_filter_sctp_add(p, cp);

				APP_LOG(app, LOW, "%s (%" PRIu32
					"): Adding SCTP filter "
					"(queue = %" PRIu32
					", IP = 0x%08" PRIx32 ")",
					p->name, p->pmd_id, p->sctp_local_q,
					cp->ip);

				if (status)
					rte_panic("%s (%" PRIu32 "): "
						"Error adding SCTP "
						"filter (queue = %" PRIu32 ", "
						"IP = 0x%08" PRIx32
						") (%" PRId32 ")\n",
						p->name, p->pmd_id,
						p->sctp_local_q, cp->ip,
						status);
			}
		}
	}

	/* PMD link up */
	status = rte_eth_dev_set_link_up(cp->pmd_id);
	/* Do not panic if PMD does not provide link up functionality */
	if (status < 0 && status != -ENOTSUP)
		rte_panic("%s (%" PRIu32 "): PMD set link up error %"
			PRId32 "\n", cp->name, cp->pmd_id, status);

	/* Mark link as UP */
	cp->state = 1;
}

void
app_link_down_internal(struct app_params *app, struct app_link_params *cp)
{
	uint32_t i;
	int status;

	/* PMD link down */
	status = rte_eth_dev_set_link_down(cp->pmd_id);
	/* Do not panic if PMD does not provide link down functionality */
	if (status < 0 && status != -ENOTSUP)
		rte_panic("%s (%" PRIu32 "): PMD set link down error %"
			PRId32 "\n", cp->name, cp->pmd_id, status);

	/* Mark link as DOWN */
	cp->state = 0;

	/* Return if current link IP is not valid */
	if (cp->ip == 0)
		return;

	/* For each link, remove filters for IP of current link */
	for (i = 0; i < app->n_links; i++) {
		struct app_link_params *p = &app->link_params[i];

		/* IP */
		if (p->ip_local_q != 0) {
			int status = app_link_filter_ip_del(p, cp);

			APP_LOG(app, LOW, "%s (%" PRIu32
				"): Deleting IP filter "
				"(queue = %" PRIu32 ", IP = 0x%" PRIx32 ")",
				p->name, p->pmd_id, p->ip_local_q, cp->ip);

			if (status)
				rte_panic("%s (%" PRIu32
					"): Error deleting IP filter "
					"(queue = %" PRIu32
					", IP = 0x%" PRIx32
					") (%" PRId32 ")\n",
					p->name, p->pmd_id, p->ip_local_q,
					cp->ip, status);
		}

		/* TCP */
		if (p->tcp_local_q != 0) {
			int status = app_link_filter_tcp_del(p, cp);

			APP_LOG(app, LOW, "%s (%" PRIu32
				"): Deleting TCP filter "
				"(queue = %" PRIu32
				", IP = 0x%" PRIx32 ")",
				p->name, p->pmd_id, p->tcp_local_q, cp->ip);

			if (status)
				rte_panic("%s (%" PRIu32
					"): Error deleting TCP filter "
					"(queue = %" PRIu32
					", IP = 0x%" PRIx32
					") (%" PRId32 ")\n",
					p->name, p->pmd_id, p->tcp_local_q,
					cp->ip, status);
		}

		/* UDP */
		if (p->udp_local_q != 0) {
			int status = app_link_filter_udp_del(p, cp);

			APP_LOG(app, LOW, "%s (%" PRIu32
				"): Deleting UDP filter "
				"(queue = %" PRIu32 ", IP = 0x%" PRIx32 ")",
				p->name, p->pmd_id, p->udp_local_q, cp->ip);

			if (status)
				rte_panic("%s (%" PRIu32
					"): Error deleting UDP filter "
					"(queue = %" PRIu32
					", IP = 0x%" PRIx32
					") (%" PRId32 ")\n",
					p->name, p->pmd_id, p->udp_local_q,
					cp->ip, status);
		}

		/* SCTP */
		if (p->sctp_local_q != 0) {
			int status = app_link_filter_sctp_del(p, cp);

			APP_LOG(app, LOW, "%s (%" PRIu32
				"): Deleting SCTP filter "
				"(queue = %" PRIu32
				", IP = 0x%" PRIx32 ")",
				p->name, p->pmd_id, p->sctp_local_q, cp->ip);

			if (status)
				rte_panic("%s (%" PRIu32
					"): Error deleting SCTP filter "
					"(queue = %" PRIu32
					", IP = 0x%" PRIx32
					") (%" PRId32 ")\n",
					p->name, p->pmd_id, p->sctp_local_q,
					cp->ip, status);
		}
	}
}

static void
app_check_link(struct app_params *app)
{
	uint32_t all_links_up, i;

	all_links_up = 1;

	for (i = 0; i < app->n_links; i++) {
		struct app_link_params *p = &app->link_params[i];
		struct rte_eth_link link_params;

		memset(&link_params, 0, sizeof(link_params));
		rte_eth_link_get(p->pmd_id, &link_params);

		APP_LOG(app, HIGH, "%s (%" PRIu32 ") (%" PRIu32 " Gbps) %s",
			p->name,
			p->pmd_id,
			link_params.link_speed / 1000,
			link_params.link_status ? "UP" : "DOWN");

		if (link_params.link_status == ETH_LINK_DOWN)
			all_links_up = 0;
	}

	if (all_links_up == 0)
		rte_panic("Some links are DOWN\n");
}

static uint32_t
is_any_swq_frag_or_ras(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_pktq_swq; i++) {
		struct app_pktq_swq_params *p = &app->swq_params[i];

		if ((p->ipv4_frag == 1) || (p->ipv6_frag == 1) ||
			(p->ipv4_ras == 1) || (p->ipv6_ras == 1))
			return 1;
	}

	return 0;
}

static void
app_init_link_frag_ras(struct app_params *app)
{
	uint32_t i;

	if (is_any_swq_frag_or_ras(app)) {
		for (i = 0; i < app->n_pktq_hwq_out; i++) {
			struct app_pktq_hwq_out_params *p_txq = &app->hwq_out_params[i];

			p_txq->conf.txq_flags &= ~ETH_TXQ_FLAGS_NOMULTSEGS;
		}
	}
}

static inline int
app_get_cpu_socket_id(uint32_t pmd_id)
{
	int status = rte_eth_dev_socket_id(pmd_id);

	return (status != SOCKET_ID_ANY) ? status : 0;
}

static inline int
app_link_rss_enabled(struct app_link_params *cp)
{
	return (cp->n_rss_qs) ? 1 : 0;
}

static void
app_link_rss_setup(struct app_link_params *cp)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rss_reta_entry64 reta_conf[APP_RETA_SIZE_MAX];
	uint32_t i;
	int status;

    /* Get RETA size */
	memset(&dev_info, 0, sizeof(dev_info));
	rte_eth_dev_info_get(cp->pmd_id, &dev_info);

	if (dev_info.reta_size == 0)
		rte_panic("%s (%u): RSS setup error (null RETA size)\n",
			cp->name, cp->pmd_id);

	if (dev_info.reta_size > ETH_RSS_RETA_SIZE_512)
		rte_panic("%s (%u): RSS setup error (RETA size too big)\n",
			cp->name, cp->pmd_id);

	/* Setup RETA contents */
	memset(reta_conf, 0, sizeof(reta_conf));

	for (i = 0; i < dev_info.reta_size; i++)
		reta_conf[i / RTE_RETA_GROUP_SIZE].mask = UINT64_MAX;

	for (i = 0; i < dev_info.reta_size; i++) {
		uint32_t reta_id = i / RTE_RETA_GROUP_SIZE;
		uint32_t reta_pos = i % RTE_RETA_GROUP_SIZE;
		uint32_t rss_qs_pos = i % cp->n_rss_qs;

		reta_conf[reta_id].reta[reta_pos] =
			(uint16_t) cp->rss_qs[rss_qs_pos];
	}

	/* RETA update */
	status = rte_eth_dev_rss_reta_update(cp->pmd_id,
		reta_conf,
		dev_info.reta_size);
	if (status != 0)
		rte_panic("%s (%u): RSS setup error (RETA update failed)\n",
			cp->name, cp->pmd_id);
}

static void
app_init_link_set_config(struct app_link_params *p)
{
	if (p->n_rss_qs) {
		p->conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
		p->conf.rx_adv_conf.rss_conf.rss_hf = p->rss_proto_ipv4 |
			p->rss_proto_ipv6 |
			p->rss_proto_l2;
	}
}

static void
app_init_link(struct app_params *app)
{
	uint32_t i;

	app_init_link_frag_ras(app);

	for (i = 0; i < app->n_links; i++) {
		struct app_link_params *p_link = &app->link_params[i];
		uint32_t link_id, n_hwq_in, n_hwq_out, j;
		int status;

		sscanf(p_link->name, "LINK%" PRIu32, &link_id);
		n_hwq_in = app_link_get_n_rxq(app, p_link);
		n_hwq_out = app_link_get_n_txq(app, p_link);
		app_init_link_set_config(p_link);

		APP_LOG(app, HIGH, "Initializing %s (%" PRIu32") "
			"(%" PRIu32 " RXQ, %" PRIu32 " TXQ) ...",
			p_link->name,
			p_link->pmd_id,
			n_hwq_in,
			n_hwq_out);

		/* LINK */
		status = rte_eth_dev_configure(
			p_link->pmd_id,
			n_hwq_in,
			n_hwq_out,
			&p_link->conf);
		if (status < 0)
			rte_panic("%s (%" PRId32 "): "
				"init error (%" PRId32 ")\n",
				p_link->name, p_link->pmd_id, status);

		rte_eth_macaddr_get(p_link->pmd_id,
			(struct ether_addr *) &p_link->mac_addr);

		if (p_link->promisc)
			rte_eth_promiscuous_enable(p_link->pmd_id);

		/* RXQ */
		for (j = 0; j < app->n_pktq_hwq_in; j++) {
			struct app_pktq_hwq_in_params *p_rxq =
				&app->hwq_in_params[j];
			uint32_t rxq_link_id, rxq_queue_id;
			uint16_t nb_rxd = p_rxq->size;

			sscanf(p_rxq->name, "RXQ%" PRIu32 ".%" PRIu32,
				&rxq_link_id, &rxq_queue_id);
			if (rxq_link_id != link_id)
				continue;

			status = rte_eth_dev_adjust_nb_rx_tx_desc(
				p_link->pmd_id,
				&nb_rxd,
				NULL);
			if (status < 0)
				rte_panic("%s (%" PRIu32 "): "
					"%s adjust number of Rx descriptors "
					"error (%" PRId32 ")\n",
					p_link->name,
					p_link->pmd_id,
					p_rxq->name,
					status);

			status = rte_eth_rx_queue_setup(
				p_link->pmd_id,
				rxq_queue_id,
				nb_rxd,
				app_get_cpu_socket_id(p_link->pmd_id),
				&p_rxq->conf,
				app->mempool[p_rxq->mempool_id]);
			if (status < 0)
				rte_panic("%s (%" PRIu32 "): "
					"%s init error (%" PRId32 ")\n",
					p_link->name,
					p_link->pmd_id,
					p_rxq->name,
					status);
		}

		/* TXQ */
		for (j = 0; j < app->n_pktq_hwq_out; j++) {
			struct app_pktq_hwq_out_params *p_txq =
				&app->hwq_out_params[j];
			uint32_t txq_link_id, txq_queue_id;
			uint16_t nb_txd = p_txq->size;

			sscanf(p_txq->name, "TXQ%" PRIu32 ".%" PRIu32,
				&txq_link_id, &txq_queue_id);
			if (txq_link_id != link_id)
				continue;

			status = rte_eth_dev_adjust_nb_rx_tx_desc(
				p_link->pmd_id,
				NULL,
				&nb_txd);
			if (status < 0)
				rte_panic("%s (%" PRIu32 "): "
					"%s adjust number of Tx descriptors "
					"error (%" PRId32 ")\n",
					p_link->name,
					p_link->pmd_id,
					p_txq->name,
					status);

			status = rte_eth_tx_queue_setup(
				p_link->pmd_id,
				txq_queue_id,
				nb_txd,
				app_get_cpu_socket_id(p_link->pmd_id),
				&p_txq->conf);
			if (status < 0)
				rte_panic("%s (%" PRIu32 "): "
					"%s init error (%" PRId32 ")\n",
					p_link->name,
					p_link->pmd_id,
					p_txq->name,
					status);
		}

		/* LINK START */
		status = rte_eth_dev_start(p_link->pmd_id);
		if (status < 0)
			rte_panic("Cannot start %s (error %" PRId32 ")\n",
				p_link->name, status);

		/* LINK FILTERS */
		app_link_set_arp_filter(app, p_link);
		app_link_set_tcp_syn_filter(app, p_link);
		if (app_link_rss_enabled(p_link))
			app_link_rss_setup(p_link);

		/* LINK UP */
		app_link_up_internal(app, p_link);
	}

	app_check_link(app);
}

static void
app_init_swq(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_pktq_swq; i++) {
		struct app_pktq_swq_params *p = &app->swq_params[i];
		unsigned flags = 0;

		if (app_swq_get_readers(app, p) == 1)
			flags |= RING_F_SC_DEQ;
		if (app_swq_get_writers(app, p) == 1)
			flags |= RING_F_SP_ENQ;

		APP_LOG(app, HIGH, "Initializing %s...", p->name);
		app->swq[i] = rte_ring_create(
				p->name,
				p->size,
				p->cpu_socket_id,
				flags);

		if (app->swq[i] == NULL)
			rte_panic("%s init error\n", p->name);
	}
}

static void
app_init_tm(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_pktq_tm; i++) {
		struct app_pktq_tm_params *p_tm = &app->tm_params[i];
		struct app_link_params *p_link;
		struct rte_eth_link link_eth_params;
		struct rte_sched_port *sched;
		uint32_t n_subports, subport_id;
		int status;

		p_link = app_get_link_for_tm(app, p_tm);
		/* LINK */
		rte_eth_link_get(p_link->pmd_id, &link_eth_params);

		/* TM */
		p_tm->sched_port_params.name = p_tm->name;
		p_tm->sched_port_params.socket =
			app_get_cpu_socket_id(p_link->pmd_id);
		p_tm->sched_port_params.rate =
			(uint64_t) link_eth_params.link_speed * 1000 * 1000 / 8;

		APP_LOG(app, HIGH, "Initializing %s ...", p_tm->name);
		sched = rte_sched_port_config(&p_tm->sched_port_params);
		if (sched == NULL)
			rte_panic("%s init error\n", p_tm->name);
		app->tm[i] = sched;

		/* Subport */
		n_subports = p_tm->sched_port_params.n_subports_per_port;
		for (subport_id = 0; subport_id < n_subports; subport_id++) {
			uint32_t n_pipes_per_subport, pipe_id;

			status = rte_sched_subport_config(sched,
				subport_id,
				&p_tm->sched_subport_params[subport_id]);
			if (status)
				rte_panic("%s subport %" PRIu32
					" init error (%" PRId32 ")\n",
					p_tm->name, subport_id, status);

			/* Pipe */
			n_pipes_per_subport =
				p_tm->sched_port_params.n_pipes_per_subport;
			for (pipe_id = 0;
				pipe_id < n_pipes_per_subport;
				pipe_id++) {
				int profile_id = p_tm->sched_pipe_to_profile[
					subport_id * APP_MAX_SCHED_PIPES +
					pipe_id];

				if (profile_id == -1)
					continue;

				status = rte_sched_pipe_config(sched,
					subport_id,
					pipe_id,
					profile_id);
				if (status)
					rte_panic("%s subport %" PRIu32
						" pipe %" PRIu32
						" (profile %" PRId32 ") "
						"init error (% " PRId32 ")\n",
						p_tm->name, subport_id, pipe_id,
						profile_id, status);
			}
		}
	}
}

#ifndef RTE_EXEC_ENV_LINUXAPP
static void
app_init_tap(struct app_params *app) {
	if (app->n_pktq_tap == 0)
		return;

	rte_panic("TAP device not supported.\n");
}
#else
static void
app_init_tap(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_pktq_tap; i++) {
		struct app_pktq_tap_params *p_tap = &app->tap_params[i];
		struct ifreq ifr;
		int fd, status;

		APP_LOG(app, HIGH, "Initializing %s ...", p_tap->name);

		fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
		if (fd < 0)
			rte_panic("Cannot open file /dev/net/tun\n");

		memset(&ifr, 0, sizeof(ifr));
		ifr.ifr_flags = IFF_TAP | IFF_NO_PI; /* No packet information */
		snprintf(ifr.ifr_name, IFNAMSIZ, "%s", p_tap->name);

		status = ioctl(fd, TUNSETIFF, (void *) &ifr);
		if (status < 0)
			rte_panic("TAP setup error\n");

		app->tap[i] = fd;
	}
}
#endif

#ifdef RTE_LIBRTE_KNI
static int
kni_config_network_interface(uint8_t port_id, uint8_t if_up) {
	int ret = 0;

	if (port_id >= rte_eth_dev_count())
		return -EINVAL;

	ret = (if_up) ?
		rte_eth_dev_set_link_up(port_id) :
		rte_eth_dev_set_link_down(port_id);

	return ret;
}

static int
kni_change_mtu(uint8_t port_id, unsigned new_mtu) {
	int ret;

	if (port_id >= rte_eth_dev_count())
		return -EINVAL;

	if (new_mtu > ETHER_MAX_LEN)
		return -EINVAL;

	/* Set new MTU */
	ret = rte_eth_dev_set_mtu(port_id, new_mtu);
	if (ret < 0)
		return ret;

	return 0;
}
#endif /* RTE_LIBRTE_KNI */

#ifndef RTE_LIBRTE_KNI
static void
app_init_kni(struct app_params *app) {
	if (app->n_pktq_kni == 0)
		return;

	rte_panic("Can not init KNI without librte_kni support.\n");
}
#else
static void
app_init_kni(struct app_params *app) {
	uint32_t i;

	if (app->n_pktq_kni == 0)
		return;

	rte_kni_init(app->n_pktq_kni);

	for (i = 0; i < app->n_pktq_kni; i++) {
		struct app_pktq_kni_params *p_kni = &app->kni_params[i];
		struct app_link_params *p_link;
		struct rte_eth_dev_info dev_info;
		struct app_mempool_params *mempool_params;
		struct rte_mempool *mempool;
		struct rte_kni_conf conf;
		struct rte_kni_ops ops;

		/* LINK */
		p_link = app_get_link_for_kni(app, p_kni);
		memset(&dev_info, 0, sizeof(dev_info));
		rte_eth_dev_info_get(p_link->pmd_id, &dev_info);

		/* MEMPOOL */
		mempool_params = &app->mempool_params[p_kni->mempool_id];
		mempool = app->mempool[p_kni->mempool_id];

		/* KNI */
		memset(&conf, 0, sizeof(conf));
		snprintf(conf.name, RTE_KNI_NAMESIZE, "%s", p_kni->name);
		conf.force_bind = p_kni->force_bind;
		if (conf.force_bind) {
			int lcore_id;

			lcore_id = cpu_core_map_get_lcore_id(app->core_map,
				p_kni->socket_id,
				p_kni->core_id,
				p_kni->hyper_th_id);

			if (lcore_id < 0)
				rte_panic("%s invalid CPU core\n", p_kni->name);

			conf.core_id = (uint32_t) lcore_id;
		}
		conf.group_id = p_link->pmd_id;
		conf.mbuf_size = mempool_params->buffer_size;
		conf.addr = dev_info.pci_dev->addr;
		conf.id = dev_info.pci_dev->id;

		memset(&ops, 0, sizeof(ops));
		ops.port_id = (uint8_t) p_link->pmd_id;
		ops.change_mtu = kni_change_mtu;
		ops.config_network_if = kni_config_network_interface;

		APP_LOG(app, HIGH, "Initializing %s ...", p_kni->name);
		app->kni[i] = rte_kni_alloc(mempool, &conf, &ops);
		if (!app->kni[i])
			rte_panic("%s init error\n", p_kni->name);
	}
}
#endif /* RTE_LIBRTE_KNI */

static void
app_init_msgq(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_msgq; i++) {
		struct app_msgq_params *p = &app->msgq_params[i];

		APP_LOG(app, HIGH, "Initializing %s ...", p->name);
		app->msgq[i] = rte_ring_create(
				p->name,
				p->size,
				p->cpu_socket_id,
				RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (app->msgq[i] == NULL)
			rte_panic("%s init error\n", p->name);
	}
}

void app_pipeline_params_get(struct app_params *app,
	struct app_pipeline_params *p_in,
	struct pipeline_params *p_out)
{
	uint32_t i;

	snprintf(p_out->name, PIPELINE_NAME_SIZE, "%s", p_in->name);

	snprintf(p_out->type, PIPELINE_TYPE_SIZE, "%s", p_in->type);

	p_out->socket_id = (int) p_in->socket_id;

	p_out->log_level = app->log_level;

	/* pktq_in */
	p_out->n_ports_in = p_in->n_pktq_in;
	for (i = 0; i < p_in->n_pktq_in; i++) {
		struct app_pktq_in_params *in = &p_in->pktq_in[i];
		struct pipeline_port_in_params *out = &p_out->port_in[i];

		switch (in->type) {
		case APP_PKTQ_IN_HWQ:
		{
			struct app_pktq_hwq_in_params *p_hwq_in =
				&app->hwq_in_params[in->id];
			struct app_link_params *p_link =
				app_get_link_for_rxq(app, p_hwq_in);
			uint32_t rxq_link_id, rxq_queue_id;

			sscanf(p_hwq_in->name, "RXQ%" SCNu32 ".%" SCNu32,
				&rxq_link_id,
				&rxq_queue_id);

			out->type = PIPELINE_PORT_IN_ETHDEV_READER;
			out->params.ethdev.port_id = p_link->pmd_id;
			out->params.ethdev.queue_id = rxq_queue_id;
			out->burst_size = p_hwq_in->burst;
			break;
		}
		case APP_PKTQ_IN_SWQ:
		{
			struct app_pktq_swq_params *swq_params = &app->swq_params[in->id];

			if ((swq_params->ipv4_frag == 0) && (swq_params->ipv6_frag == 0)) {
				if (app_swq_get_readers(app, swq_params) == 1) {
					out->type = PIPELINE_PORT_IN_RING_READER;
					out->params.ring.ring = app->swq[in->id];
					out->burst_size = app->swq_params[in->id].burst_read;
				} else {
					out->type = PIPELINE_PORT_IN_RING_MULTI_READER;
					out->params.ring_multi.ring = app->swq[in->id];
					out->burst_size = swq_params->burst_read;
				}
			} else {
				if (swq_params->ipv4_frag == 1) {
					struct rte_port_ring_reader_ipv4_frag_params *params =
						&out->params.ring_ipv4_frag;

					out->type = PIPELINE_PORT_IN_RING_READER_IPV4_FRAG;
					params->ring = app->swq[in->id];
					params->mtu = swq_params->mtu;
					params->metadata_size = swq_params->metadata_size;
					params->pool_direct =
						app->mempool[swq_params->mempool_direct_id];
					params->pool_indirect =
						app->mempool[swq_params->mempool_indirect_id];
					out->burst_size = swq_params->burst_read;
				} else {
					struct rte_port_ring_reader_ipv6_frag_params *params =
						&out->params.ring_ipv6_frag;

					out->type = PIPELINE_PORT_IN_RING_READER_IPV6_FRAG;
					params->ring = app->swq[in->id];
					params->mtu = swq_params->mtu;
					params->metadata_size = swq_params->metadata_size;
					params->pool_direct =
						app->mempool[swq_params->mempool_direct_id];
					params->pool_indirect =
						app->mempool[swq_params->mempool_indirect_id];
					out->burst_size = swq_params->burst_read;
				}
			}
			break;
		}
		case APP_PKTQ_IN_TM:
		{
			out->type = PIPELINE_PORT_IN_SCHED_READER;
			out->params.sched.sched = app->tm[in->id];
			out->burst_size = app->tm_params[in->id].burst_read;
			break;
		}
#ifdef RTE_EXEC_ENV_LINUXAPP
		case APP_PKTQ_IN_TAP:
		{
			struct app_pktq_tap_params *tap_params =
				&app->tap_params[in->id];
			struct app_mempool_params *mempool_params =
				&app->mempool_params[tap_params->mempool_id];
			struct rte_mempool *mempool =
				app->mempool[tap_params->mempool_id];

			out->type = PIPELINE_PORT_IN_FD_READER;
			out->params.fd.fd = app->tap[in->id];
			out->params.fd.mtu = mempool_params->buffer_size;
			out->params.fd.mempool = mempool;
			out->burst_size = app->tap_params[in->id].burst_read;
			break;
		}
#endif
#ifdef RTE_LIBRTE_KNI
		case APP_PKTQ_IN_KNI:
		{
			out->type = PIPELINE_PORT_IN_KNI_READER;
			out->params.kni.kni = app->kni[in->id];
			out->burst_size = app->kni_params[in->id].burst_read;
			break;
		}
#endif /* RTE_LIBRTE_KNI */
		case APP_PKTQ_IN_SOURCE:
		{
			uint32_t mempool_id =
				app->source_params[in->id].mempool_id;

			out->type = PIPELINE_PORT_IN_SOURCE;
			out->params.source.mempool = app->mempool[mempool_id];
			out->burst_size = app->source_params[in->id].burst;
			out->params.source.file_name =
				app->source_params[in->id].file_name;
			out->params.source.n_bytes_per_pkt =
				app->source_params[in->id].n_bytes_per_pkt;
			break;
		}
		default:
			break;
		}
	}

	/* pktq_out */
	p_out->n_ports_out = p_in->n_pktq_out;
	for (i = 0; i < p_in->n_pktq_out; i++) {
		struct app_pktq_out_params *in = &p_in->pktq_out[i];
		struct pipeline_port_out_params *out = &p_out->port_out[i];

		switch (in->type) {
		case APP_PKTQ_OUT_HWQ:
		{
			struct app_pktq_hwq_out_params *p_hwq_out =
				&app->hwq_out_params[in->id];
			struct app_link_params *p_link =
				app_get_link_for_txq(app, p_hwq_out);
			uint32_t txq_link_id, txq_queue_id;

			sscanf(p_hwq_out->name,
				"TXQ%" SCNu32 ".%" SCNu32,
				&txq_link_id,
				&txq_queue_id);

			if (p_hwq_out->dropless == 0) {
				struct rte_port_ethdev_writer_params *params =
					&out->params.ethdev;

				out->type = PIPELINE_PORT_OUT_ETHDEV_WRITER;
				params->port_id = p_link->pmd_id;
				params->queue_id = txq_queue_id;
				params->tx_burst_sz =
					app->hwq_out_params[in->id].burst;
			} else {
				struct rte_port_ethdev_writer_nodrop_params
					*params = &out->params.ethdev_nodrop;

				out->type =
					PIPELINE_PORT_OUT_ETHDEV_WRITER_NODROP;
				params->port_id = p_link->pmd_id;
				params->queue_id = txq_queue_id;
				params->tx_burst_sz = p_hwq_out->burst;
				params->n_retries = p_hwq_out->n_retries;
			}
			break;
		}
		case APP_PKTQ_OUT_SWQ:
		{
			struct app_pktq_swq_params *swq_params = &app->swq_params[in->id];

			if ((swq_params->ipv4_ras == 0) && (swq_params->ipv6_ras == 0)) {
				if (app_swq_get_writers(app, swq_params) == 1) {
					if (app->swq_params[in->id].dropless == 0) {
						struct rte_port_ring_writer_params *params =
							&out->params.ring;

						out->type = PIPELINE_PORT_OUT_RING_WRITER;
						params->ring = app->swq[in->id];
						params->tx_burst_sz =
							app->swq_params[in->id].burst_write;
					} else {
						struct rte_port_ring_writer_nodrop_params
							*params = &out->params.ring_nodrop;

						out->type =
							PIPELINE_PORT_OUT_RING_WRITER_NODROP;
						params->ring = app->swq[in->id];
						params->tx_burst_sz =
							app->swq_params[in->id].burst_write;
						params->n_retries =
							app->swq_params[in->id].n_retries;
					}
				} else {
					if (swq_params->dropless == 0) {
						struct rte_port_ring_multi_writer_params *params =
							&out->params.ring_multi;

						out->type = PIPELINE_PORT_OUT_RING_MULTI_WRITER;
						params->ring = app->swq[in->id];
						params->tx_burst_sz = swq_params->burst_write;
					} else {
						struct rte_port_ring_multi_writer_nodrop_params
							*params = &out->params.ring_multi_nodrop;

						out->type = PIPELINE_PORT_OUT_RING_MULTI_WRITER_NODROP;
						params->ring = app->swq[in->id];
						params->tx_burst_sz = swq_params->burst_write;
						params->n_retries = swq_params->n_retries;
					}
				}
			} else {
				if (swq_params->ipv4_ras == 1) {
					struct rte_port_ring_writer_ipv4_ras_params *params =
						&out->params.ring_ipv4_ras;

					out->type = PIPELINE_PORT_OUT_RING_WRITER_IPV4_RAS;
					params->ring = app->swq[in->id];
					params->tx_burst_sz = swq_params->burst_write;
				} else {
					struct rte_port_ring_writer_ipv6_ras_params *params =
						&out->params.ring_ipv6_ras;

					out->type = PIPELINE_PORT_OUT_RING_WRITER_IPV6_RAS;
					params->ring = app->swq[in->id];
					params->tx_burst_sz = swq_params->burst_write;
				}
			}
			break;
		}
		case APP_PKTQ_OUT_TM:
		{
			struct rte_port_sched_writer_params *params =
				&out->params.sched;

			out->type = PIPELINE_PORT_OUT_SCHED_WRITER;
			params->sched = app->tm[in->id];
			params->tx_burst_sz =
				app->tm_params[in->id].burst_write;
			break;
		}
#ifdef RTE_EXEC_ENV_LINUXAPP
		case APP_PKTQ_OUT_TAP:
		{
			struct rte_port_fd_writer_params *params =
				&out->params.fd;

			out->type = PIPELINE_PORT_OUT_FD_WRITER;
			params->fd = app->tap[in->id];
			params->tx_burst_sz =
				app->tap_params[in->id].burst_write;
			break;
		}
#endif
#ifdef RTE_LIBRTE_KNI
		case APP_PKTQ_OUT_KNI:
		{
			struct app_pktq_kni_params *p_kni =
				&app->kni_params[in->id];

			if (p_kni->dropless == 0) {
				struct rte_port_kni_writer_params *params =
					&out->params.kni;

				out->type = PIPELINE_PORT_OUT_KNI_WRITER;
				params->kni = app->kni[in->id];
				params->tx_burst_sz =
					app->kni_params[in->id].burst_write;
			} else {
				struct rte_port_kni_writer_nodrop_params
					*params = &out->params.kni_nodrop;

				out->type = PIPELINE_PORT_OUT_KNI_WRITER_NODROP;
				params->kni = app->kni[in->id];
				params->tx_burst_sz =
					app->kni_params[in->id].burst_write;
				params->n_retries =
					app->kni_params[in->id].n_retries;
			}
			break;
		}
#endif /* RTE_LIBRTE_KNI */
		case APP_PKTQ_OUT_SINK:
		{
			out->type = PIPELINE_PORT_OUT_SINK;
			out->params.sink.file_name =
				app->sink_params[in->id].file_name;
			out->params.sink.max_n_pkts =
				app->sink_params[in->id].
				n_pkts_to_dump;

			break;
		}
		default:
			break;
		}
	}

	/* msgq */
	p_out->n_msgq = p_in->n_msgq_in;

	for (i = 0; i < p_in->n_msgq_in; i++)
		p_out->msgq_in[i] = app->msgq[p_in->msgq_in[i]];

	for (i = 0; i < p_in->n_msgq_out; i++)
		p_out->msgq_out[i] = app->msgq[p_in->msgq_out[i]];

	/* args */
	p_out->n_args = p_in->n_args;
	for (i = 0; i < p_in->n_args; i++) {
		p_out->args_name[i] = p_in->args_name[i];
		p_out->args_value[i] = p_in->args_value[i];
	}
}

static void
app_init_pipelines(struct app_params *app)
{
	uint32_t p_id;

	for (p_id = 0; p_id < app->n_pipelines; p_id++) {
		struct app_pipeline_params *params =
			&app->pipeline_params[p_id];
		struct app_pipeline_data *data = &app->pipeline_data[p_id];
		struct pipeline_type *ptype;
		struct pipeline_params pp;

		APP_LOG(app, HIGH, "Initializing %s ...", params->name);

		ptype = app_pipeline_type_find(app, params->type);
		if (ptype == NULL)
			rte_panic("Init error: Unknown pipeline type \"%s\"\n",
				params->type);

		app_pipeline_params_get(app, params, &pp);

		/* Back-end */
		data->be = NULL;
		if (ptype->be_ops->f_init) {
			data->be = ptype->be_ops->f_init(&pp, (void *) app);

			if (data->be == NULL)
				rte_panic("Pipeline instance \"%s\" back-end "
					"init error\n", params->name);
		}

		/* Front-end */
		data->fe = NULL;
		if (ptype->fe_ops->f_init) {
			data->fe = ptype->fe_ops->f_init(&pp, (void *) app);

			if (data->fe == NULL)
				rte_panic("Pipeline instance \"%s\" front-end "
				"init error\n", params->name);
		}

		data->ptype = ptype;

		data->timer_period = (rte_get_tsc_hz() *
			params->timer_period) / 100;
	}
}

static void
app_post_init_pipelines(struct app_params *app)
{
	uint32_t p_id;

	for (p_id = 0; p_id < app->n_pipelines; p_id++) {
		struct app_pipeline_params *params =
			&app->pipeline_params[p_id];
		struct app_pipeline_data *data = &app->pipeline_data[p_id];
		int status;

		if (data->ptype->fe_ops->f_post_init == NULL)
			continue;

		status = data->ptype->fe_ops->f_post_init(data->fe);
		if (status)
			rte_panic("Pipeline instance \"%s\" front-end "
				"post-init error\n", params->name);
	}
}

static void
app_init_threads(struct app_params *app)
{
	uint64_t time = rte_get_tsc_cycles();
	uint32_t p_id;

	for (p_id = 0; p_id < app->n_pipelines; p_id++) {
		struct app_pipeline_params *params =
			&app->pipeline_params[p_id];
		struct app_pipeline_data *data = &app->pipeline_data[p_id];
		struct pipeline_type *ptype;
		struct app_thread_data *t;
		struct app_thread_pipeline_data *p;
		int lcore_id;

		lcore_id = cpu_core_map_get_lcore_id(app->core_map,
			params->socket_id,
			params->core_id,
			params->hyper_th_id);

		if (lcore_id < 0)
			rte_panic("Invalid core s%" PRIu32 "c%" PRIu32 "%s\n",
				params->socket_id,
				params->core_id,
				(params->hyper_th_id) ? "h" : "");

		t = &app->thread_data[lcore_id];

		t->timer_period = (rte_get_tsc_hz() * APP_THREAD_TIMER_PERIOD) / 1000;
		t->thread_req_deadline = time + t->timer_period;

		t->headroom_cycles = 0;
		t->headroom_time = rte_get_tsc_cycles();
		t->headroom_ratio = 0.0;

		t->msgq_in = app_thread_msgq_in_get(app,
				params->socket_id,
				params->core_id,
				params->hyper_th_id);
		if (t->msgq_in == NULL)
			rte_panic("Init error: Cannot find MSGQ_IN for thread %" PRId32,
				lcore_id);

		t->msgq_out = app_thread_msgq_out_get(app,
				params->socket_id,
				params->core_id,
				params->hyper_th_id);
		if (t->msgq_out == NULL)
			rte_panic("Init error: Cannot find MSGQ_OUT for thread %" PRId32,
				lcore_id);

		ptype = app_pipeline_type_find(app, params->type);
		if (ptype == NULL)
			rte_panic("Init error: Unknown pipeline "
				"type \"%s\"\n", params->type);

		p = (ptype->be_ops->f_run == NULL) ?
			&t->regular[t->n_regular] :
			&t->custom[t->n_custom];

		p->pipeline_id = p_id;
		p->be = data->be;
		p->f_run = ptype->be_ops->f_run;
		p->f_timer = ptype->be_ops->f_timer;
		p->timer_period = data->timer_period;
		p->deadline = time + data->timer_period;

		data->enabled = 1;

		if (ptype->be_ops->f_run == NULL)
			t->n_regular++;
		else
			t->n_custom++;
	}
}

int app_init(struct app_params *app)
{
	app_init_core_map(app);
	app_init_core_mask(app);

	app_init_eal(app);
	app_init_mempool(app);
	app_init_link(app);
	app_init_swq(app);
	app_init_tm(app);
	app_init_tap(app);
	app_init_kni(app);
	app_init_msgq(app);

	app_pipeline_common_cmd_push(app);
	app_pipeline_thread_cmd_push(app);
	app_pipeline_type_register(app, &pipeline_master);
	app_pipeline_type_register(app, &pipeline_passthrough);
	app_pipeline_type_register(app, &pipeline_flow_classification);
	app_pipeline_type_register(app, &pipeline_flow_actions);
	app_pipeline_type_register(app, &pipeline_firewall);
	app_pipeline_type_register(app, &pipeline_routing);

	app_init_pipelines(app);
	app_init_threads(app);

	return 0;
}

int app_post_init(struct app_params *app)
{
	app_post_init_pipelines(app);

	return 0;
}

static int
app_pipeline_type_cmd_push(struct app_params *app,
	struct pipeline_type *ptype)
{
	cmdline_parse_ctx_t *cmds;
	uint32_t n_cmds, i;

	/* Check input arguments */
	if ((app == NULL) ||
		(ptype == NULL))
		return -EINVAL;

	n_cmds = pipeline_type_cmds_count(ptype);
	if (n_cmds == 0)
		return 0;

	cmds = ptype->fe_ops->cmds;

	/* Check for available slots in the application commands array */
	if (n_cmds > APP_MAX_CMDS - app->n_cmds)
		return -ENOMEM;

	/* Push pipeline commands into the application */
	memcpy(&app->cmds[app->n_cmds],
		cmds,
		n_cmds * sizeof(cmdline_parse_ctx_t));

	for (i = 0; i < n_cmds; i++)
		app->cmds[app->n_cmds + i]->data = app;

	app->n_cmds += n_cmds;
	app->cmds[app->n_cmds] = NULL;

	return 0;
}

int
app_pipeline_type_register(struct app_params *app, struct pipeline_type *ptype)
{
	uint32_t n_cmds, i;

	/* Check input arguments */
	if ((app == NULL) ||
		(ptype == NULL) ||
		(ptype->name == NULL) ||
		(strlen(ptype->name) == 0) ||
		(ptype->be_ops->f_init == NULL) ||
		(ptype->be_ops->f_timer == NULL))
		return -EINVAL;

	/* Check for duplicate entry */
	for (i = 0; i < app->n_pipeline_types; i++)
		if (strcmp(app->pipeline_type[i].name, ptype->name) == 0)
			return -EEXIST;

	/* Check for resource availability */
	n_cmds = pipeline_type_cmds_count(ptype);
	if ((app->n_pipeline_types == APP_MAX_PIPELINE_TYPES) ||
		(n_cmds > APP_MAX_CMDS - app->n_cmds))
		return -ENOMEM;

	/* Copy pipeline type */
	memcpy(&app->pipeline_type[app->n_pipeline_types++],
		ptype,
		sizeof(struct pipeline_type));

	/* Copy CLI commands */
	if (n_cmds)
		app_pipeline_type_cmd_push(app, ptype);

	return 0;
}

struct
pipeline_type *app_pipeline_type_find(struct app_params *app, char *name)
{
	uint32_t i;

	for (i = 0; i < app->n_pipeline_types; i++)
		if (strcmp(app->pipeline_type[i].name, name) == 0)
			return &app->pipeline_type[i];

	return NULL;
}
