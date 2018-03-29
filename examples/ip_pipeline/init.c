/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
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
#include <rte_bus_pci.h>

#include "app.h"
#include "pipeline.h"

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
		for (i = 0; i < app->n_links; i++) {
			struct app_link_params *p_link = &app->link_params[i];
				p_link->conf.txmode.offloads |=
						DEV_TX_OFFLOAD_MULTI_SEGS;
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
		struct rte_eth_dev_info dev_info;
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
		rte_eth_dev_info_get(p_link->pmd_id, &dev_info);
		if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
			p_link->conf.txmode.offloads |=
				DEV_TX_OFFLOAD_MBUF_FAST_FREE;
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

			p_rxq->conf.offloads = p_link->conf.rxmode.offloads;
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

			p_txq->conf.offloads = p_link->conf.txmode.offloads;
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
kni_config_network_interface(uint16_t port_id, uint8_t if_up) {
	int ret = 0;

	if (port_id >= rte_eth_dev_count())
		return -EINVAL;

	ret = (if_up) ?
		rte_eth_dev_set_link_up(port_id) :
		rte_eth_dev_set_link_down(port_id);

	return ret;
}

static int
kni_change_mtu(uint16_t port_id, unsigned int new_mtu) {
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

	return 0;
}
