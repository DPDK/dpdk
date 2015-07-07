/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
#include "pipeline_routing.h"

#define APP_NAME_SIZE	32

static void
app_init_core_map(struct app_params *app)
{
	APP_LOG(app, HIGH, "Initializing CPU core map ...");
	app->core_map = cpu_core_map_init(4, 32, 4, 0);

	if (app->core_map == NULL)
		rte_panic("Cannot create CPU core map\n");

	if (app->log_level >= APP_LOG_LEVEL_LOW)
		cpu_core_map_print(app->core_map);
}

static void
app_init_core_mask(struct app_params *app)
{
	uint64_t mask = 0;
	uint32_t i;

	for (i = 0; i < app->n_pipelines; i++) {
		struct app_pipeline_params *p = &app->pipeline_params[i];
		int lcore_id;

		lcore_id = cpu_core_map_get_lcore_id(app->core_map,
			p->socket_id,
			p->core_id,
			p->hyper_th_id);

		if (lcore_id < 0)
			rte_panic("Cannot create CPU core mask\n");

		mask |= 1LLU << lcore_id;
	}

	app->core_mask = mask;
	APP_LOG(app, HIGH, "CPU core mask = 0x%016" PRIx64, app->core_mask);
}

static void
app_init_eal(struct app_params *app)
{
	char buffer[32];
	struct app_eal_params *p = &app->eal_params;
	uint32_t n_args = 0;
	int status;

	app->eal_argv[n_args++] = strdup(app->app_name);

	snprintf(buffer, sizeof(buffer), "-c%" PRIx64, app->core_mask);
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

	if (p->pci_blacklist) {
		snprintf(buffer,
			sizeof(buffer),
			"--pci-blacklist=%s",
			p->pci_blacklist);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->pci_whitelist) {
		snprintf(buffer,
			sizeof(buffer),
			"--pci-whitelist=%s",
			p->pci_whitelist);
		app->eal_argv[n_args++] = strdup(buffer);
	}

	if (p->vdev) {
		snprintf(buffer, sizeof(buffer), "--vdev=%s", p->vdev);
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
		snprintf(buffer, sizeof(buffer), "-d=%s", p->add_driver);
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
		app->mempool[i] = rte_mempool_create(
				p->name,
				p->pool_size,
				p->buffer_size,
				p->cache_size,
				sizeof(struct rte_pktmbuf_pool_private),
				rte_pktmbuf_pool_init, NULL,
				rte_pktmbuf_init, NULL,
				p->cpu_socket_id,
				0);

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
		.queue = link->tcp_syn_local_q,
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
	if (cp->tcp_syn_local_q != 0) {
		int status = app_link_filter_tcp_syn_add(cp);

		APP_LOG(app, LOW, "%s (%" PRIu32 "): "
			"Adding TCP SYN filter (queue = %" PRIu32 ")",
			cp->name, cp->pmd_id, cp->tcp_syn_local_q);

		if (status)
			rte_panic("%s (%" PRIu32 "): "
				"Error adding TCP SYN filter "
				"(queue = %" PRIu32 ") (%" PRId32 ")\n",
				cp->name, cp->pmd_id, cp->tcp_syn_local_q,
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
	if (status < 0)
		rte_panic("%s (%" PRIu32 "): PMD set up error %" PRId32 "\n",
			cp->name, cp->pmd_id, status);

	/* Mark link as UP */
	cp->state = 1;
}

void
app_link_down_internal(struct app_params *app, struct app_link_params *cp)
{
	uint32_t i;

	/* PMD link down */
	rte_eth_dev_set_link_down(cp->pmd_id);

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

		if (link_params.link_status == 0)
			all_links_up = 0;
	}

	if (all_links_up == 0)
		rte_panic("Some links are DOWN\n");
}

static void
app_init_link(struct app_params *app)
{
	uint32_t i;

	for (i = 0; i < app->n_links; i++) {
		struct app_link_params *p_link = &app->link_params[i];
		uint32_t link_id, n_hwq_in, n_hwq_out, j;
		int status;

		sscanf(p_link->name, "LINK%" PRIu32, &link_id);
		n_hwq_in = app_link_get_n_rxq(app, p_link);
		n_hwq_out = app_link_get_n_txq(app, p_link);

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

			sscanf(p_rxq->name, "RXQ%" PRIu32 ".%" PRIu32,
				&rxq_link_id, &rxq_queue_id);
			if (rxq_link_id != link_id)
				continue;

			status = rte_eth_rx_queue_setup(
				p_link->pmd_id,
				rxq_queue_id,
				p_rxq->size,
				rte_eth_dev_socket_id(p_link->pmd_id),
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

			sscanf(p_txq->name, "TXQ%" PRIu32 ".%" PRIu32,
				&txq_link_id, &txq_queue_id);
			if (txq_link_id != link_id)
				continue;

			status = rte_eth_tx_queue_setup(
				p_link->pmd_id,
				txq_queue_id,
				p_txq->size,
				rte_eth_dev_socket_id(p_link->pmd_id),
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

		/* LINK UP */
		app_link_set_arp_filter(app, p_link);
		app_link_set_tcp_syn_filter(app, p_link);
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

		APP_LOG(app, HIGH, "Initializing %s...", p->name);
		app->swq[i] = rte_ring_create(
				p->name,
				p->size,
				p->cpu_socket_id,
				RING_F_SP_ENQ | RING_F_SC_DEQ);

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
			rte_eth_dev_socket_id(p_link->pmd_id);
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

static void app_pipeline_params_get(struct app_params *app,
	struct app_pipeline_params *p_in,
	struct pipeline_params *p_out)
{
	uint32_t i;

	strcpy(p_out->name, p_in->name);

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
			out->type = PIPELINE_PORT_IN_RING_READER;
			out->params.ring.ring = app->swq[in->id];
			out->burst_size = app->swq_params[in->id].burst_read;
			/* What about frag and ras ports? */
			break;
		case APP_PKTQ_IN_TM:
			out->type = PIPELINE_PORT_IN_SCHED_READER;
			out->params.sched.sched = app->tm[in->id];
			out->burst_size = app->tm_params[in->id].burst_read;
			break;
		case APP_PKTQ_IN_SOURCE:
			out->type = PIPELINE_PORT_IN_SOURCE;
			out->params.source.mempool = app->mempool[in->id];
			out->burst_size = app->source_params[in->id].burst;
			break;
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
			/* What about frag and ras ports? */
			break;
		case APP_PKTQ_OUT_TM: {
			struct rte_port_sched_writer_params *params =
				&out->params.sched;

			out->type = PIPELINE_PORT_OUT_SCHED_WRITER;
			params->sched = app->tm[in->id];
			params->tx_burst_sz =
				app->tm_params[in->id].burst_write;
			break;
		}
		case APP_PKTQ_OUT_SINK:
			out->type = PIPELINE_PORT_OUT_SINK;
			break;
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

		data->timer_period = (rte_get_tsc_hz() * params->timer_period)
			/ 1000;
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

		ptype = app_pipeline_type_find(app, params->type);
		if (ptype == NULL)
			rte_panic("Init error: Unknown pipeline "
				"type \"%s\"\n", params->type);

		p = (ptype->be_ops->f_run == NULL) ?
			&t->regular[t->n_regular] :
			&t->custom[t->n_custom];

		p->be = data->be;
		p->f_run = ptype->be_ops->f_run;
		p->f_timer = ptype->be_ops->f_timer;
		p->timer_period = data->timer_period;
		p->deadline = time + data->timer_period;

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
	app_init_msgq(app);

	app_pipeline_common_cmd_push(app);
	app_pipeline_type_register(app, &pipeline_master);
	app_pipeline_type_register(app, &pipeline_passthrough);
	app_pipeline_type_register(app, &pipeline_flow_classification);
	app_pipeline_type_register(app, &pipeline_firewall);
	app_pipeline_type_register(app, &pipeline_routing);

	app_init_pipelines(app);
	app_init_threads(app);

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
		n_cmds * sizeof(cmdline_parse_ctx_t *));

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
