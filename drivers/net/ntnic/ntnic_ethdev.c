/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>

#include <rte_eal.h>
#include <rte_dev.h>
#include <rte_vfio.h>
#include <rte_ethdev.h>
#include <rte_bus_pci.h>
#include <ethdev_pci.h>
#include <rte_kvargs.h>

#include <sys/queue.h>

#include "ntlog.h"
#include "ntdrv_4ga.h"
#include "ntos_drv.h"
#include "ntos_system.h"
#include "nthw_fpga_instances.h"
#include "ntnic_vfio.h"
#include "ntnic_mod_reg.h"
#include "nt_util.h"

#define HW_MAX_PKT_LEN (10000)
#define MAX_MTU (HW_MAX_PKT_LEN - RTE_ETHER_HDR_LEN - RTE_ETHER_CRC_LEN)

#define EXCEPTION_PATH_HID 0

#define MAX_TOTAL_QUEUES       128

/* Max RSS queues */
#define MAX_QUEUES 125

#define ETH_DEV_NTNIC_HELP_ARG "help"
#define ETH_DEV_NTHW_RXQUEUES_ARG "rxqs"
#define ETH_DEV_NTHW_TXQUEUES_ARG "txqs"

static const char *const valid_arguments[] = {
	ETH_DEV_NTNIC_HELP_ARG,
	ETH_DEV_NTHW_RXQUEUES_ARG,
	ETH_DEV_NTHW_TXQUEUES_ARG,
	NULL,
};


static const struct rte_pci_id nthw_pci_id_map[] = {
	{ RTE_PCI_DEVICE(NT_HW_PCI_VENDOR_ID, NT_HW_PCI_DEVICE_ID_NT200A02) },
	{
		.vendor_id = 0,
	},	/* sentinel */
};

static rte_spinlock_t hwlock = RTE_SPINLOCK_INITIALIZER;

/*
 * Store and get adapter info
 */

static struct drv_s *_g_p_drv[NUM_ADAPTER_MAX] = { NULL };

static void
store_pdrv(struct drv_s *p_drv)
{
	if (p_drv->adapter_no >= NUM_ADAPTER_MAX) {
		NT_LOG(ERR, NTNIC,
			"Internal error adapter number %u out of range. Max number of adapters: %u",
			p_drv->adapter_no, NUM_ADAPTER_MAX);
		return;
	}

	if (_g_p_drv[p_drv->adapter_no] != 0) {
		NT_LOG(WRN, NTNIC,
			"Overwriting adapter structure for PCI  " PCIIDENT_PRINT_STR
			" with adapter structure for PCI  " PCIIDENT_PRINT_STR,
			PCIIDENT_TO_DOMAIN(_g_p_drv[p_drv->adapter_no]->ntdrv.pciident),
			PCIIDENT_TO_BUSNR(_g_p_drv[p_drv->adapter_no]->ntdrv.pciident),
			PCIIDENT_TO_DEVNR(_g_p_drv[p_drv->adapter_no]->ntdrv.pciident),
			PCIIDENT_TO_FUNCNR(_g_p_drv[p_drv->adapter_no]->ntdrv.pciident),
			PCIIDENT_TO_DOMAIN(p_drv->ntdrv.pciident),
			PCIIDENT_TO_BUSNR(p_drv->ntdrv.pciident),
			PCIIDENT_TO_DEVNR(p_drv->ntdrv.pciident),
			PCIIDENT_TO_FUNCNR(p_drv->ntdrv.pciident));
	}

	rte_spinlock_lock(&hwlock);
	_g_p_drv[p_drv->adapter_no] = p_drv;
	rte_spinlock_unlock(&hwlock);
}

static struct drv_s *
get_pdrv_from_pci(struct rte_pci_addr addr)
{
	int i;
	struct drv_s *p_drv = NULL;
	rte_spinlock_lock(&hwlock);

	for (i = 0; i < NUM_ADAPTER_MAX; i++) {
		if (_g_p_drv[i]) {
			if (PCIIDENT_TO_DOMAIN(_g_p_drv[i]->ntdrv.pciident) == addr.domain &&
				PCIIDENT_TO_BUSNR(_g_p_drv[i]->ntdrv.pciident) == addr.bus) {
				p_drv = _g_p_drv[i];
				break;
			}
		}
	}

	rte_spinlock_unlock(&hwlock);
	return p_drv;
}

