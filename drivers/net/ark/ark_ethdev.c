/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include <bus_pci_driver.h>
#include <ethdev_pci.h>
#include <rte_kvargs.h>

#include "ark_global.h"
#include "ark_logs.h"
#include "ark_ethdev_tx.h"
#include "ark_ethdev_rx.h"
#include "ark_mpu.h"
#include "ark_ddm.h"
#include "ark_udm.h"
#include "ark_pktdir.h"
#include "ark_pktgen.h"
#include "ark_pktchkr.h"

/*  Internal prototypes */
static int eth_ark_check_args(struct ark_adapter *ark, const char *params);
static int eth_ark_dev_init(struct rte_eth_dev *dev);
static int ark_config_device(struct rte_eth_dev *dev);
static int eth_ark_dev_uninit(struct rte_eth_dev *eth_dev);
static int eth_ark_dev_configure(struct rte_eth_dev *dev);
static int eth_ark_dev_start(struct rte_eth_dev *dev);
static int eth_ark_dev_stop(struct rte_eth_dev *dev);
static int eth_ark_dev_close(struct rte_eth_dev *dev);
static int eth_ark_dev_info_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);
static int eth_ark_dev_link_update(struct rte_eth_dev *dev,
				   int wait_to_complete);
static int eth_ark_dev_set_link_up(struct rte_eth_dev *dev);
static int eth_ark_dev_set_link_down(struct rte_eth_dev *dev);
static int eth_ark_dev_stats_get(struct rte_eth_dev *dev,
				  struct rte_eth_stats *stats);
static int eth_ark_dev_stats_reset(struct rte_eth_dev *dev);
static int eth_ark_set_default_mac_addr(struct rte_eth_dev *dev,
					 struct rte_ether_addr *mac_addr);
static int eth_ark_macaddr_add(struct rte_eth_dev *dev,
			       struct rte_ether_addr *mac_addr,
			       uint32_t index,
			       uint32_t pool);
static void eth_ark_macaddr_remove(struct rte_eth_dev *dev,
				   uint32_t index);
static int  eth_ark_set_mtu(struct rte_eth_dev *dev, uint16_t size);

/*
 * The packet generator is a functional block used to generate packet
 * patterns for testing.  It is not intended for nominal use.
 */
#define ARK_PKTGEN_ARG "Pkt_gen"

/*
 * The packet checker is a functional block used to verify packet
 * patterns for testing.  It is not intended for nominal use.
 */
#define ARK_PKTCHKR_ARG "Pkt_chkr"

/*
 * The packet director is used to select the internal ingress and
 * egress packets paths during testing.  It is not intended for
 * nominal use.
 */
#define ARK_PKTDIR_ARG "Pkt_dir"

/* Devinfo configurations */
#define ARK_RX_MAX_QUEUE (4096 * 4)
#define ARK_RX_MIN_QUEUE (512)
#define ARK_RX_MAX_PKT_LEN ((16 * 1024) - 128)
#define ARK_RX_MIN_BUFSIZE (1024)

#define ARK_TX_MAX_QUEUE (4096 * 4)
#define ARK_TX_MIN_QUEUE (256)

static const char * const valid_arguments[] = {
	ARK_PKTGEN_ARG,
	ARK_PKTCHKR_ARG,
	ARK_PKTDIR_ARG,
	NULL
};

#define AR_VENDOR_ID 0x1d6c
static const struct rte_pci_id pci_id_ark_map[] = {
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x100d)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x100e)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x100f)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x1010)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x1017)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x1018)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x1019)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x101a)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x101b)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x101c)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x101e)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x101f)},
	{RTE_PCI_DEVICE(AR_VENDOR_ID, 0x1022)},
	{.vendor_id = 0, /* sentinel */ },
};

/*
 * This structure is used to statically define the capabilities
 * of supported devices.
 * Capabilities:
 *    isvf -- defined for function id that are virtual
 */
struct ark_caps {
	bool isvf;
};
struct ark_dev_caps {
	uint32_t  device_id;
	struct ark_caps  caps;
};
#define SET_DEV_CAPS(id, vf)			\
	{id, {.isvf = vf} }

