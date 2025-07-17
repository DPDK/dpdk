/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2010-2016 Freescale Semiconductor Inc.
 * Copyright 2017-2024 NXP
 *
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>

/* This header declares the driver interface we implement */
#include <fman.h>
#include <dpaa_of.h>
#include <rte_malloc.h>
#include <rte_dpaa_logs.h>
#include <rte_string_fns.h>

int fman_ccsr_map_fd = -1;
static COMPAT_LIST_HEAD(__ifs);
static COMPAT_LIST_HEAD(__fmans);

/* This is the (const) global variable that callers have read-only access to.
 * Internally, we have read-write access directly to __ifs.
 */
const struct list_head *fman_if_list = &__ifs;
const struct list_head *fman_list = &__fmans;

static void
if_destructor(struct __fman_if *__if)
{
	struct fman_if_bpool *bp, *tmpbp;

	if (!__if)
		return;

	if (__if->__if.mac_type == fman_offline_internal)
		goto cleanup;

	list_for_each_entry_safe(bp, tmpbp, &__if->__if.bpool_list, node) {
		list_del(&bp->node);
		free(bp);
	}
cleanup:
	rte_free(__if);
}

static int
_fman_init(const struct device_node *fman_node, int fd)
{
	const struct device_node *ptp_node;
	const uint32_t *fman_addr, *ptp_addr, *cell_idx;
	uint64_t phys_addr, regs_size, lenp;
	void *vir_addr;
	uint32_t ip_rev_1;
	int _errno = 0;
	struct __fman *fman;

	fman = rte_zmalloc(NULL, sizeof(struct __fman), 0);
	if (!fman) {
		FMAN_ERR(-ENOMEM, "malloc fman");
		return -ENOMEM;
	}

	cell_idx = of_get_property(fman_node, "cell-index", &lenp);
	if (!cell_idx) {
		FMAN_ERR(-ENXIO, "%s: no cell-index", fman_node->full_name);
		return -ENXIO;
	}
	assert(lenp == sizeof(*cell_idx));
	fman->idx = of_read_number(cell_idx, lenp / sizeof(phandle));

	fman_addr = of_get_address(fman_node, 0, &regs_size, NULL);
	if (!fman_addr) {
		FMAN_ERR(-EINVAL, "Get fman's CCSR failed");
		return -EINVAL;
	}
	phys_addr = of_translate_address(fman_node, fman_addr);
	if (!phys_addr) {
		FMAN_ERR(-EINVAL, "Translate fman's CCSR failed");
		return -EINVAL;
	}
	vir_addr = mmap(NULL, regs_size, PROT_READ | PROT_WRITE,
		MAP_SHARED, fd, phys_addr);
	if (vir_addr == MAP_FAILED) {
		FMAN_ERR(-EINVAL, "Map fman's CCSR failed");
		return -EINVAL;
	}
	fman->ccsr_phy = phys_addr;
	fman->ccsr_size = regs_size;
	fman->ccsr_vir = vir_addr;

	fman->time_phy = 0;
	for_each_compatible_node(ptp_node, NULL, "fsl,fman-ptp-timer") {
		ptp_addr = of_get_address(ptp_node, 0, &regs_size, NULL);
		if (!ptp_addr)
			continue;
		phys_addr = of_translate_address(ptp_node, ptp_addr);
		if (phys_addr != (fman->ccsr_phy + fman->ccsr_size))
			continue;
		vir_addr = mmap(NULL, regs_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, phys_addr);
		if (vir_addr == MAP_FAILED) {
			FMAN_ERR(-EINVAL, "Map fman's IEEE 1588 failed");
			return -EINVAL;
		}
		fman->time_phy = phys_addr;
		fman->time_size = regs_size;
		fman->time_vir = vir_addr;
		break;
	}

	if (!fman->time_phy) {
		FMAN_ERR(-EINVAL, "Map fman's IEEE 1588 failed");
		return -EINVAL;
	}

	ip_rev_1 = in_be32((uint8_t *)fman->ccsr_vir + FMAN_IP_REV_1);
	fman->ip_rev = ip_rev_1 >> FMAN_IP_REV_1_MAJOR_SHIFT;
	fman->ip_rev &=	FMAN_IP_REV_1_MAJOR_MASK;
	DPAA_BUS_LOG(NOTICE, "FMan version is 0x%02x", fman->ip_rev);

	if (fman->ip_rev >= FMAN_V3) {
		/*
		 * Set A2V, OVOM, EBD bits in contextA to allow external
		 * buffer deallocation by fman.
		 */
		fman->dealloc_bufs_mask_hi =
			DPAA_FQD_CTX_A_A2_FIELD_VALID |
			DPAA_FQD_CTX_A_OVERRIDE_OMB;
		fman->dealloc_bufs_mask_lo = DPAA_FQD_CTX_A2_EBD_BIT;
	} else {
		fman->dealloc_bufs_mask_hi = 0;
		fman->dealloc_bufs_mask_lo = 0;
	}

	fman->fman_node = fman_node;

	list_add_tail(&fman->node, &__fmans);

	return _errno;
}