static int
eth_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete __rte_unused)
{
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	const int n_intf_no = internals->n_intf_no;
	struct adapter_info_s *p_adapter_info = &internals->p_drv->ntdrv.adapter_info;

	if (eth_dev->data->dev_started) {
		const bool port_link_status = port_ops->get_link_status(p_adapter_info, n_intf_no);
		eth_dev->data->dev_link.link_status =
			port_link_status ? RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;

		nt_link_speed_t port_link_speed =
			port_ops->get_link_speed(p_adapter_info, n_intf_no);
		eth_dev->data->dev_link.link_speed =
			nt_link_speed_to_eth_speed_num(port_link_speed);

		nt_link_duplex_t nt_link_duplex =
			port_ops->get_link_duplex(p_adapter_info, n_intf_no);
		eth_dev->data->dev_link.link_duplex = nt_link_duplex_to_eth_duplex(nt_link_duplex);

	} else {
		eth_dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;
		eth_dev->data->dev_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		eth_dev->data->dev_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	}

	return 0;
}

static int
eth_dev_infos_get(struct rte_eth_dev *eth_dev, struct rte_eth_dev_info *dev_info)
{
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	const int n_intf_no = internals->n_intf_no;
	struct adapter_info_s *p_adapter_info = &internals->p_drv->ntdrv.adapter_info;

	dev_info->driver_name = internals->name;
	dev_info->max_mac_addrs = NUM_MAC_ADDRS_PER_PORT;
	dev_info->max_rx_pktlen = HW_MAX_PKT_LEN;
	dev_info->max_mtu = MAX_MTU;

	if (internals->p_drv) {
		dev_info->max_rx_queues = internals->nb_rx_queues;
		dev_info->max_tx_queues = internals->nb_tx_queues;

		dev_info->min_rx_bufsize = 64;

		const uint32_t nt_port_speed_capa =
			port_ops->get_link_speed_capabilities(p_adapter_info, n_intf_no);
		dev_info->speed_capa = nt_link_speed_capa_to_eth_speed_capa(nt_port_speed_capa);
	}

	return 0;
}

static void eth_tx_queue_release(struct rte_eth_dev *eth_dev, uint16_t queue_id)
{
	(void)eth_dev;
	(void)queue_id;
}

static void eth_rx_queue_release(struct rte_eth_dev *eth_dev, uint16_t queue_id)
{
	(void)eth_dev;
	(void)queue_id;
}

static int num_queues_alloced;

/* Returns num queue starting at returned queue num or -1 on fail */
static int allocate_queue(int num)
{
	int next_free = num_queues_alloced;
	NT_LOG_DBGX(DBG, NTNIC, "num_queues_alloced=%u, New queues=%u, Max queues=%u",
		num_queues_alloced, num, MAX_TOTAL_QUEUES);

	if (num_queues_alloced + num > MAX_TOTAL_QUEUES)
		return -1;

	num_queues_alloced += num;
	return next_free;
}

static int eth_rx_queue_start(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	eth_dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;
}

static int eth_rx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	eth_dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

static int eth_tx_queue_start(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	eth_dev->data->tx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;
}

static int eth_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	eth_dev->data->tx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

static int
eth_mac_addr_add(struct rte_eth_dev *eth_dev,
	struct rte_ether_addr *mac_addr,
	uint32_t index,
	uint32_t vmdq __rte_unused)
{
	struct rte_ether_addr *const eth_addrs = eth_dev->data->mac_addrs;

	assert(index < NUM_MAC_ADDRS_PER_PORT);

	if (index >= NUM_MAC_ADDRS_PER_PORT) {
		const struct pmd_internals *const internals =
			(struct pmd_internals *)eth_dev->data->dev_private;
		NT_LOG_DBGX(DBG, NTNIC, "Port %i: illegal index %u (>= %u)",
			internals->n_intf_no, index, NUM_MAC_ADDRS_PER_PORT);
		return -1;
	}

	eth_addrs[index] = *mac_addr;

	return 0;
}

static int
eth_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct rte_ether_addr *const eth_addrs = dev->data->mac_addrs;

	eth_addrs[0U] = *mac_addr;

	return 0;
}