static const struct ark_dev_caps
ark_device_caps[] = {
		     SET_DEV_CAPS(0x100d, false),
		     SET_DEV_CAPS(0x100e, false),
		     SET_DEV_CAPS(0x100f, false),
		     SET_DEV_CAPS(0x1010, false),
		     SET_DEV_CAPS(0x1017, false),
		     SET_DEV_CAPS(0x1018, false),
		     SET_DEV_CAPS(0x1019, false),
		     SET_DEV_CAPS(0x101a, false),
		     SET_DEV_CAPS(0x101b, false),
		     SET_DEV_CAPS(0x101c, true),
		     SET_DEV_CAPS(0x101e, false),
		     SET_DEV_CAPS(0x101f, false),
		     {.device_id = 0,}
};

static int
eth_ark_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;
	int ret;

	eth_dev = rte_eth_dev_pci_allocate(pci_dev, sizeof(struct ark_adapter));

	if (eth_dev == NULL)
		return -ENOMEM;

	ret = eth_ark_dev_init(eth_dev);
	if (ret)
		rte_eth_dev_release_port(eth_dev);

	return ret;
}

static int
eth_ark_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_ark_dev_uninit);
}

static struct rte_pci_driver rte_ark_pmd = {
	.id_table = pci_id_ark_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_ark_pci_probe,
	.remove = eth_ark_pci_remove,
};

static const struct eth_dev_ops ark_eth_dev_ops = {
	.dev_configure = eth_ark_dev_configure,
	.dev_start = eth_ark_dev_start,
	.dev_stop = eth_ark_dev_stop,
	.dev_close = eth_ark_dev_close,

	.dev_infos_get = eth_ark_dev_info_get,

	.rx_queue_setup = eth_ark_dev_rx_queue_setup,
	.tx_queue_setup = eth_ark_tx_queue_setup,

	.link_update = eth_ark_dev_link_update,
	.dev_set_link_up = eth_ark_dev_set_link_up,
	.dev_set_link_down = eth_ark_dev_set_link_down,

	.rx_queue_start = eth_ark_rx_start_queue,
	.rx_queue_stop = eth_ark_rx_stop_queue,

	.tx_queue_start = eth_ark_tx_queue_start,
	.tx_queue_stop = eth_ark_tx_queue_stop,

	.stats_get = eth_ark_dev_stats_get,
	.stats_reset = eth_ark_dev_stats_reset,

	.mac_addr_add = eth_ark_macaddr_add,
	.mac_addr_remove = eth_ark_macaddr_remove,
	.mac_addr_set = eth_ark_set_default_mac_addr,

	.mtu_set = eth_ark_set_mtu,
};