static int
fman_get_mac_index(uint64_t regs_addr_host, uint8_t *mac_idx)
{
	int ret = 0;

	/*
	 * MAC1 : E_0000h
	 * MAC2 : E_2000h
	 * MAC3 : E_4000h
	 * MAC4 : E_6000h
	 * MAC5 : E_8000h
	 * MAC6 : E_A000h
	 * MAC7 : E_C000h
	 * MAC8 : E_E000h
	 * MAC9 : F_0000h
	 * MAC10: F_2000h
	 */
	switch (regs_addr_host) {
	case 0xE0000:
		*mac_idx = 1;
		break;
	case 0xE2000:
		*mac_idx = 2;
		break;
	case 0xE4000:
		*mac_idx = 3;
		break;
	case 0xE6000:
		*mac_idx = 4;
		break;
	case 0xE8000:
		*mac_idx = 5;
		break;
	case 0xEA000:
		*mac_idx = 6;
		break;
	case 0xEC000:
		*mac_idx = 7;
		break;
	case 0xEE000:
		*mac_idx = 8;
		break;
	case 0xF0000:
		*mac_idx = 9;
		break;
	case 0xF2000:
		*mac_idx = 10;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void fman_if_vsp_init(struct __fman_if *__if)
{
	const phandle *prop;
	int cell_index;
	const struct device_node *dev;
	size_t lenp;
	const uint8_t mac_idx[] = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1};

	if (__if->__if.mac_idx <= 8) {
		for_each_compatible_node(dev, NULL,
			"fsl,fman-port-1g-rx-extended-args") {
			prop = of_get_property(dev, "cell-index", &lenp);
			if (prop) {
				cell_index = of_read_number(
						&prop[0],
						lenp / sizeof(phandle));
				if (cell_index == mac_idx[__if->__if.mac_idx]) {
					prop = of_get_property(
							dev,
							"vsp-window", &lenp);
					if (prop) {
						__if->__if.num_profiles =
							of_read_number(
								&prop[0], 1);
						__if->__if.base_profile_id =
							of_read_number(
								&prop[1], 1);
					}
				}
			}
		}

		for_each_compatible_node(dev, NULL,
					 "fsl,fman-port-op-extended-args") {
			prop = of_get_property(dev, "cell-index", &lenp);

			if (prop) {
				cell_index = of_read_number(&prop[0],
						lenp / sizeof(phandle));

				if (cell_index == __if->__if.mac_idx) {
					prop = of_get_property(dev,
							       "vsp-window",
							       &lenp);

					if (prop) {
						__if->__if.num_profiles =
							of_read_number(&prop[0],
								       1);
						__if->__if.base_profile_id =
							of_read_number(&prop[1],
								       1);
					}
				}
			}
		}
	} else {
		for_each_compatible_node(dev, NULL,
			"fsl,fman-port-10g-rx-extended-args") {
			prop = of_get_property(dev, "cell-index", &lenp);
			if (prop) {
				cell_index = of_read_number(
					&prop[0], lenp / sizeof(phandle));
				if (cell_index == mac_idx[__if->__if.mac_idx]) {
					prop = of_get_property(
						dev, "vsp-window", &lenp);
					if (prop) {
						__if->__if.num_profiles =
							of_read_number(
								&prop[0], 1);
						__if->__if.base_profile_id =
							of_read_number(
								&prop[1], 1);
					}
				}
			}
		}
	}
}

static int
fman_if_init(const struct device_node *dpa_node, int fd)
{
	const char *rprop, *mprop;
	uint64_t phys_addr;
	struct __fman_if *__if;
	struct fman_if_bpool *bpool;

	const phandle *mac_phandle, *ports_phandle, *pools_phandle;
	const phandle *tx_channel_id = NULL, *mac_addr, *cell_idx;
	const phandle *rx_phandle, *tx_phandle;
	const phandle *port_cell_idx, *ext_args_cell_idx;
	const struct device_node *parent_node_ext_args;
	uint64_t tx_phandle_host[4] = {0};
	uint64_t rx_phandle_host[6] = {0};
	uint64_t regs_addr_host = 0;
	uint64_t cell_idx_host = 0;
	uint64_t port_cell_idx_val = 0;
	uint64_t ext_args_cell_idx_val = 0;

	const struct device_node *mac_node = NULL, *ext_args_node;
	const struct device_node *pool_node, *fman_node;
	const struct device_node *rx_node = NULL, *tx_node = NULL;
	const struct device_node *oh_node = NULL;
	const uint32_t *regs_addr = NULL;
	const char *mname;
	const char *dname = dpa_node->full_name;
	size_t lenp;
	int _errno, is_shared = 0, is_offline = 0, find_fman = false;
	const char *char_prop;
	uint32_t na;
	struct __fman *fman, *tmp_fman;

	if (of_device_is_available(dpa_node) == false)
		return 0;

	if (of_device_is_compatible(dpa_node, "fsl,dpa-oh"))
		is_offline = 1;

	if (!of_device_is_compatible(dpa_node, "fsl,dpa-oh") &&
	    !of_device_is_compatible(dpa_node, "fsl,dpa-ethernet-init") &&
	    !of_device_is_compatible(dpa_node, "fsl,dpa-ethernet")) {
		return 0;
	}

	rprop = is_offline ? "fsl,qman-frame-queues-oh" :
			     "fsl,qman-frame-queues-rx";
	mprop = is_offline ? "fsl,fman-oh-port" :
			     "fsl,fman-mac";

	/* Obtain the MAC node used by this interface except macless */
	mac_phandle = of_get_property(dpa_node, mprop, &lenp);
	if (!mac_phandle) {
		FMAN_ERR(-EINVAL, "%s: no %s", dname, mprop);
		return -EINVAL;
	}
	assert(lenp == sizeof(phandle));
	mac_node = of_find_node_by_phandle(*mac_phandle);
	if (!mac_node) {
		FMAN_ERR(-ENXIO, "%s: bad 'fsl,fman-mac", dname);
		return -ENXIO;
	}
	mname = mac_node->full_name;

	if (!is_offline) {
		/* Extract the Rx and Tx ports */
		ports_phandle = of_get_property(mac_node, "fsl,port-handles",
						&lenp);
		if (!ports_phandle)
			ports_phandle = of_get_property(mac_node, "fsl,fman-ports",
							&lenp);
		if (!ports_phandle) {
			FMAN_ERR(-EINVAL, "%s: no fsl,port-handles",
				 mname);
			return -EINVAL;
		}
		assert(lenp == (2 * sizeof(phandle)));
		rx_node = of_find_node_by_phandle(ports_phandle[0]);
		if (!rx_node) {
			FMAN_ERR(-ENXIO, "%s: bad fsl,port-handle[0]", mname);
			return -ENXIO;
		}
		tx_node = of_find_node_by_phandle(ports_phandle[1]);
		if (!tx_node) {
			FMAN_ERR(-ENXIO, "%s: bad fsl,port-handle[1]", mname);
			return -ENXIO;
		}
	} else {
		/* Extract the OH ports */
		ports_phandle = of_get_property(dpa_node, "fsl,fman-oh-port",
						&lenp);
		if (!ports_phandle) {
			FMAN_ERR(-EINVAL, "%s: no fsl,fman-oh-port", dname);
			return -EINVAL;
		}
		assert(lenp == (sizeof(phandle)));
		oh_node = of_find_node_by_phandle(ports_phandle[0]);
		if (!oh_node) {
			FMAN_ERR(-ENXIO, "%s: bad fsl,port-handle[0]", mname);
			return -ENXIO;
		}
	}

	/* Check if the port is shared interface */
	if (of_device_is_compatible(dpa_node, "fsl,dpa-ethernet")) {
		port_cell_idx = of_get_property(rx_node, "cell-index", &lenp);
		if (!port_cell_idx) {
			FMAN_ERR(-ENXIO, "%s: no cell-index for port", mname);
			return -ENXIO;
		}
		assert(lenp == sizeof(*port_cell_idx));
		port_cell_idx_val =
			of_read_number(port_cell_idx, lenp / sizeof(phandle));

		if (of_device_is_compatible(rx_node, "fsl,fman-port-1g-rx"))
			port_cell_idx_val -= 0x8;
		else if (of_device_is_compatible(
				rx_node, "fsl,fman-port-10g-rx"))
			port_cell_idx_val -= 0x10;

		parent_node_ext_args = of_find_compatible_node(NULL,
			NULL, "fsl,fman-extended-args");
		if (!parent_node_ext_args)
			return 0;

		for_each_child_node(parent_node_ext_args, ext_args_node) {
			ext_args_cell_idx = of_get_property(ext_args_node,
				"cell-index", &lenp);
			if (!ext_args_cell_idx) {
				FMAN_ERR(-ENXIO, "%s: no cell-index for ext args",
					 mname);
				return -ENXIO;
			}
			assert(lenp == sizeof(*ext_args_cell_idx));
			ext_args_cell_idx_val =
				of_read_number(ext_args_cell_idx, lenp /
				sizeof(phandle));

			if (port_cell_idx_val == ext_args_cell_idx_val) {
				if (of_device_is_compatible(ext_args_node,
					"fsl,fman-port-1g-rx-extended-args") &&
					of_device_is_compatible(rx_node,
					"fsl,fman-port-1g-rx")) {
					if (of_get_property(ext_args_node,
						"vsp-window", &lenp))
						is_shared = 1;
					break;
				}
				if (of_device_is_compatible(ext_args_node,
					"fsl,fman-port-10g-rx-extended-args") &&
					of_device_is_compatible(rx_node,
					"fsl,fman-port-10g-rx")) {
					if (of_get_property(ext_args_node,
						"vsp-window", &lenp))
						is_shared = 1;
					break;
				}
			}
		}
		if (!is_shared)
			return 0;
	}

	/* Allocate an object for this network interface */
	__if = rte_malloc(NULL, sizeof(*__if), RTE_CACHE_LINE_SIZE);
	if (!__if) {
		FMAN_ERR(-ENOMEM, "malloc(%zu)", sizeof(*__if));
		goto err;
	}
	memset(__if, 0, sizeof(*__if));
	INIT_LIST_HEAD(&__if->__if.bpool_list);
	strlcpy(__if->node_name, dpa_node->name, IF_NAME_MAX_LEN - 1);
	__if->node_name[IF_NAME_MAX_LEN - 1] = '\0';
	strlcpy(__if->node_path, dpa_node->full_name, PATH_MAX - 1);
	__if->node_path[PATH_MAX - 1] = '\0';

	/* Map the CCSR regs for the MAC node */
	regs_addr = of_get_address(mac_node, 0, &__if->regs_size, NULL);
	if (!regs_addr) {
		FMAN_ERR(-EINVAL, "of_get_address(%s)", mname);
		goto err;
	}
	phys_addr = of_translate_address(mac_node, regs_addr);
	if (!phys_addr) {
		FMAN_ERR(-EINVAL, "of_translate_address(%s, %p)",
			 mname, regs_addr);
		goto err;
	}
	__if->ccsr_map = mmap(NULL, __if->regs_size,
		PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys_addr);
	if (__if->ccsr_map == MAP_FAILED) {
		FMAN_ERR(-errno, "mmap(0x%"PRIx64")", phys_addr);
		goto err;
	}
	na = of_n_addr_cells(mac_node);
	/* Get rid of endianness (issues). Convert to host byte order */
	regs_addr_host = of_read_number(regs_addr, na);

	/* Get the index of the Fman this i/f belongs to */
	fman_node = of_get_parent(mac_node);
	list_for_each_entry_safe(fman, tmp_fman, &__fmans, node) {
		if (fman_node == fman->fman_node) {
			find_fman = true;
			break;
		}
	}
	if (!find_fman) {
		FMAN_ERR(-ENXIO, "Failed to get parent of %s", mname);
		goto err;
	}
	__if->__if.fman = fman;

	/* Is the MAC node 1G, 2.5G, 10G or offline? */
	__if->__if.is_memac = 0;

	if (is_offline) {
		__if->__if.mac_type = fman_offline_internal;
	} else if (of_device_is_compatible(mac_node, "fsl,fman-memac")) {
		__if->__if.is_memac = 1;
		char_prop = of_get_property(mac_node, "phy-connection-type",
					    NULL);
		if (!char_prop) {
			FMAN_ERR(-EINVAL, "memac: unknown MII type assuming 1G");
			/* Right now forcing memac to 1g in case of error*/
			__if->__if.mac_type = fman_mac_1g;
		} else {
			if (strstr(char_prop, "sgmii-2500"))
				__if->__if.mac_type = fman_mac_2_5g;
			else if (strstr(char_prop, "sgmii"))
				__if->__if.mac_type = fman_mac_1g;
			else if (strstr(char_prop, "rgmii")) {
				__if->__if.mac_type = fman_mac_1g;
				__if->__if.is_rgmii = 1;
			} else if (strstr(char_prop, "xgmii"))
				__if->__if.mac_type = fman_mac_10g;
		}
	} else {
		FMAN_ERR(-ENOTSUP, "%s: Unsupported MAC type", mname);
		goto err;
	}

	if (!is_offline) {
		/*
		 * For MAC ports, we cannot rely on cell-index. In
		 * T2080, two of the 10G ports on single FMAN have same
		 * duplicate cell-indexes as the other two 10G ports on
		 * same FMAN. Hence, we now rely upon addresses of the
		 * ports from device tree to deduce the index.
		 */

		_errno = fman_get_mac_index(regs_addr_host, &__if->__if.mac_idx);
		if (_errno) {
			FMAN_ERR(-EINVAL, "Invalid register address: %" PRIx64,
				 regs_addr_host);
			goto err;
		}
	} else {
		cell_idx = of_get_property(oh_node, "cell-index", &lenp);
		if (!cell_idx) {
			FMAN_ERR(-ENXIO, "%s: no cell-index)",
				 oh_node->full_name);
			goto err;
		}
		assert(lenp == sizeof(*cell_idx));
		cell_idx_host = of_read_number(cell_idx,
					       lenp / sizeof(phandle));

		__if->__if.mac_idx = cell_idx_host;
	}

	if (!is_offline) {
		/* Extract the MAC address for private and shared interfaces */
		mac_addr = of_get_property(mac_node, "local-mac-address",
					   &lenp);
		if (!mac_addr) {
			FMAN_ERR(-EINVAL, "%s: no local-mac-address",
				 mname);
			goto err;
		}
		memcpy(&__if->__if.mac_addr, mac_addr, ETHER_ADDR_LEN);

		/* Extract the channel ID (from tx-port-handle) */
		tx_channel_id = of_get_property(tx_node, "fsl,qman-channel-id",
						&lenp);
		if (!tx_channel_id) {
			FMAN_ERR(-EINVAL, "%s: no fsl-qman-channel-id",
				 tx_node->full_name);
			goto err;
		}
	} else {
		/* Extract the channel ID (from mac) */
		tx_channel_id = of_get_property(mac_node, "fsl,qman-channel-id",
						&lenp);
		if (!tx_channel_id) {
			FMAN_ERR(-EINVAL, "%s: no fsl-qman-channel-id",
				 tx_node->full_name);
			goto err;
		}
	}

	na = of_n_addr_cells(mac_node);
	__if->__if.tx_channel_id = of_read_number(tx_channel_id, na);

	if (!is_offline)
		regs_addr = of_get_address(rx_node, 0, &__if->regs_size, NULL);
	else
		regs_addr = of_get_address(oh_node, 0, &__if->regs_size, NULL);
	if (!regs_addr) {
		FMAN_ERR(-EINVAL, "of_get_address(%s)", mname);
		goto err;
	}

	if (!is_offline)
		phys_addr = of_translate_address(rx_node, regs_addr);
	else
		phys_addr = of_translate_address(oh_node, regs_addr);
	if (!phys_addr) {
		FMAN_ERR(-EINVAL, "of_translate_address(%s, %p)",
			 mname, regs_addr);
		goto err;
	}

	__if->bmi_map = mmap(NULL, __if->regs_size,
		PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys_addr);
	if (__if->bmi_map == MAP_FAILED) {
		FMAN_ERR(-errno, "mmap(0x%"PRIx64")", phys_addr);
		goto err;
	}

	if (!is_offline) {
		regs_addr = of_get_address(tx_node, 0, &__if->regs_size, NULL);
		if (!regs_addr) {
			FMAN_ERR(-EINVAL, "of_get_address(%s)", mname);
			goto err;
		}

		phys_addr = of_translate_address(tx_node, regs_addr);
		if (!phys_addr) {
			FMAN_ERR(-EINVAL, "of_translate_address(%s, %p)",
				 mname, regs_addr);
			goto err;
		}

		__if->tx_bmi_map = mmap(NULL, __if->regs_size,
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys_addr);
		if (__if->tx_bmi_map == MAP_FAILED) {
			FMAN_ERR(-errno, "mmap(0x%"PRIx64")", phys_addr);
			goto err;
		}
	}

	/* Extract the Rx FQIDs. (Note, the device representation is silly,
	 * there are "counts" that must always be 1.)
	 */
	rx_phandle = of_get_property(dpa_node, rprop, &lenp);
	if (!rx_phandle) {
		FMAN_ERR(-EINVAL, "%s: no fsl,qman-frame-queues-rx", dname);
		goto err;
	}

	/*
	 * Check if "fsl,qman-frame-queues-rx/oh" in dtb file is valid entry or
	 * not.
	 *
	 * A valid rx entry contains either 4 or 6 entries. Mandatory entries
	 * are rx_error_queue, rx_error_queue_count, fqid_rx_def and
	 * fqid_rx_def_count. Optional entries are fqid_rx_pcd and
	 * fqid_rx_pcd_count.
	 *
	 * A valid oh entry contains 4 entries. Those entries are
	 * rx_error_queue, rx_error_queue_count, fqid_rx_def and
	 * fqid_rx_def_count.
	 */

	if (!is_offline)
		assert(lenp == (4 * sizeof(phandle)) ||
		       lenp == (6 * sizeof(phandle)));
	else
		assert(lenp == (4 * sizeof(phandle)));

	/* Get rid of endianness (issues). Convert to host byte order */
	rx_phandle_host[0] = of_read_number(&rx_phandle[0], na);
	rx_phandle_host[1] = of_read_number(&rx_phandle[1], na);
	rx_phandle_host[2] = of_read_number(&rx_phandle[2], na);
	rx_phandle_host[3] = of_read_number(&rx_phandle[3], na);
	rx_phandle_host[4] = of_read_number(&rx_phandle[4], na);
	rx_phandle_host[5] = of_read_number(&rx_phandle[5], na);

	assert((rx_phandle_host[1] == 1) && (rx_phandle_host[3] == 1));
	__if->__if.fqid_rx_err = rx_phandle_host[0];
	__if->__if.fqid_rx_def = rx_phandle_host[2];

	/* If there are 6 entries in "fsl,qman-frame-queues-rx" in dtb file, it
	 * means PCD queues are also available. Hence, store that information.
	 */
	if (lenp == 6 * sizeof(phandle)) {
		__if->__if.fqid_rx_pcd = rx_phandle_host[4];
		__if->__if.fqid_rx_pcd_count = rx_phandle_host[5];
	}

	if (is_offline)
		goto oh_init_done;

	/* Extract the Tx FQIDs */
	tx_phandle = of_get_property(dpa_node,
				     "fsl,qman-frame-queues-tx", &lenp);
	if (!tx_phandle) {
		FMAN_ERR(-EINVAL, "%s: no fsl,qman-frame-queues-tx", dname);
		goto err;
	}

	assert(lenp >= (4 * sizeof(phandle)));
	/*TODO: Fix for other cases also */
	na = of_n_addr_cells(mac_node);
	/* Get rid of endianness (issues). Convert to host byte order */
	tx_phandle_host[0] = of_read_number(&tx_phandle[0], na);
	tx_phandle_host[1] = of_read_number(&tx_phandle[1], na);
	tx_phandle_host[2] = of_read_number(&tx_phandle[2], na);
	tx_phandle_host[3] = of_read_number(&tx_phandle[3], na);
	assert((tx_phandle_host[1] == 1) && (tx_phandle_host[3] == 1));
	__if->__if.fqid_tx_err = tx_phandle_host[0];
	__if->__if.fqid_tx_confirm = tx_phandle_host[2];

	/* Obtain the buffer pool nodes used by this interface */
	pools_phandle = of_get_property(dpa_node, "fsl,bman-buffer-pools",
					&lenp);
	if (!pools_phandle) {
		FMAN_ERR(-EINVAL, "%s: no fsl,bman-buffer-pools", dname);
		goto err;
	}
	/* For each pool, parse the corresponding node and add a pool object
	 * to the interface's "bpool_list"
	 */
	assert(lenp && !(lenp % sizeof(phandle)));
	while (lenp) {
		size_t proplen;
		const phandle *prop;
		uint64_t bpid_host = 0;
		uint64_t bpool_host[6] = {0};
		const char *pname;
		/* Allocate an object for the pool */
		bpool = rte_malloc(NULL, sizeof(*bpool), RTE_CACHE_LINE_SIZE);
		if (!bpool) {
			FMAN_ERR(-ENOMEM, "malloc(%zu)", sizeof(*bpool));
			goto err;
		}
		/* Find the pool node */
		pool_node = of_find_node_by_phandle(*pools_phandle);
		if (!pool_node) {
			FMAN_ERR(-ENXIO, "%s: bad fsl,bman-buffer-pools",
				 dname);
			rte_free(bpool);
			goto err;
		}
		pname = pool_node->full_name;
		/* Extract the BPID property */
		prop = of_get_property(pool_node, "fsl,bpid", &proplen);
		if (!prop) {
			FMAN_ERR(-EINVAL, "%s: no fsl,bpid", pname);
			rte_free(bpool);
			goto err;
		}
		assert(proplen == sizeof(*prop));
		na = of_n_addr_cells(mac_node);
		/* Get rid of endianness (issues).
		 * Convert to host byte-order
		 */
		bpid_host = of_read_number(prop, na);
		bpool->bpid = bpid_host;
		/* Extract the cfg property (count/size/addr). "fsl,bpool-cfg"
		 * indicates for the Bman driver to seed the pool.
		 * "fsl,bpool-ethernet-cfg" is used by the network driver. The
		 * two are mutually exclusive, so check for either of them.
		 */
		prop = of_get_property(pool_node, "fsl,bpool-cfg",
				       &proplen);
		if (!prop)
			prop = of_get_property(pool_node,
					       "fsl,bpool-ethernet-cfg",
					       &proplen);
		if (!prop) {
			/* It's OK for there to be no bpool-cfg */
			bpool->count = bpool->size = bpool->addr = 0;
		} else {
			assert(proplen == (6 * sizeof(*prop)));
			na = of_n_addr_cells(mac_node);
			/* Get rid of endianness (issues).
			 * Convert to host byte order
			 */
			bpool_host[0] = of_read_number(&prop[0], na);
			bpool_host[1] = of_read_number(&prop[1], na);
			bpool_host[2] = of_read_number(&prop[2], na);
			bpool_host[3] = of_read_number(&prop[3], na);
			bpool_host[4] = of_read_number(&prop[4], na);
			bpool_host[5] = of_read_number(&prop[5], na);

			bpool->count = ((uint64_t)bpool_host[0] << 32) |
					bpool_host[1];
			bpool->size = ((uint64_t)bpool_host[2] << 32) |
					bpool_host[3];
			bpool->addr = ((uint64_t)bpool_host[4] << 32) |
					bpool_host[5];
		}
		/* Parsing of the pool is complete, add it to the interface
		 * list.
		 */
		list_add_tail(&bpool->node, &__if->__if.bpool_list);
		lenp -= sizeof(phandle);
		pools_phandle++;
	}

	if (is_shared)
		__if->__if.is_shared_mac = 1;

oh_init_done:
	fman_if_vsp_init(__if);

	/* Parsing of the network interface is complete, add it to the list */
	DPAA_BUS_LOG(DEBUG, "Found %s, Tx Channel = %x, FMAN = %x,"
		    "Port ID = %x",
		    dname, __if->__if.tx_channel_id, __if->__if.fman->idx,
		    __if->__if.mac_idx);

	/* Don't add OH port to the port list since they will be used by ONIC
	 * ports.
	 */
	if (!is_offline)
		list_add_tail(&__if->__if.node, &__ifs);

	return 0;
err:
	if_destructor(__if);
	return _errno;
}

static int fman_if_init_onic(const struct device_node *dpa_node)
{
	struct __fman_if *__if;
	struct fman_if_bpool *bpool;
	const phandle *tx_pools_phandle;
	const phandle *tx_channel_id, *mac_addr, *cell_idx;
	const phandle *rx_phandle;
	const struct device_node *pool_node;
	size_t lenp;
	int _errno;
	const phandle *p_onic_oh_nodes = NULL;
	const struct device_node *rx_oh_node = NULL;
	const struct device_node *tx_oh_node = NULL;
	const phandle *p_fman_rx_oh_node = NULL, *p_fman_tx_oh_node = NULL;
	const struct device_node *fman_rx_oh_node = NULL;
	const struct device_node *fman_tx_oh_node = NULL;
	const struct device_node *fman_node;
	uint32_t na = OF_DEFAULT_NA;
	uint64_t rx_phandle_host[4] = {0};
	uint64_t cell_idx_host = 0;
	struct __fman *fman, *tmp_fman;
	int find_fman = false;

	if (of_device_is_available(dpa_node) == false)
		return 0;

	if (!of_device_is_compatible(dpa_node, "fsl,dpa-ethernet-generic"))
		return 0;

	/* Allocate an object for this network interface */
	__if = rte_malloc(NULL, sizeof(*__if), RTE_CACHE_LINE_SIZE);
	if (!__if) {
		FMAN_ERR(-ENOMEM, "malloc(%zu)", sizeof(*__if));
		goto err;
	}
	memset(__if, 0, sizeof(*__if));

	INIT_LIST_HEAD(&__if->__if.bpool_list);

	strlcpy(__if->node_name, dpa_node->name, IF_NAME_MAX_LEN - 1);
	__if->node_name[IF_NAME_MAX_LEN - 1] = '\0';

	strlcpy(__if->node_path, dpa_node->full_name, PATH_MAX - 1);
	__if->node_path[PATH_MAX - 1] = '\0';

	/* Mac node is onic */
	__if->__if.is_memac = 0;
	__if->__if.mac_type = fman_onic;

	/* Extract the MAC address for linux peer */
	mac_addr = of_get_property(dpa_node, "local-mac-address", &lenp);
	if (!mac_addr) {
		FMAN_ERR(-EINVAL, "%s: no local-mac-address",
			 dpa_node->full_name);
		goto err;
	}

	memcpy(&__if->__if.onic_info.peer_mac, mac_addr, ETHER_ADDR_LEN);

	/* Extract the Rx port (it's the first of the two port handles)
	 * and get its channel ID.
	 */
	p_onic_oh_nodes = of_get_property(dpa_node, "fsl,oh-ports", &lenp);
	if (!p_onic_oh_nodes) {
		FMAN_ERR(-EINVAL, "%s: couldn't get p_onic_oh_nodes",
			 dpa_node->full_name);
		goto err;
	}

	rx_oh_node = of_find_node_by_phandle(p_onic_oh_nodes[0]);
	if (!rx_oh_node) {
		FMAN_ERR(-EINVAL, "%s: couldn't get rx_oh_node",
			 dpa_node->full_name);
		goto err;
	}

	p_fman_rx_oh_node = of_get_property(rx_oh_node, "fsl,fman-oh-port",
					    &lenp);
	if (!p_fman_rx_oh_node) {
		FMAN_ERR(-EINVAL, "%s: couldn't get p_fman_rx_oh_node",
			 rx_oh_node->full_name);
		goto err;
	}

	fman_rx_oh_node = of_find_node_by_phandle(*p_fman_rx_oh_node);
	if (!fman_rx_oh_node) {
		FMAN_ERR(-EINVAL, "%s: couldn't get fman_rx_oh_node",
			 rx_oh_node->full_name);
		goto err;
	}

	tx_channel_id = of_get_property(fman_rx_oh_node, "fsl,qman-channel-id",
					&lenp);
	if (!tx_channel_id) {
		FMAN_ERR(-EINVAL, "%s: no fsl-qman-channel-id",
			 rx_oh_node->full_name);
		goto err;
	}
	assert(lenp == sizeof(*tx_channel_id));

	__if->__if.tx_channel_id = of_read_number(tx_channel_id, na);

	/* Extract the FQs from which oNIC driver in Linux is dequeuing */
	rx_phandle = of_get_property(rx_oh_node, "fsl,qman-frame-queues-oh",
				     &lenp);
	if (!rx_phandle) {
		FMAN_ERR(-EINVAL, "%s: no fsl,qman-frame-queues-oh",
			 rx_oh_node->full_name);
		goto err;
	}
	assert(lenp == (4 * sizeof(phandle)));

	__if->__if.onic_info.rx_start = of_read_number(&rx_phandle[2], na);
	__if->__if.onic_info.rx_count = of_read_number(&rx_phandle[3], na);

	/* Extract the Rx FQIDs */
	tx_oh_node = of_find_node_by_phandle(p_onic_oh_nodes[1]);
	if (!tx_oh_node) {
		FMAN_ERR(-EINVAL, "%s: couldn't get tx_oh_node",
			 dpa_node->full_name);
		goto err;
	}

	p_fman_tx_oh_node = of_get_property(tx_oh_node, "fsl,fman-oh-port",
					    &lenp);
	if (!p_fman_tx_oh_node) {
		FMAN_ERR(-EINVAL, "%s: couldn't get p_fman_tx_oh_node",
			 tx_oh_node->full_name);
		goto err;
	}

	fman_tx_oh_node = of_find_node_by_phandle(*p_fman_tx_oh_node);
	if (!fman_tx_oh_node) {
		FMAN_ERR(-EINVAL, "%s: couldn't get fman_tx_oh_node",
			 tx_oh_node->full_name);
		goto err;
	}

	cell_idx = of_get_property(fman_tx_oh_node, "cell-index", &lenp);
	if (!cell_idx) {
		FMAN_ERR(-ENXIO, "%s: no cell-index)", tx_oh_node->full_name);
		goto err;
	}
	assert(lenp == sizeof(*cell_idx));

	cell_idx_host = of_read_number(cell_idx, lenp / sizeof(phandle));
	__if->__if.mac_idx = cell_idx_host;

	fman_node = of_get_parent(fman_tx_oh_node);
	list_for_each_entry_safe(fman, tmp_fman, &__fmans, node) {
		if (fman_node == fman->fman_node) {
			find_fman = true;
			break;
		}
	}
	if (!find_fman) {
		FMAN_ERR(-ENXIO, "Failed to get parent of %s",
			fman_tx_oh_node->full_name);
		goto err;
	}
	__if->__if.fman = fman;

	rx_phandle = of_get_property(tx_oh_node, "fsl,qman-frame-queues-oh",
				     &lenp);
	if (!rx_phandle) {
		FMAN_ERR(-EINVAL, "%s: no fsl,qman-frame-queues-oh",
			 dpa_node->full_name);
		goto err;
	}
	assert(lenp == (4 * sizeof(phandle)));

	rx_phandle_host[0] = of_read_number(&rx_phandle[0], na);
	rx_phandle_host[1] = of_read_number(&rx_phandle[1], na);
	rx_phandle_host[2] = of_read_number(&rx_phandle[2], na);
	rx_phandle_host[3] = of_read_number(&rx_phandle[3], na);

	assert((rx_phandle_host[1] == 1) && (rx_phandle_host[3] == 1));

	__if->__if.fqid_rx_err = rx_phandle_host[0];
	__if->__if.fqid_rx_def = rx_phandle_host[2];

	/* Don't Extract the Tx FQIDs */
	__if->__if.fqid_tx_err = 0;
	__if->__if.fqid_tx_confirm = 0;

	/* Obtain the buffer pool nodes used by Tx OH port */
	tx_pools_phandle = of_get_property(tx_oh_node, "fsl,bman-buffer-pools",
			&lenp);
	if (!tx_pools_phandle) {
		FMAN_ERR(-EINVAL, "%s: no fsl,bman-buffer-pools",
			 tx_oh_node->full_name);
		goto err;
	}
	assert(lenp && !(lenp % sizeof(phandle)));

	/* For each pool, parse the corresponding node and add a pool object to
	 * the interface's "bpool_list".
	 */

	while (lenp) {
		size_t proplen;
		const phandle *prop;
		uint64_t bpool_host[6] = {0};

		/* Allocate an object for the pool */
		bpool = rte_malloc(NULL, sizeof(*bpool), RTE_CACHE_LINE_SIZE);
		if (!bpool) {
			FMAN_ERR(-ENOMEM, "malloc(%zu)", sizeof(*bpool));
			goto err;
		}

		/* Find the pool node */
		pool_node = of_find_node_by_phandle(*tx_pools_phandle);
		if (!pool_node) {
			FMAN_ERR(-ENXIO, "%s: bad fsl,bman-buffer-pools",
				 tx_oh_node->full_name);
			rte_free(bpool);
			goto err;
		}

		/* Extract the BPID property */
		prop = of_get_property(pool_node, "fsl,bpid", &proplen);
		if (!prop) {
			FMAN_ERR(-EINVAL, "%s: no fsl,bpid",
				 pool_node->full_name);
			rte_free(bpool);
			goto err;
		}
		assert(proplen == sizeof(*prop));

		bpool->bpid = of_read_number(prop, na);

		/* Extract the cfg property (count/size/addr). "fsl,bpool-cfg"
		 * indicates for the Bman driver to seed the pool.
		 * "fsl,bpool-ethernet-cfg" is used by the network driver. The
		 * two are mutually exclusive, so check for either of them.
		 */

		prop = of_get_property(pool_node, "fsl,bpool-cfg", &proplen);
		if (!prop)
			prop = of_get_property(pool_node,
					       "fsl,bpool-ethernet-cfg",
					       &proplen);
		if (!prop) {
			/* It's OK for there to be no bpool-cfg */
			bpool->count = bpool->size = bpool->addr = 0;
		} else {
			assert(proplen == (6 * sizeof(*prop)));

			bpool_host[0] = of_read_number(&prop[0], na);
			bpool_host[1] = of_read_number(&prop[1], na);
			bpool_host[2] = of_read_number(&prop[2], na);
			bpool_host[3] = of_read_number(&prop[3], na);
			bpool_host[4] = of_read_number(&prop[4], na);
			bpool_host[5] = of_read_number(&prop[5], na);

			bpool->count = ((uint64_t)bpool_host[0] << 32) |
				       bpool_host[1];
			bpool->size = ((uint64_t)bpool_host[2] << 32) |
				      bpool_host[3];
			bpool->addr = ((uint64_t)bpool_host[4] << 32) |
				      bpool_host[5];
		}

		/* Parsing of the pool is complete, add it to the interface
		 * list.
		 */
		list_add_tail(&bpool->node, &__if->__if.bpool_list);
		lenp -= sizeof(phandle);
		tx_pools_phandle++;
	}

	fman_if_vsp_init(__if);

	/* Parsing of the network interface is complete, add it to the list. */
	DPAA_BUS_DEBUG("Found %s, Tx Channel = %x, FMAN = %x, Port ID = %x",
		       dpa_node->full_name, __if->__if.tx_channel_id,
		       __if->__if.fman->idx, __if->__if.mac_idx);

	list_add_tail(&__if->__if.node, &__ifs);
	return 0;
err:
	if_destructor(__if);
	return _errno;
}

int
fman_init(void)
{
	const struct device_node *dpa_node, *parent_node, *fman_node;
	int _errno, fd, ret;

	if (fman_ccsr_map_fd >= 0)
		return 0;

	fd = open(FMAN_DEVICE_PATH, O_RDWR);
	if (unlikely(fd < 0)) {
		DPAA_BUS_LOG(ERR, "Unable to open %s: %s", FMAN_DEVICE_PATH, strerror(errno));
		return fd;
	}
	fman_ccsr_map_fd = fd;

	parent_node = of_find_compatible_node(NULL, NULL, "fsl,dpaa");
	if (!parent_node) {
		DPAA_BUS_LOG(ERR, "Unable to find fsl,dpaa node");
		return -ENODEV;
	}

	for_each_compatible_node(fman_node, NULL, "fsl,fman") {
		ret = _fman_init(fman_node, fd);
		if (ret)
			return ret;
	}

	for_each_child_node(parent_node, dpa_node) {
		_errno = fman_if_init(dpa_node, fd);
		if (_errno) {
			FMAN_ERR(_errno, "if_init(%s)", dpa_node->full_name);
			return _errno;
		}
	}

	for_each_compatible_node(dpa_node, NULL, "fsl,dpa-ethernet-generic") {
		/* it is a oNIC interface */
		_errno = fman_if_init_onic(dpa_node);
		if (_errno)
			FMAN_ERR(_errno, "if_init(%s)", dpa_node->full_name);
	}

	return 0;
}

void
fman_finish(void)
{
	struct __fman_if *__if, *tmpif;
	struct __fman *fman, *tmpfman;
	int _errno;
	struct memac_regs *regs;
	uint32_t cfg;

	assert(fman_ccsr_map_fd != -1);

	list_for_each_entry_safe(__if, tmpif, &__ifs, __if.node) {
		/* No need to disable Offline port */
		if (__if->__if.mac_type == fman_offline_internal)
			continue;

		if (!__if->__if.is_memac) {
			DPAA_BUS_ERR("FM%d-MAC%d's MAC is not memac!",
				__if->__if.fman->idx, __if->__if.mac_idx);
			continue;
		}

		/* disable Rx and Tx */
		regs = __if->ccsr_map;
		cfg = in_be32(&regs->command_config);
		out_be32(&regs->command_config,
			cfg & (~(MEMAC_RX_ENABLE | MEMAC_TX_ENABLE)));

		/* release the mapping */
		_errno = munmap(__if->ccsr_map, __if->regs_size);
		if (unlikely(_errno < 0))
			FMAN_ERR(_errno, "munmap() = (%s)", strerror(errno));
		DPAA_BUS_INFO("Tearing down %s", __if->node_path);
		list_del(&__if->__if.node);
		rte_free(__if);
	}

	list_for_each_entry_safe(fman, tmpfman, &__fmans, node) {
		/* release the mapping */
		_errno = munmap(fman->ccsr_vir, fman->ccsr_size);
		if (unlikely(_errno < 0))
			FMAN_ERR(_errno, "munmap() = (%s)", strerror(errno));
		_errno = munmap(fman->time_vir, fman->time_size);
		if (unlikely(_errno < 0))
			FMAN_ERR(_errno, "munmap() = (%s)", strerror(errno));
		list_del(&fman->node);
		rte_free(fman);
	}

	close(fman_ccsr_map_fd);
	fman_ccsr_map_fd = -1;
}