static int
eth_set_mc_addr_list(struct rte_eth_dev *eth_dev,
	struct rte_ether_addr *mc_addr_set,
	uint32_t nb_mc_addr)
{
	struct pmd_internals *const internals = (struct pmd_internals *)eth_dev->data->dev_private;
	struct rte_ether_addr *const mc_addrs = internals->mc_addrs;
	size_t i;

	if (nb_mc_addr >= NUM_MULTICAST_ADDRS_PER_PORT) {
		NT_LOG_DBGX(DBG, NTNIC,
			"Port %i: too many multicast addresses %u (>= %u)",
			internals->n_intf_no, nb_mc_addr, NUM_MULTICAST_ADDRS_PER_PORT);
		return -1;
	}

	for (i = 0U; i < NUM_MULTICAST_ADDRS_PER_PORT; i++)
		if (i < nb_mc_addr)
			mc_addrs[i] = mc_addr_set[i];

		else
			(void)memset(&mc_addrs[i], 0, sizeof(mc_addrs[i]));

	return 0;
}

static int
eth_dev_configure(struct rte_eth_dev *eth_dev)
{
	NT_LOG_DBGX(DBG, NTNIC, "Called for eth_dev %p", eth_dev);

	/* The device is ALWAYS running promiscuous mode. */
	eth_dev->data->promiscuous ^= ~eth_dev->data->promiscuous;
	return 0;
}

static int
eth_dev_start(struct rte_eth_dev *eth_dev)
{
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	const int n_intf_no = internals->n_intf_no;
	struct adapter_info_s *p_adapter_info = &internals->p_drv->ntdrv.adapter_info;

	NT_LOG_DBGX(DBG, NTNIC, "Port %u", internals->n_intf_no);

	/* Start queues */
	uint q;

	for (q = 0; q < internals->nb_rx_queues; q++)
		eth_rx_queue_start(eth_dev, q);

	for (q = 0; q < internals->nb_tx_queues; q++)
		eth_tx_queue_start(eth_dev, q);

	if (internals->type == PORT_TYPE_VIRTUAL || internals->type == PORT_TYPE_OVERRIDE) {
		eth_dev->data->dev_link.link_status = RTE_ETH_LINK_UP;

	} else {
		/* Enable the port */
		port_ops->set_adm_state(p_adapter_info, internals->n_intf_no, true);

		/*
		 * wait for link on port
		 * If application starts sending too soon before FPGA port is ready, garbage is
		 * produced
		 */
		int loop = 0;

		while (port_ops->get_link_status(p_adapter_info, n_intf_no) == RTE_ETH_LINK_DOWN) {
			/* break out after 5 sec */
			if (++loop >= 50) {
				NT_LOG_DBGX(DBG, NTNIC,
					"TIMEOUT No link on port %i (5sec timeout)",
					internals->n_intf_no);
				break;
			}

			nt_os_wait_usec(100 * 1000);
		}

		if (internals->lpbk_mode) {
			if (internals->lpbk_mode & 1 << 0) {
				port_ops->set_loopback_mode(p_adapter_info, n_intf_no,
					NT_LINK_LOOPBACK_HOST);
			}

			if (internals->lpbk_mode & 1 << 1) {
				port_ops->set_loopback_mode(p_adapter_info, n_intf_no,
					NT_LINK_LOOPBACK_LINE);
			}
		}
	}

	return 0;
}

static int
eth_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	NT_LOG_DBGX(DBG, NTNIC, "Port %u", internals->n_intf_no);

	if (internals->type != PORT_TYPE_VIRTUAL) {
		uint q;

		for (q = 0; q < internals->nb_rx_queues; q++)
			eth_rx_queue_stop(eth_dev, q);

		for (q = 0; q < internals->nb_tx_queues; q++)
			eth_tx_queue_stop(eth_dev, q);
	}

	eth_dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;
	return 0;
}

static int
eth_dev_set_link_up(struct rte_eth_dev *eth_dev)
{
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	struct pmd_internals *const internals = (struct pmd_internals *)eth_dev->data->dev_private;

	struct adapter_info_s *p_adapter_info = &internals->p_drv->ntdrv.adapter_info;
	const int port = internals->n_intf_no;

	if (internals->type == PORT_TYPE_VIRTUAL || internals->type == PORT_TYPE_OVERRIDE)
		return 0;

	assert(port >= 0 && port < NUM_ADAPTER_PORTS_MAX);
	assert(port == internals->n_intf_no);

	port_ops->set_adm_state(p_adapter_info, port, true);

	return 0;
}

static int
eth_dev_set_link_down(struct rte_eth_dev *eth_dev)
{
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	struct pmd_internals *const internals = (struct pmd_internals *)eth_dev->data->dev_private;

	struct adapter_info_s *p_adapter_info = &internals->p_drv->ntdrv.adapter_info;
	const int port = internals->n_intf_no;

	if (internals->type == PORT_TYPE_VIRTUAL || internals->type == PORT_TYPE_OVERRIDE)
		return 0;

	assert(port >= 0 && port < NUM_ADAPTER_PORTS_MAX);
	assert(port == internals->n_intf_no);

	port_ops->set_link_status(p_adapter_info, port, false);

	return 0;
}