static int
check_for_ext(struct ark_adapter *ark)
{
	int found = 0;

	/* Get the env */
	const char *dllpath = getenv("ARK_EXT_PATH");

	if (dllpath == NULL) {
		ARK_PMD_LOG(DEBUG, "EXT NO dll path specified\n");
		return 0;
	}
	ARK_PMD_LOG(NOTICE, "EXT found dll path at %s\n", dllpath);

	/* Open and load the .so */
	ark->d_handle = dlopen(dllpath, RTLD_LOCAL | RTLD_LAZY);
	if (ark->d_handle == NULL) {
		ARK_PMD_LOG(ERR, "Could not load user extension %s\n",
			    dllpath);
		return -1;
	}
	ARK_PMD_LOG(DEBUG, "SUCCESS: loaded user extension %s\n",
			    dllpath);

	/* Get the entry points */
	ark->user_ext.dev_init =
		(void *(*)(struct rte_eth_dev *, void *, int))
		dlsym(ark->d_handle, "rte_pmd_ark_dev_init");
	ARK_PMD_LOG(DEBUG, "device ext init pointer = %p\n",
		      ark->user_ext.dev_init);
	ark->user_ext.dev_get_port_count =
		(int (*)(struct rte_eth_dev *, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_dev_get_port_count");
	ark->user_ext.dev_uninit =
		(void (*)(struct rte_eth_dev *, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_dev_uninit");
	ark->user_ext.dev_configure =
		(int (*)(struct rte_eth_dev *, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_dev_configure");
	ark->user_ext.dev_start =
		(int (*)(struct rte_eth_dev *, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_dev_start");
	ark->user_ext.dev_stop =
		(void (*)(struct rte_eth_dev *, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_dev_stop");
	ark->user_ext.dev_close =
		(void (*)(struct rte_eth_dev *, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_dev_close");
	ark->user_ext.link_update =
		(int (*)(struct rte_eth_dev *, int, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_link_update");
	ark->user_ext.dev_set_link_up =
		(int (*)(struct rte_eth_dev *, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_dev_set_link_up");
	ark->user_ext.dev_set_link_down =
		(int (*)(struct rte_eth_dev *, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_dev_set_link_down");
	ark->user_ext.stats_get =
		(int (*)(struct rte_eth_dev *, struct rte_eth_stats *,
			  void *))
		dlsym(ark->d_handle, "rte_pmd_ark_stats_get");
	ark->user_ext.stats_reset =
		(void (*)(struct rte_eth_dev *, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_stats_reset");
	ark->user_ext.mac_addr_add =
		(void (*)(struct rte_eth_dev *, struct rte_ether_addr *,
			uint32_t, uint32_t, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_mac_addr_add");
	ark->user_ext.mac_addr_remove =
		(void (*)(struct rte_eth_dev *, uint32_t, void *))
		dlsym(ark->d_handle, "rte_pmd_ark_mac_addr_remove");
	ark->user_ext.mac_addr_set =
		(void (*)(struct rte_eth_dev *, struct rte_ether_addr *,
			  void *))
		dlsym(ark->d_handle, "rte_pmd_ark_mac_addr_set");
	ark->user_ext.set_mtu =
		(int (*)(struct rte_eth_dev *, uint16_t,
			  void *))
		dlsym(ark->d_handle, "rte_pmd_ark_set_mtu");
	ark->user_ext.rx_user_meta_hook =
		(rx_user_meta_hook_fn)dlsym(ark->d_handle,
					    "rte_pmd_ark_rx_user_meta_hook");
	ark->user_ext.tx_user_meta_hook =
		(tx_user_meta_hook_fn)dlsym(ark->d_handle,
					    "rte_pmd_ark_tx_user_meta_hook");

	return found;
}

static int
eth_ark_dev_init(struct rte_eth_dev *dev)
{
	struct ark_adapter *ark = dev->data->dev_private;
	struct rte_pci_device *pci_dev;
	int ret;
	int port_count = 1;
	int p;
	uint16_t num_queues;

	ark->eth_dev = dev;

	ARK_PMD_LOG(DEBUG, "\n");

	/* Check to see if there is an extension that we need to load */
	ret = check_for_ext(ark);
	if (ret)
		return ret;

	pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	rte_eth_copy_pci_info(dev, pci_dev);
	dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	p = 0;
	while (ark_device_caps[p].device_id != 0) {
		if (pci_dev->id.device_id == ark_device_caps[p].device_id) {
			ark->isvf = ark_device_caps[p].caps.isvf;
			break;
		}
		p++;
	}

	/* Use dummy function until setup */
	dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;

	ark->bar0 = (uint8_t *)pci_dev->mem_resource[0].addr;
	ark->a_bar = (uint8_t *)pci_dev->mem_resource[2].addr;

	ark->sysctrl.v  = (void *)&ark->bar0[ARK_SYSCTRL_BASE];
	ark->mpurx.v  = (void *)&ark->bar0[ARK_MPU_RX_BASE];
	ark->udm.v  = (void *)&ark->bar0[ARK_UDM_BASE];
	ark->mputx.v  = (void *)&ark->bar0[ARK_MPU_TX_BASE];
	ark->ddm.v  = (void *)&ark->bar0[ARK_DDM_BASE];
	ark->cmac.v  = (void *)&ark->bar0[ARK_CMAC_BASE];
	ark->external.v  = (void *)&ark->bar0[ARK_EXTERNAL_BASE];
	ark->pktdir.v  = (void *)&ark->bar0[ARK_PKTDIR_BASE];
	ark->pktgen.v  = (void *)&ark->bar0[ARK_PKTGEN_BASE];
	ark->pktchkr.v  = (void *)&ark->bar0[ARK_PKTCHKR_BASE];

	ark->started = 0;
	ark->pkt_dir_v = ARK_PKT_DIR_INIT_VAL;

	ARK_PMD_LOG(INFO, "Sys Ctrl Const = 0x%x  HW Commit_ID: %08x\n",
		      ark->sysctrl.t32[4],
		      rte_be_to_cpu_32(ark->sysctrl.t32[0x20 / 4]));
	ARK_PMD_LOG(NOTICE, "Arkville HW Commit_ID: %08x\n",
		    rte_be_to_cpu_32(ark->sysctrl.t32[0x20 / 4]));

	/* If HW sanity test fails, return an error */
	if (ark->sysctrl.t32[4] != 0xcafef00d) {
		ARK_PMD_LOG(ERR,
			    "HW Sanity test has failed, expected constant"
			    " 0x%x, read 0x%x (%s)\n",
			    0xcafef00d,
			    ark->sysctrl.t32[4], __func__);
		return -1;
	}

	ARK_PMD_LOG(DEBUG,
		    "HW Sanity test has PASSED, expected constant"
		    " 0x%x, read 0x%x (%s)\n",
		    0xcafef00d, ark->sysctrl.t32[4], __func__);

	/* We are a single function multi-port device. */
	ret = ark_config_device(dev);
	if (ret)
		return -1;

	dev->dev_ops = &ark_eth_dev_ops;
	dev->rx_queue_count = eth_ark_dev_rx_queue_count;

	dev->data->mac_addrs = rte_zmalloc("ark", RTE_ETHER_ADDR_LEN, 0);
	if (!dev->data->mac_addrs) {
		ARK_PMD_LOG(ERR,
			    "Failed to allocated memory for storing mac address"
			    );
	}

	if (ark->user_ext.dev_init) {
		ark->user_data[dev->data->port_id] =
			ark->user_ext.dev_init(dev, ark->a_bar, 0);
		if (!ark->user_data[dev->data->port_id]) {
			ARK_PMD_LOG(WARNING,
				    "Failed to initialize PMD extension!"
				    " continuing without it\n");
			memset(&ark->user_ext, 0, sizeof(struct ark_user_ext));
			dlclose(ark->d_handle);
		}
	}

	if (pci_dev->device.devargs)
		ret = eth_ark_check_args(ark, pci_dev->device.devargs->args);
	else
		ARK_PMD_LOG(INFO, "No Device args found\n");

	if (ret)
		goto error;
	/*
	 * We will create additional devices based on the number of requested
	 * ports
	 */
	if (ark->user_ext.dev_get_port_count)
		port_count =
			ark->user_ext.dev_get_port_count(dev,
				 ark->user_data[dev->data->port_id]);
	ark->num_ports = port_count;
	num_queues = ark_api_num_queues_per_port(ark->mpurx.v, port_count);

	for (p = 0; p < port_count; p++) {
		struct rte_eth_dev *eth_dev;
		char name[RTE_ETH_NAME_MAX_LEN];

		snprintf(name, sizeof(name), "arketh%d",
			 dev->data->port_id + p);

		if (p == 0) {
			/* First port is already allocated by DPDK */
			eth_dev = ark->eth_dev;
			rte_eth_dev_probing_finish(eth_dev);
			continue;
		}

		/* reserve an ethdev entry */
		eth_dev = rte_eth_dev_allocate(name);
		if (!eth_dev) {
			ARK_PMD_LOG(ERR,
				    "Could not allocate eth_dev for port %d\n",
				    p);
			goto error;
		}

		eth_dev->device = &pci_dev->device;
		/* Device requires new dev_private data */
		eth_dev->data->dev_private =
			rte_zmalloc_socket(name,
					   sizeof(struct ark_adapter),
					   RTE_CACHE_LINE_SIZE,
					   rte_socket_id());

		memcpy(eth_dev->data->dev_private, ark,
		       sizeof(struct ark_adapter));
		ark = eth_dev->data->dev_private;
		ark->qbase = p * num_queues;

		eth_dev->dev_ops = ark->eth_dev->dev_ops;
		eth_dev->tx_pkt_burst = ark->eth_dev->tx_pkt_burst;
		eth_dev->rx_pkt_burst = ark->eth_dev->rx_pkt_burst;

		rte_eth_copy_pci_info(eth_dev, pci_dev);
		eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

		eth_dev->data->mac_addrs = rte_zmalloc(name,
						RTE_ETHER_ADDR_LEN, 0);
		if (!eth_dev->data->mac_addrs) {
			ARK_PMD_LOG(ERR,
				    "Memory allocation for MAC failed!"
				    " Exiting.\n");
			goto error;
		}

		if (ark->user_ext.dev_init) {
			ark->user_data[eth_dev->data->port_id] =
				ark->user_ext.dev_init(dev, ark->a_bar, p);
		}

		rte_eth_dev_probing_finish(eth_dev);
	}

	return ret;

error:
	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;
	return -1;
}

/*
 *Initial device configuration when device is opened
 * setup the DDM, and UDM
 * Called once per PCIE device
 */
static int
ark_config_device(struct rte_eth_dev *dev)
{
	struct ark_adapter *ark = dev->data->dev_private;
	uint16_t num_q, i;
	struct ark_mpu_t *mpu;

	/*
	 * Make sure that the packet director, generator and checker are in a
	 * known state
	 */
	if (!ark->isvf) {
		ark->start_pg = 0;
		ark->pg_running = 0;
		ark->pg = ark_pktgen_init(ark->pktgen.v, 0, 1);
		if (ark->pg == NULL)
			return -1;
		ark_pktgen_reset(ark->pg);
		ark->pc = ark_pktchkr_init(ark->pktchkr.v, 0, 1);
		if (ark->pc == NULL)
			return -1;
		ark_pktchkr_stop(ark->pc);
		ark->pd = ark_pktdir_init(ark->pktdir.v);
		if (ark->pd == NULL)
			return -1;
	}
	/* Verify HW */
	if (ark_udm_verify(ark->udm.v))
		return -1;
	if (ark_ddm_verify(ark->ddm.v))
		return -1;

	/* MPU reset */
	mpu = ark->mpurx.v;
	num_q = ark_api_num_queues(mpu);
	ark->rx_queues = num_q;
	for (i = 0; i < num_q; i++) {
		mpu = RTE_PTR_ADD(mpu, ARK_MPU_QOFFSET);
	}

	mpu = ark->mputx.v;
	num_q = ark_api_num_queues(mpu);
	ark->tx_queues = num_q;
	for (i = 0; i < num_q; i++) {
		mpu = RTE_PTR_ADD(mpu, ARK_MPU_QOFFSET);
	}

	return 0;
}

static int
eth_ark_dev_uninit(struct rte_eth_dev *dev)
{
	struct ark_adapter *ark = dev->data->dev_private;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (ark->user_ext.dev_uninit)
		ark->user_ext.dev_uninit(dev,
			 ark->user_data[dev->data->port_id]);

	if (!ark->isvf) {
		ark_pktgen_uninit(ark->pg);
		ark_pktchkr_uninit(ark->pc);
	}

	return 0;
}

static int
eth_ark_dev_configure(struct rte_eth_dev *dev)
{
	struct ark_adapter *ark = dev->data->dev_private;

	eth_ark_dev_set_link_up(dev);
	if (ark->user_ext.dev_configure)
		return ark->user_ext.dev_configure(dev,
			   ark->user_data[dev->data->port_id]);
	return 0;
}

static int
eth_ark_dev_start(struct rte_eth_dev *dev)
{
	struct ark_adapter *ark = dev->data->dev_private;
	int i;

	/* RX Side */
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		eth_ark_rx_start_queue(dev, i);

	/* TX Side */
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		eth_ark_tx_queue_start(dev, i);

	ark->started = 1;
	/* set xmit and receive function */
	dev->rx_pkt_burst = &eth_ark_recv_pkts;
	dev->tx_pkt_burst = &eth_ark_xmit_pkts;

	if (!ark->isvf && ark->start_pg)
		ark_pktchkr_run(ark->pc);

	if (!ark->isvf && ark->start_pg && !ark->pg_running) {
		rte_thread_t thread;

		/* Delay packet generator start allow the hardware to be ready
		 * This is only used for sanity checking with internal generator
		 */
		char tname[RTE_THREAD_INTERNAL_NAME_SIZE];
		snprintf(tname, sizeof(tname), "ark-pg%d", dev->data->port_id);

		if (rte_thread_create_internal_control(&thread, tname,
					ark_pktgen_delay_start, ark->pg)) {
			ARK_PMD_LOG(ERR, "Could not create pktgen "
				    "starter thread\n");
			return -1;
		}
		ark->pg_running = 1;
	}

	if (ark->user_ext.dev_start)
		ark->user_ext.dev_start(dev,
			ark->user_data[dev->data->port_id]);

	return 0;
}

static int
eth_ark_dev_stop(struct rte_eth_dev *dev)
{
	uint16_t i;
	int status;
	struct ark_adapter *ark = dev->data->dev_private;

	if (ark->started == 0)
		return 0;
	ark->started = 0;
	dev->data->dev_started = 0;

	/* Stop the extension first */
	if (ark->user_ext.dev_stop)
		ark->user_ext.dev_stop(dev,
		       ark->user_data[dev->data->port_id]);

	/* Stop the packet generator */
	if (!ark->isvf && ark->start_pg && ark->pg_running) {
		ark_pktgen_pause(ark->pg);
		ark->pg_running = 0;
	}

	dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;

	/* Stop RX Side */
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		eth_ark_rx_stop_queue(dev, i);

	/* STOP TX Side */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		status = eth_ark_tx_queue_stop(dev, i);
		if (status != 0) {
			uint16_t port = dev->data->port_id;
			ARK_PMD_LOG(ERR,
				    "tx_queue stop anomaly"
				    " port %u, queue %u\n",
				    port, i);
		}
	}

	ark_udm_dump_stats(ark->udm.v, "Post stop");

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		eth_ark_rx_dump_queue(dev, i, __func__);

	/* Stop the packet checker if it is running */
	if (!ark->isvf && ark->start_pg) {
		ark_pktchkr_dump_stats(ark->pc);
		ark_pktchkr_stop(ark->pc);
	}

	return 0;
}

static int
eth_ark_dev_close(struct rte_eth_dev *dev)
{
	struct ark_adapter *ark = dev->data->dev_private;
	uint16_t i;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (ark->user_ext.dev_close)
		ark->user_ext.dev_close(dev,
		 ark->user_data[dev->data->port_id]);

	eth_ark_dev_stop(dev);

	/*
	 * This should only be called once for the device during shutdown
	 */
	/* return to power-on state */
	if (ark->pd)
		ark_pktdir_setup(ark->pd, ARK_PKT_DIR_INIT_VAL);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		eth_ark_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = 0;
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		eth_ark_dev_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = 0;
	}

	return 0;
}

static int
eth_ark_dev_info_get(struct rte_eth_dev *dev,
		     struct rte_eth_dev_info *dev_info)
{
	struct ark_adapter *ark = dev->data->dev_private;
	struct ark_mpu_t *tx_mpu = RTE_PTR_ADD(ark->bar0, ARK_MPU_TX_BASE);
	struct ark_mpu_t *rx_mpu = RTE_PTR_ADD(ark->bar0, ARK_MPU_RX_BASE);
	uint16_t ports = ark->num_ports;

	dev_info->max_rx_pktlen = ARK_RX_MAX_PKT_LEN;
	dev_info->min_rx_bufsize = ARK_RX_MIN_BUFSIZE;

	dev_info->max_rx_queues = ark_api_num_queues_per_port(rx_mpu, ports);
	dev_info->max_tx_queues = ark_api_num_queues_per_port(tx_mpu, ports);

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ARK_RX_MAX_QUEUE,
		.nb_min = ARK_RX_MIN_QUEUE,
		.nb_align = ARK_RX_MIN_QUEUE}; /* power of 2 */

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ARK_TX_MAX_QUEUE,
		.nb_min = ARK_TX_MIN_QUEUE,
		.nb_align = ARK_TX_MIN_QUEUE}; /* power of 2 */

	/* ARK PMD supports all line rates, how do we indicate that here ?? */
	dev_info->speed_capa = (RTE_ETH_LINK_SPEED_1G |
				RTE_ETH_LINK_SPEED_10G |
				RTE_ETH_LINK_SPEED_25G |
				RTE_ETH_LINK_SPEED_40G |
				RTE_ETH_LINK_SPEED_50G |
				RTE_ETH_LINK_SPEED_100G);

	dev_info->rx_offload_capa = RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	return 0;
}

static int
eth_ark_dev_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	ARK_PMD_LOG(DEBUG, "link status = %d\n",
			dev->data->dev_link.link_status);
	struct ark_adapter *ark = dev->data->dev_private;

	if (ark->user_ext.link_update) {
		return ark->user_ext.link_update
			(dev, wait_to_complete,
			 ark->user_data[dev->data->port_id]);
	}
	return 0;
}

static int
eth_ark_dev_set_link_up(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = 1;
	struct ark_adapter *ark = dev->data->dev_private;

	if (ark->user_ext.dev_set_link_up)
		return ark->user_ext.dev_set_link_up(dev,
			     ark->user_data[dev->data->port_id]);
	return 0;
}

static int
eth_ark_dev_set_link_down(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = 0;
	struct ark_adapter *ark = dev->data->dev_private;

	if (ark->user_ext.dev_set_link_down)
		return ark->user_ext.dev_set_link_down(dev,
		       ark->user_data[dev->data->port_id]);
	return 0;
}

static int
eth_ark_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	uint16_t i;
	struct ark_adapter *ark = dev->data->dev_private;

	stats->ipackets = 0;
	stats->ibytes = 0;
	stats->opackets = 0;
	stats->obytes = 0;
	stats->imissed = 0;
	stats->oerrors = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		eth_tx_queue_stats_get(dev->data->tx_queues[i], stats);
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		eth_rx_queue_stats_get(dev->data->rx_queues[i], stats);
	if (ark->user_ext.stats_get)
		return ark->user_ext.stats_get(dev, stats,
			ark->user_data[dev->data->port_id]);
	return 0;
}

static int
eth_ark_dev_stats_reset(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct ark_adapter *ark = dev->data->dev_private;

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		eth_tx_queue_stats_reset(dev->data->tx_queues[i]);
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		eth_rx_queue_stats_reset(dev->data->rx_queues[i]);
	if (ark->user_ext.stats_reset)
		ark->user_ext.stats_reset(dev,
			  ark->user_data[dev->data->port_id]);

	return 0;
}

static int
eth_ark_macaddr_add(struct rte_eth_dev *dev,
		    struct rte_ether_addr *mac_addr,
		    uint32_t index,
		    uint32_t pool)
{
	struct ark_adapter *ark = dev->data->dev_private;

	if (ark->user_ext.mac_addr_add) {
		ark->user_ext.mac_addr_add(dev,
					   mac_addr,
					   index,
					   pool,
			   ark->user_data[dev->data->port_id]);
		return 0;
	}
	return -ENOTSUP;
}

static void
eth_ark_macaddr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct ark_adapter *ark = dev->data->dev_private;

	if (ark->user_ext.mac_addr_remove)
		ark->user_ext.mac_addr_remove(dev, index,
			      ark->user_data[dev->data->port_id]);
}

static int
eth_ark_set_default_mac_addr(struct rte_eth_dev *dev,
			     struct rte_ether_addr *mac_addr)
{
	struct ark_adapter *ark = dev->data->dev_private;

	if (ark->user_ext.mac_addr_set) {
		ark->user_ext.mac_addr_set(dev, mac_addr,
			   ark->user_data[dev->data->port_id]);
		return 0;
	}
	return -ENOTSUP;
}

static int
eth_ark_set_mtu(struct rte_eth_dev *dev, uint16_t  size)
{
	struct ark_adapter *ark = dev->data->dev_private;

	if (ark->user_ext.set_mtu)
		return ark->user_ext.set_mtu(dev, size,
			     ark->user_data[dev->data->port_id]);

	return -ENOTSUP;
}

static inline int
process_pktdir_arg(const char *key, const char *value,
		   void *extra_args)
{
	ARK_PMD_LOG(DEBUG, "key = %s, value = %s\n",
		    key, value);
	struct ark_adapter *ark =
		(struct ark_adapter *)extra_args;

	ark->pkt_dir_v = strtol(value, NULL, 16);
	ARK_PMD_LOG(DEBUG, "pkt_dir_v = 0x%x\n", ark->pkt_dir_v);
	return 0;
}

static inline int
process_file_args(const char *key, const char *value, void *extra_args)
{
	ARK_PMD_LOG(DEBUG, "key = %s, value = %s\n",
		    key, value);
	char *args = (char *)extra_args;

	/* Open the configuration file */
	FILE *file = fopen(value, "r");
	char line[ARK_MAX_ARG_LEN];
	int  size = 0;
	int first = 1;

	if (file == NULL) {
		ARK_PMD_LOG(ERR, "Unable to open "
			    "config file %s\n", value);
		return -1;
	}

	while (fgets(line, sizeof(line), file)) {
		size += strlen(line);
		if (size >= ARK_MAX_ARG_LEN) {
			ARK_PMD_LOG(ERR, "Unable to parse file %s args, "
				    "parameter list is too long\n", value);
			fclose(file);
			return -1;
		}
		if (first) {
			strncpy(args, line, ARK_MAX_ARG_LEN);
			first = 0;
		} else {
			strncat(args, line, ARK_MAX_ARG_LEN);
		}
	}
	ARK_PMD_LOG(DEBUG, "file = %s\n", args);
	fclose(file);
	return 0;
}

static int
eth_ark_check_args(struct ark_adapter *ark, const char *params)
{
	struct rte_kvargs *kvlist;
	unsigned int k_idx;
	struct rte_kvargs_pair *pair = NULL;
	int ret = -1;

	kvlist = rte_kvargs_parse(params, valid_arguments);
	if (kvlist == NULL)
		return 0;

	ark->pkt_gen_args[0] = 0;
	ark->pkt_chkr_args[0] = 0;

	for (k_idx = 0; k_idx < kvlist->count; k_idx++) {
		pair = &kvlist->pairs[k_idx];
		ARK_PMD_LOG(DEBUG, "**** Arg passed to PMD = %s:%s\n",
			     pair->key,
			     pair->value);
	}

	if (rte_kvargs_process(kvlist,
			       ARK_PKTDIR_ARG,
			       &process_pktdir_arg,
			       ark) != 0) {
		ARK_PMD_LOG(ERR, "Unable to parse arg %s\n", ARK_PKTDIR_ARG);
		goto free_kvlist;
	}

	if (rte_kvargs_process(kvlist,
			       ARK_PKTGEN_ARG,
			       &process_file_args,
			       ark->pkt_gen_args) != 0) {
		ARK_PMD_LOG(ERR, "Unable to parse arg %s\n", ARK_PKTGEN_ARG);
		goto free_kvlist;
	}

	if (rte_kvargs_process(kvlist,
			       ARK_PKTCHKR_ARG,
			       &process_file_args,
			       ark->pkt_chkr_args) != 0) {
		ARK_PMD_LOG(ERR, "Unable to parse arg %s\n", ARK_PKTCHKR_ARG);
		goto free_kvlist;
	}

	if (ark->isvf) {
		ret = 0;
		goto free_kvlist;
	}
	ARK_PMD_LOG(INFO, "packet director set to 0x%x\n", ark->pkt_dir_v);
	/* Setup the packet director */
	ark_pktdir_setup(ark->pd, ark->pkt_dir_v);

	/* Setup the packet generator */
	if (ark->pkt_gen_args[0]) {
		ARK_PMD_LOG(DEBUG, "Setting up the packet generator\n");
		ark_pktgen_parse(ark->pkt_gen_args);
		ark_pktgen_reset(ark->pg);
		ark_pktgen_setup(ark->pg);
		ark->start_pg = 1;
	}

	/* Setup the packet checker */
	if (ark->pkt_chkr_args[0]) {
		ark_pktchkr_parse(ark->pkt_chkr_args);
		ark_pktchkr_setup(ark->pc);
	}

	ret = 0;

free_kvlist:
	rte_kvargs_free(kvlist);

	return ret;
}

RTE_PMD_REGISTER_PCI(net_ark, rte_ark_pmd);
RTE_PMD_REGISTER_KMOD_DEP(net_ark, "* igb_uio | uio_pci_generic ");
RTE_PMD_REGISTER_PCI_TABLE(net_ark, pci_id_ark_map);
RTE_PMD_REGISTER_PARAM_STRING(net_ark,
			      ARK_PKTGEN_ARG "=<filename> "
			      ARK_PKTCHKR_ARG "=<filename> "
			      ARK_PKTDIR_ARG "=<bitmap>");
RTE_LOG_REGISTER_DEFAULT(ark_logtype, NOTICE);