static void
drv_deinit(struct drv_s *p_drv)
{
	const struct adapter_ops *adapter_ops = get_adapter_ops();

	if (adapter_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Adapter module uninitialized");
		return;
	}

	if (p_drv == NULL)
		return;

	ntdrv_4ga_t *p_nt_drv = &p_drv->ntdrv;

	/* stop adapter */
	adapter_ops->deinit(&p_nt_drv->adapter_info);

	/* clean memory */
	rte_free(p_drv);
	p_drv = NULL;
}

static int
eth_dev_close(struct rte_eth_dev *eth_dev)
{
	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;
	struct drv_s *p_drv = internals->p_drv;

	internals->p_drv = NULL;

	if (p_drv) {
		/* decrease initialized ethernet devices */
		p_drv->n_eth_dev_init_count--;

		/*
		 * rte_pci_dev has no private member for p_drv
		 * wait until all rte_eth_dev's are closed - then close adapters via p_drv
		 */
		if (!p_drv->n_eth_dev_init_count)
			drv_deinit(p_drv);
	}

	return 0;
}

static int
eth_fw_version_get(struct rte_eth_dev *eth_dev, char *fw_version, size_t fw_size)
{
	struct pmd_internals *internals = (struct pmd_internals *)eth_dev->data->dev_private;

	if (internals->type == PORT_TYPE_VIRTUAL || internals->type == PORT_TYPE_OVERRIDE)
		return 0;

	fpga_info_t *fpga_info = &internals->p_drv->ntdrv.adapter_info.fpga_info;
	const int length = snprintf(fw_version, fw_size, "%03d-%04d-%02d-%02d",
			fpga_info->n_fpga_type_id, fpga_info->n_fpga_prod_id,
			fpga_info->n_fpga_ver_id, fpga_info->n_fpga_rev_id);

	if ((size_t)length < fw_size) {
		/* We have space for the version string */
		return 0;

	} else {
		/* We do not have space for the version string -return the needed space */
		return length + 1;
	}
}

static int
promiscuous_enable(struct rte_eth_dev __rte_unused(*dev))
{
	NT_LOG(DBG, NTHW, "The device always run promiscuous mode");
	return 0;
}

static const struct eth_dev_ops nthw_eth_dev_ops = {
	.dev_configure = eth_dev_configure,
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_set_link_up = eth_dev_set_link_up,
	.dev_set_link_down = eth_dev_set_link_down,
	.dev_close = eth_dev_close,
	.link_update = eth_link_update,
	.dev_infos_get = eth_dev_infos_get,
	.fw_version_get = eth_fw_version_get,
	.rx_queue_start = eth_rx_queue_start,
	.rx_queue_stop = eth_rx_queue_stop,
	.rx_queue_release = eth_rx_queue_release,
	.tx_queue_start = eth_tx_queue_start,
	.tx_queue_stop = eth_tx_queue_stop,
	.tx_queue_release = eth_tx_queue_release,
	.mac_addr_add = eth_mac_addr_add,
	.mac_addr_set = eth_mac_addr_set,
	.set_mc_addr_list = eth_set_mc_addr_list,
	.promiscuous_enable = promiscuous_enable,
};

static int
nthw_pci_dev_init(struct rte_pci_device *pci_dev)
{
	nt_vfio_init();
	const struct port_ops *port_ops = get_port_ops();

	if (port_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Link management module uninitialized");
		return -1;
	}

	const struct adapter_ops *adapter_ops = get_adapter_ops();

	if (adapter_ops == NULL) {
		NT_LOG(ERR, NTNIC, "Adapter module uninitialized");
		return -1;
	}

	int res;
	struct drv_s *p_drv;
	ntdrv_4ga_t *p_nt_drv;
	hw_info_t *p_hw_info;
	fpga_info_t *fpga_info;
	uint32_t n_port_mask = -1;	/* All ports enabled by default */
	uint32_t nb_rx_queues = 1;
	uint32_t nb_tx_queues = 1;
	struct flow_queue_id_s queue_ids[MAX_QUEUES];
	int n_phy_ports;
	struct port_link_speed pls_mbps[NUM_ADAPTER_PORTS_MAX] = { 0 };
	int num_port_speeds = 0;
	NT_LOG_DBGX(DBG, NTNIC, "Dev %s PF #%i Init : %02x:%02x:%i", pci_dev->name,
		pci_dev->addr.function, pci_dev->addr.bus, pci_dev->addr.devid,
		pci_dev->addr.function);

	/*
	 * Process options/arguments
	 */
	if (pci_dev->device.devargs && pci_dev->device.devargs->args) {
		int kvargs_count;
		struct rte_kvargs *kvlist =
			rte_kvargs_parse(pci_dev->device.devargs->args, valid_arguments);

		if (kvlist == NULL)
			return -1;

		/*
		 * Argument: help
		 * NOTE: this argument/option check should be the first as it will stop
		 * execution after producing its output
		 */
		{
			if (rte_kvargs_get(kvlist, ETH_DEV_NTNIC_HELP_ARG)) {
				size_t i;

				for (i = 0; i < RTE_DIM(valid_arguments); i++)
					if (valid_arguments[i] == NULL)
						break;

				exit(0);
			}
		}

		/*
		 * rxq option/argument
		 * The number of rxq (hostbuffers) allocated in memory.
		 * Default is 32 RX Hostbuffers
		 */
		kvargs_count = rte_kvargs_count(kvlist, ETH_DEV_NTHW_RXQUEUES_ARG);

		if (kvargs_count != 0) {
			assert(kvargs_count == 1);
			res = rte_kvargs_process(kvlist, ETH_DEV_NTHW_RXQUEUES_ARG, &string_to_u32,
					&nb_rx_queues);

			if (res < 0) {
				NT_LOG_DBGX(ERR, NTNIC,
					"problem with command line arguments: res=%d",
					res);
				return -1;
			}

			NT_LOG_DBGX(DBG, NTNIC, "devargs: %s=%u",
				ETH_DEV_NTHW_RXQUEUES_ARG, nb_rx_queues);
		}

		/*
		 * txq option/argument
		 * The number of txq (hostbuffers) allocated in memory.
		 * Default is 32 TX Hostbuffers
		 */
		kvargs_count = rte_kvargs_count(kvlist, ETH_DEV_NTHW_TXQUEUES_ARG);

		if (kvargs_count != 0) {
			assert(kvargs_count == 1);
			res = rte_kvargs_process(kvlist, ETH_DEV_NTHW_TXQUEUES_ARG, &string_to_u32,
					&nb_tx_queues);

			if (res < 0) {
				NT_LOG_DBGX(ERR, NTNIC,
					"problem with command line arguments: res=%d",
					res);
				return -1;
			}

			NT_LOG_DBGX(DBG, NTNIC, "devargs: %s=%u",
				ETH_DEV_NTHW_TXQUEUES_ARG, nb_tx_queues);
		}
	}


	/* alloc */
	p_drv = rte_zmalloc_socket(pci_dev->name, sizeof(struct drv_s), RTE_CACHE_LINE_SIZE,
			pci_dev->device.numa_node);

	if (!p_drv) {
		NT_LOG_DBGX(ERR, NTNIC, "%s: error %d",
			(pci_dev->name[0] ? pci_dev->name : "NA"), -1);
		return -1;
	}

	/* Setup VFIO context */
	int vfio = nt_vfio_setup(pci_dev);

	if (vfio < 0) {
		NT_LOG_DBGX(ERR, NTNIC, "%s: vfio_setup error %d",
			(pci_dev->name[0] ? pci_dev->name : "NA"), -1);
		rte_free(p_drv);
		return -1;
	}

	/* context */
	p_nt_drv = &p_drv->ntdrv;
	p_hw_info = &p_nt_drv->adapter_info.hw_info;
	fpga_info = &p_nt_drv->adapter_info.fpga_info;

	p_drv->p_dev = pci_dev;

	/* Set context for NtDrv */
	p_nt_drv->pciident = BDF_TO_PCIIDENT(pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);
	p_nt_drv->adapter_info.n_rx_host_buffers = nb_rx_queues;
	p_nt_drv->adapter_info.n_tx_host_buffers = nb_tx_queues;

	fpga_info->bar0_addr = (void *)pci_dev->mem_resource[0].addr;
	fpga_info->bar0_size = pci_dev->mem_resource[0].len;
	fpga_info->numa_node = pci_dev->device.numa_node;
	fpga_info->pciident = p_nt_drv->pciident;
	fpga_info->adapter_no = p_drv->adapter_no;

	p_nt_drv->adapter_info.hw_info.pci_class_id = pci_dev->id.class_id;
	p_nt_drv->adapter_info.hw_info.pci_vendor_id = pci_dev->id.vendor_id;
	p_nt_drv->adapter_info.hw_info.pci_device_id = pci_dev->id.device_id;
	p_nt_drv->adapter_info.hw_info.pci_sub_vendor_id = pci_dev->id.subsystem_vendor_id;
	p_nt_drv->adapter_info.hw_info.pci_sub_device_id = pci_dev->id.subsystem_device_id;

	NT_LOG(DBG, NTNIC, "%s: " PCIIDENT_PRINT_STR " %04X:%04X: %04X:%04X:",
		p_nt_drv->adapter_info.mp_adapter_id_str, PCIIDENT_TO_DOMAIN(p_nt_drv->pciident),
		PCIIDENT_TO_BUSNR(p_nt_drv->pciident), PCIIDENT_TO_DEVNR(p_nt_drv->pciident),
		PCIIDENT_TO_FUNCNR(p_nt_drv->pciident),
		p_nt_drv->adapter_info.hw_info.pci_vendor_id,
		p_nt_drv->adapter_info.hw_info.pci_device_id,
		p_nt_drv->adapter_info.hw_info.pci_sub_vendor_id,
		p_nt_drv->adapter_info.hw_info.pci_sub_device_id);

	p_nt_drv->b_shutdown = false;
	p_nt_drv->adapter_info.pb_shutdown = &p_nt_drv->b_shutdown;

	for (int i = 0; i < num_port_speeds; ++i) {
		struct adapter_info_s *p_adapter_info = &p_nt_drv->adapter_info;
		nt_link_speed_t link_speed = convert_link_speed(pls_mbps[i].link_speed);
		port_ops->set_link_speed(p_adapter_info, i, link_speed);
	}

	/* store context */
	store_pdrv(p_drv);

	/* initialize nt4ga nthw fpga module instance in drv */
	int err = adapter_ops->init(&p_nt_drv->adapter_info);

	if (err != 0) {
		NT_LOG(ERR, NTNIC, "%s: Cannot initialize the adapter instance",
			p_nt_drv->adapter_info.mp_adapter_id_str);
		return -1;
	}

	/* Start ctrl, monitor, stat thread only for primary process. */
	if (err == 0) {
		/* mp_adapter_id_str is initialized after nt4ga_adapter_init(p_nt_drv) */
		const char *const p_adapter_id_str = p_nt_drv->adapter_info.mp_adapter_id_str;
		(void)p_adapter_id_str;
		NT_LOG(DBG, NTNIC,
			"%s: %s: AdapterPCI=" PCIIDENT_PRINT_STR " Hw=0x%02X_rev%d PhyPorts=%d",
			(pci_dev->name[0] ? pci_dev->name : "NA"), p_adapter_id_str,
			PCIIDENT_TO_DOMAIN(p_nt_drv->adapter_info.fpga_info.pciident),
			PCIIDENT_TO_BUSNR(p_nt_drv->adapter_info.fpga_info.pciident),
			PCIIDENT_TO_DEVNR(p_nt_drv->adapter_info.fpga_info.pciident),
			PCIIDENT_TO_FUNCNR(p_nt_drv->adapter_info.fpga_info.pciident),
			p_hw_info->hw_platform_id, fpga_info->nthw_hw_info.hw_id,
			fpga_info->n_phy_ports);

	} else {
		NT_LOG_DBGX(ERR, NTNIC, "%s: error=%d",
			(pci_dev->name[0] ? pci_dev->name : "NA"), err);
		return -1;
	}

	n_phy_ports = fpga_info->n_phy_ports;

	for (int n_intf_no = 0; n_intf_no < n_phy_ports; n_intf_no++) {
		const char *const p_port_id_str = p_nt_drv->adapter_info.mp_port_id_str[n_intf_no];
		(void)p_port_id_str;
		struct pmd_internals *internals = NULL;
		struct rte_eth_dev *eth_dev = NULL;
		char name[32];
		int i;

		if ((1 << n_intf_no) & ~n_port_mask) {
			NT_LOG_DBGX(DBG, NTNIC,
				"%s: interface #%d: skipping due to portmask 0x%02X",
				p_port_id_str, n_intf_no, n_port_mask);
			continue;
		}

		snprintf(name, sizeof(name), "ntnic%d", n_intf_no);
		NT_LOG_DBGX(DBG, NTNIC, "%s: interface #%d: %s: '%s'", p_port_id_str,
			n_intf_no, (pci_dev->name[0] ? pci_dev->name : "NA"), name);

		internals = rte_zmalloc_socket(name, sizeof(struct pmd_internals),
				RTE_CACHE_LINE_SIZE, pci_dev->device.numa_node);

		if (!internals) {
			NT_LOG_DBGX(ERR, NTNIC, "%s: %s: error=%d",
				(pci_dev->name[0] ? pci_dev->name : "NA"), name, -1);
			return -1;
		}

		internals->pci_dev = pci_dev;
		internals->n_intf_no = n_intf_no;
		internals->type = PORT_TYPE_PHYSICAL;
		internals->nb_rx_queues = nb_rx_queues;
		internals->nb_tx_queues = nb_tx_queues;

		/* Not used queue index as dest port in bypass - use 0x80 + port nr */
		for (i = 0; i < MAX_QUEUES; i++)
			internals->vpq[i].hw_id = -1;


		/* Setup queue_ids */
		if (nb_rx_queues > 1) {
			NT_LOG(DBG, NTNIC,
				"(%i) NTNIC configured with Rx multi queues. %i queues",
				internals->n_intf_no, nb_rx_queues);
		}

		if (nb_tx_queues > 1) {
			NT_LOG(DBG, NTNIC,
				"(%i) NTNIC configured with Tx multi queues. %i queues",
				internals->n_intf_no, nb_tx_queues);
		}

		int max_num_queues = (nb_rx_queues > nb_tx_queues) ? nb_rx_queues : nb_tx_queues;
		int start_queue = allocate_queue(max_num_queues);

		if (start_queue < 0)
			return -1;

		for (i = 0; i < (int)max_num_queues; i++) {
			queue_ids[i].id = i;
			queue_ids[i].hw_id = start_queue + i;

			internals->rxq_scg[i].queue = queue_ids[i];
			/* use same index in Rx and Tx rings */
			internals->txq_scg[i].queue = queue_ids[i];
			internals->rxq_scg[i].enabled = 0;
			internals->txq_scg[i].type = internals->type;
			internals->rxq_scg[i].type = internals->type;
			internals->rxq_scg[i].port = internals->port;
		}

		/* no tx queues - tx data goes out on phy */
		internals->vpq_nb_vq = 0;

		for (i = 0; i < (int)nb_tx_queues; i++) {
			internals->txq_scg[i].port = internals->port;
			internals->txq_scg[i].enabled = 0;
		}

		/* Set MAC address (but only if the MAC address is permitted) */
		if (n_intf_no < fpga_info->nthw_hw_info.vpd_info.mn_mac_addr_count) {
			const uint64_t mac =
				fpga_info->nthw_hw_info.vpd_info.mn_mac_addr_value + n_intf_no;
			internals->eth_addrs[0].addr_bytes[0] = (mac >> 40) & 0xFFu;
			internals->eth_addrs[0].addr_bytes[1] = (mac >> 32) & 0xFFu;
			internals->eth_addrs[0].addr_bytes[2] = (mac >> 24) & 0xFFu;
			internals->eth_addrs[0].addr_bytes[3] = (mac >> 16) & 0xFFu;
			internals->eth_addrs[0].addr_bytes[4] = (mac >> 8) & 0xFFu;
			internals->eth_addrs[0].addr_bytes[5] = (mac >> 0) & 0xFFu;
		}

		eth_dev = rte_eth_dev_allocate(name);

		if (!eth_dev) {
			NT_LOG_DBGX(ERR, NTNIC, "%s: %s: error=%d",
				(pci_dev->name[0] ? pci_dev->name : "NA"), name, -1);
			return -1;
		}

		/* connect structs */
		internals->p_drv = p_drv;
		eth_dev->data->dev_private = internals;
		eth_dev->data->mac_addrs = rte_malloc(NULL,
					NUM_MAC_ADDRS_PER_PORT * sizeof(struct rte_ether_addr), 0);
		rte_memcpy(&eth_dev->data->mac_addrs[0],
					&internals->eth_addrs[0], RTE_ETHER_ADDR_LEN);


		struct rte_eth_link pmd_link;
		pmd_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
		pmd_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		pmd_link.link_status = RTE_ETH_LINK_DOWN;
		pmd_link.link_autoneg = RTE_ETH_LINK_AUTONEG;

		eth_dev->device = &pci_dev->device;
		eth_dev->data->dev_link = pmd_link;
		eth_dev->dev_ops = &nthw_eth_dev_ops;

		eth_dev_pci_specific_init(eth_dev, pci_dev);
		rte_eth_dev_probing_finish(eth_dev);

		/* increase initialized ethernet devices - PF */
		p_drv->n_eth_dev_init_count++;
	}

	return 0;
}

static int
nthw_pci_dev_deinit(struct rte_eth_dev *eth_dev __rte_unused)
{
	NT_LOG_DBGX(DBG, NTNIC, "PCI device deinitialization");

	int i;
	char name[32];

	struct pmd_internals *internals = eth_dev->data->dev_private;
	ntdrv_4ga_t *p_ntdrv = &internals->p_drv->ntdrv;
	fpga_info_t *fpga_info = &p_ntdrv->adapter_info.fpga_info;
	const int n_phy_ports = fpga_info->n_phy_ports;
	for (i = 0; i < n_phy_ports; i++) {
		sprintf(name, "ntnic%d", i);
		eth_dev = rte_eth_dev_allocated(name);
		if (eth_dev == NULL)
			continue; /* port already released */
		rte_eth_dev_release_port(eth_dev);
	}

	nt_vfio_remove(EXCEPTION_PATH_HID);
	return 0;
}

static int
nthw_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	int ret;

	NT_LOG_DBGX(DBG, NTNIC, "pcidev: name: '%s'", pci_dev->name);
	NT_LOG_DBGX(DBG, NTNIC, "devargs: name: '%s'", pci_dev->device.name);

	if (pci_dev->device.devargs) {
		NT_LOG_DBGX(DBG, NTNIC, "devargs: args: '%s'",
			(pci_dev->device.devargs->args ? pci_dev->device.devargs->args : "NULL"));
		NT_LOG_DBGX(DBG, NTNIC, "devargs: data: '%s'",
			(pci_dev->device.devargs->data ? pci_dev->device.devargs->data : "NULL"));
	}

	const int n_rte_vfio_no_io_mmu_enabled = rte_vfio_noiommu_is_enabled();
	NT_LOG(DBG, NTNIC, "vfio_no_iommu_enabled=%d", n_rte_vfio_no_io_mmu_enabled);

	if (n_rte_vfio_no_io_mmu_enabled) {
		NT_LOG(ERR, NTNIC, "vfio_no_iommu_enabled=%d: this PMD needs VFIO IOMMU",
			n_rte_vfio_no_io_mmu_enabled);
		return -1;
	}

	const enum rte_iova_mode n_rte_io_va_mode = rte_eal_iova_mode();
	NT_LOG(DBG, NTNIC, "iova mode=%d", n_rte_io_va_mode);

	NT_LOG(DBG, NTNIC,
		"busid=" PCI_PRI_FMT
		" pciid=%04x:%04x_%04x:%04x locstr=%s @ numanode=%d: drv=%s drvalias=%s",
		pci_dev->addr.domain, pci_dev->addr.bus, pci_dev->addr.devid,
		pci_dev->addr.function, pci_dev->id.vendor_id, pci_dev->id.device_id,
		pci_dev->id.subsystem_vendor_id, pci_dev->id.subsystem_device_id,
		pci_dev->name[0] ? pci_dev->name : "NA",
		pci_dev->device.numa_node,
		pci_dev->driver->driver.name ? pci_dev->driver->driver.name : "NA",
		pci_dev->driver->driver.alias ? pci_dev->driver->driver.alias : "NA");


	ret = nthw_pci_dev_init(pci_dev);

	NT_LOG_DBGX(DBG, NTNIC, "leave: ret=%d", ret);
	return ret;
}

static int
nthw_pci_remove(struct rte_pci_device *pci_dev)
{
	NT_LOG_DBGX(DBG, NTNIC);

	struct drv_s *p_drv = get_pdrv_from_pci(pci_dev->addr);
	drv_deinit(p_drv);

	return rte_eth_dev_pci_generic_remove(pci_dev, nthw_pci_dev_deinit);
}

static struct rte_pci_driver rte_nthw_pmd = {
	.id_table = nthw_pci_id_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = nthw_pci_probe,
	.remove = nthw_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_ntnic, rte_nthw_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ntnic, nthw_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ntnic, "* vfio-pci");

RTE_LOG_REGISTER_SUFFIX(nt_log_general, general, INFO);
RTE_LOG_REGISTER_SUFFIX(nt_log_nthw, nthw, INFO);
RTE_LOG_REGISTER_SUFFIX(nt_log_filter, filter, INFO);
RTE_LOG_REGISTER_SUFFIX(nt_log_ntnic, ntnic, INFO);
