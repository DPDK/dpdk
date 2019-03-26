/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2010-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>

/* This header declares the driver interface we implement */
#include <fman.h>
#include <of.h>
#include <rte_malloc.h>
#include <rte_dpaa_logs.h>
#include <rte_string_fns.h>

#define QMI_PORT_REGS_OFFSET		0x400

/* CCSR map address to access ccsr based register */
void *fman_ccsr_map;
/* fman version info */
u16 fman_ip_rev;
static int get_once;
u32 fman_dealloc_bufs_mask_hi;
u32 fman_dealloc_bufs_mask_lo;

int fman_ccsr_map_fd = -1;
static COMPAT_LIST_HEAD(__ifs);

/* This is the (const) global variable that callers have read-only access to.
 * Internally, we have read-write access directly to __ifs.
 */
const struct list_head *fman_if_list = &__ifs;

static void
if_destructor(struct __fman_if *__if)
{
	struct fman_if_bpool *bp, *tmpbp;

	if (!__if)
		return;

	if (__if->__if.mac_type == fman_offline)
		goto cleanup;

	list_for_each_entry_safe(bp, tmpbp, &__if->__if.bpool_list, node) {
		list_del(&bp->node);
		free(bp);
	}
cleanup:
	free(__if);
}

static int
fman_get_ip_rev(const struct device_node *fman_node)
{
	const uint32_t *fman_addr;
	uint64_t phys_addr;
	uint64_t regs_size;
	uint32_t ip_rev_1;
	int _errno;

	fman_addr = of_get_address(fman_node, 0, &regs_size, NULL);
	if (!fman_addr) {
		pr_err("of_get_address cannot return fman address\n");
		return -EINVAL;
	}
	phys_addr = of_translate_address(fman_node, fman_addr);
	if (!phys_addr) {
		pr_err("of_translate_address failed\n");
		return -EINVAL;
	}
	fman_ccsr_map = mmap(NULL, regs_size, PROT_READ | PROT_WRITE,
			     MAP_SHARED, fman_ccsr_map_fd, phys_addr);
	if (fman_ccsr_map == MAP_FAILED) {
		pr_err("Can not map FMan ccsr base");
		return -EINVAL;
	}

	ip_rev_1 = in_be32(fman_ccsr_map + FMAN_IP_REV_1);
	fman_ip_rev = (ip_rev_1 & FMAN_IP_REV_1_MAJOR_MASK) >>
			FMAN_IP_REV_1_MAJOR_SHIFT;

	_errno = munmap(fman_ccsr_map, regs_size);
	if (_errno)
		pr_err("munmap() of FMan ccsr failed");

	return 0;
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

static int
fman_if_init(const struct device_node *dpa_node)
{
	const char *rprop, *mprop;
	uint64_t phys_addr;
	struct __fman_if *__if;
	struct fman_if_bpool *bpool;

	const phandle *mac_phandle, *ports_phandle, *pools_phandle;
	const phandle *tx_channel_id = NULL, *mac_addr, *cell_idx;
	const phandle *rx_phandle, *tx_phandle;
	uint64_t tx_phandle_host[4] = {0};
	uint64_t rx_phandle_host[4] = {0};
	uint64_t regs_addr_host = 0;
	uint64_t cell_idx_host = 0;

	const struct device_node *mac_node = NULL, *tx_node;
	const struct device_node *pool_node, *fman_node, *rx_node;
	const uint32_t *regs_addr = NULL;
	const char *mname, *fname;
	const char *dname = dpa_node->full_name;
	size_t lenp;
	int _errno;
	const char *char_prop;
	uint32_t na;

	if (of_device_is_available(dpa_node) == false)
		return 0;

	rprop = "fsl,qman-frame-queues-rx";
	mprop = "fsl,fman-mac";

	/* Allocate an object for this network interface */
	__if = rte_malloc(NULL, sizeof(*__if), RTE_CACHE_LINE_SIZE);
	if (!__if) {
		FMAN_ERR(-ENOMEM, "malloc(%zu)\n", sizeof(*__if));
		goto err;
	}
	memset(__if, 0, sizeof(*__if));
	INIT_LIST_HEAD(&__if->__if.bpool_list);
	strlcpy(__if->node_path, dpa_node->full_name, PATH_MAX - 1);
	__if->node_path[PATH_MAX - 1] = '\0';

	/* Obtain the MAC node used by this interface except macless */
	mac_phandle = of_get_property(dpa_node, mprop, &lenp);
	if (!mac_phandle) {
		FMAN_ERR(-EINVAL, "%s: no %s\n", dname, mprop);
		goto err;
	}
	assert(lenp == sizeof(phandle));
	mac_node = of_find_node_by_phandle(*mac_phandle);
	if (!mac_node) {
		FMAN_ERR(-ENXIO, "%s: bad 'fsl,fman-mac\n", dname);
		goto err;
	}
	mname = mac_node->full_name;

	/* Map the CCSR regs for the MAC node */
	regs_addr = of_get_address(mac_node, 0, &__if->regs_size, NULL);
	if (!regs_addr) {
		FMAN_ERR(-EINVAL, "of_get_address(%s)\n", mname);
		goto err;
	}
	phys_addr = of_translate_address(mac_node, regs_addr);
	if (!phys_addr) {
		FMAN_ERR(-EINVAL, "of_translate_address(%s, %p)\n",
			 mname, regs_addr);
		goto err;
	}
	__if->ccsr_map = mmap(NULL, __if->regs_size,
			      PROT_READ | PROT_WRITE, MAP_SHARED,
			      fman_ccsr_map_fd, phys_addr);
	if (__if->ccsr_map == MAP_FAILED) {
		FMAN_ERR(-errno, "mmap(0x%"PRIx64")\n", phys_addr);
		goto err;
	}
	na = of_n_addr_cells(mac_node);
	/* Get rid of endianness (issues). Convert to host byte order */
	regs_addr_host = of_read_number(regs_addr, na);


	/* Get the index of the Fman this i/f belongs to */
	fman_node = of_get_parent(mac_node);
	na = of_n_addr_cells(mac_node);
	if (!fman_node) {
		FMAN_ERR(-ENXIO, "of_get_parent(%s)\n", mname);
		goto err;
	}
	fname = fman_node->full_name;
	cell_idx = of_get_property(fman_node, "cell-index", &lenp);
	if (!cell_idx) {
		FMAN_ERR(-ENXIO, "%s: no cell-index)\n", fname);
		goto err;
	}
	assert(lenp == sizeof(*cell_idx));
	cell_idx_host = of_read_number(cell_idx, lenp / sizeof(phandle));
	__if->__if.fman_idx = cell_idx_host;
	if (!get_once) {
		_errno = fman_get_ip_rev(fman_node);
		if (_errno) {
			FMAN_ERR(-ENXIO, "%s: ip_rev is not available\n",
				 fname);
			goto err;
		}
	}

	if (fman_ip_rev >= FMAN_V3) {
		/*
		 * Set A2V, OVOM, EBD bits in contextA to allow external
		 * buffer deallocation by fman.
		 */
		fman_dealloc_bufs_mask_hi = FMAN_V3_CONTEXTA_EN_A2V |
						FMAN_V3_CONTEXTA_EN_OVOM;
		fman_dealloc_bufs_mask_lo = FMAN_V3_CONTEXTA_EN_EBD;
	} else {
		fman_dealloc_bufs_mask_hi = 0;
		fman_dealloc_bufs_mask_lo = 0;
	}
	/* Is the MAC node 1G, 10G? */
	__if->__if.is_memac = 0;

	if (of_device_is_compatible(mac_node, "fsl,fman-1g-mac"))
		__if->__if.mac_type = fman_mac_1g;
	else if (of_device_is_compatible(mac_node, "fsl,fman-10g-mac"))
		__if->__if.mac_type = fman_mac_10g;
	else if (of_device_is_compatible(mac_node, "fsl,fman-memac")) {
		__if->__if.is_memac = 1;
		char_prop = of_get_property(mac_node, "phy-connection-type",
					    NULL);
		if (!char_prop) {
			printf("memac: unknown MII type assuming 1G\n");
			/* Right now forcing memac to 1g in case of error*/
			__if->__if.mac_type = fman_mac_1g;
		} else {
			if (strstr(char_prop, "sgmii"))
				__if->__if.mac_type = fman_mac_1g;
			else if (strstr(char_prop, "rgmii")) {
				__if->__if.mac_type = fman_mac_1g;
				__if->__if.is_rgmii = 1;
			} else if (strstr(char_prop, "xgmii"))
				__if->__if.mac_type = fman_mac_10g;
		}
	} else {
		FMAN_ERR(-EINVAL, "%s: unknown MAC type\n", mname);
		goto err;
	}

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

	/* Extract the MAC address for private and shared interfaces */
	mac_addr = of_get_property(mac_node, "local-mac-address",
				   &lenp);
	if (!mac_addr) {
		FMAN_ERR(-EINVAL, "%s: no local-mac-address\n",
			 mname);
		goto err;
	}
	memcpy(&__if->__if.mac_addr, mac_addr, ETHER_ADDR_LEN);

	/* Extract the Tx port (it's the second of the two port handles)
	 * and get its channel ID
	 */
	ports_phandle = of_get_property(mac_node, "fsl,port-handles",
					&lenp);
	if (!ports_phandle)
		ports_phandle = of_get_property(mac_node, "fsl,fman-ports",
						&lenp);
	if (!ports_phandle) {
		FMAN_ERR(-EINVAL, "%s: no fsl,port-handles\n",
			 mname);
		goto err;
	}
	assert(lenp == (2 * sizeof(phandle)));
	tx_node = of_find_node_by_phandle(ports_phandle[1]);
	if (!tx_node) {
		FMAN_ERR(-ENXIO, "%s: bad fsl,port-handle[1]\n", mname);
		goto err;
	}
	/* Extract the channel ID (from tx-port-handle) */
	tx_channel_id = of_get_property(tx_node, "fsl,qman-channel-id",
					&lenp);
	if (!tx_channel_id) {
		FMAN_ERR(-EINVAL, "%s: no fsl-qman-channel-id\n",
			 tx_node->full_name);
		goto err;
	}

	rx_node = of_find_node_by_phandle(ports_phandle[0]);
	if (!rx_node) {
		FMAN_ERR(-ENXIO, "%s: bad fsl,port-handle[0]\n", mname);
		goto err;
	}
	regs_addr = of_get_address(rx_node, 0, &__if->regs_size, NULL);
	if (!regs_addr) {
		FMAN_ERR(-EINVAL, "of_get_address(%s)\n", mname);
		goto err;
	}
	phys_addr = of_translate_address(rx_node, regs_addr);
	if (!phys_addr) {
		FMAN_ERR(-EINVAL, "of_translate_address(%s, %p)\n",
			 mname, regs_addr);
		goto err;
	}
	__if->bmi_map = mmap(NULL, __if->regs_size,
				 PROT_READ | PROT_WRITE, MAP_SHARED,
				 fman_ccsr_map_fd, phys_addr);
	if (__if->bmi_map == MAP_FAILED) {
		FMAN_ERR(-errno, "mmap(0x%"PRIx64")\n", phys_addr);
		goto err;
	}

	/* No channel ID for MAC-less */
	assert(lenp == sizeof(*tx_channel_id));
	na = of_n_addr_cells(mac_node);
	__if->__if.tx_channel_id = of_read_number(tx_channel_id, na);

	/* Extract the Rx FQIDs. (Note, the device representation is silly,
	 * there are "counts" that must always be 1.)
	 */
	rx_phandle = of_get_property(dpa_node, rprop, &lenp);
	if (!rx_phandle) {
		FMAN_ERR(-EINVAL, "%s: no fsl,qman-frame-queues-rx\n", dname);
		goto err;
	}

	assert(lenp == (4 * sizeof(phandle)));

	na = of_n_addr_cells(mac_node);
	/* Get rid of endianness (issues). Convert to host byte order */
	rx_phandle_host[0] = of_read_number(&rx_phandle[0], na);
	rx_phandle_host[1] = of_read_number(&rx_phandle[1], na);
	rx_phandle_host[2] = of_read_number(&rx_phandle[2], na);
	rx_phandle_host[3] = of_read_number(&rx_phandle[3], na);

	assert((rx_phandle_host[1] == 1) && (rx_phandle_host[3] == 1));
	__if->__if.fqid_rx_err = rx_phandle_host[0];
	__if->__if.fqid_rx_def = rx_phandle_host[2];

	/* Extract the Tx FQIDs */
	tx_phandle = of_get_property(dpa_node,
				     "fsl,qman-frame-queues-tx", &lenp);
	if (!tx_phandle) {
		FMAN_ERR(-EINVAL, "%s: no fsl,qman-frame-queues-tx\n", dname);
		goto err;
	}

	assert(lenp == (4 * sizeof(phandle)));
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
		FMAN_ERR(-EINVAL, "%s: no fsl,bman-buffer-pools\n", dname);
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
			FMAN_ERR(-ENOMEM, "malloc(%zu)\n", sizeof(*bpool));
			goto err;
		}
		/* Find the pool node */
		pool_node = of_find_node_by_phandle(*pools_phandle);
		if (!pool_node) {
			FMAN_ERR(-ENXIO, "%s: bad fsl,bman-buffer-pools\n",
				 dname);
			rte_free(bpool);
			goto err;
		}
		pname = pool_node->full_name;
		/* Extract the BPID property */
		prop = of_get_property(pool_node, "fsl,bpid", &proplen);
		if (!prop) {
			FMAN_ERR(-EINVAL, "%s: no fsl,bpid\n", pname);
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

	/* Parsing of the network interface is complete, add it to the list */
	DPAA_BUS_LOG(DEBUG, "Found %s, Tx Channel = %x, FMAN = %x,"
		    "Port ID = %x",
		    dname, __if->__if.tx_channel_id, __if->__if.fman_idx,
		    __if->__if.mac_idx);

	list_add_tail(&__if->__if.node, &__ifs);
	return 0;
err:
	if_destructor(__if);
	return _errno;
}

int
fman_init(void)
{
	const struct device_node *dpa_node;
	int _errno;

	/* If multiple dependencies try to initialise the Fman driver, don't
	 * panic.
	 */
	if (fman_ccsr_map_fd != -1)
		return 0;

	fman_ccsr_map_fd = open(FMAN_DEVICE_PATH, O_RDWR);
	if (unlikely(fman_ccsr_map_fd < 0)) {
		DPAA_BUS_LOG(ERR, "Unable to open (/dev/mem)");
		return fman_ccsr_map_fd;
	}

	for_each_compatible_node(dpa_node, NULL, "fsl,dpa-ethernet-init") {
		_errno = fman_if_init(dpa_node);
		if (_errno) {
			FMAN_ERR(_errno, "if_init(%s)\n", dpa_node->full_name);
			goto err;
		}
	}

	return 0;
err:
	fman_finish();
	return _errno;
}

void
fman_finish(void)
{
	struct __fman_if *__if, *tmpif;

	assert(fman_ccsr_map_fd != -1);

	list_for_each_entry_safe(__if, tmpif, &__ifs, __if.node) {
		int _errno;

		/* disable Rx and Tx */
		if ((__if->__if.mac_type == fman_mac_1g) &&
		    (!__if->__if.is_memac))
			out_be32(__if->ccsr_map + 0x100,
				 in_be32(__if->ccsr_map + 0x100) & ~(u32)0x5);
		else
			out_be32(__if->ccsr_map + 8,
				 in_be32(__if->ccsr_map + 8) & ~(u32)3);
		/* release the mapping */
		_errno = munmap(__if->ccsr_map, __if->regs_size);
		if (unlikely(_errno < 0))
			fprintf(stderr, "%s:%d:%s(): munmap() = %d (%s)\n",
				__FILE__, __LINE__, __func__,
				-errno, strerror(errno));
		printf("Tearing down %s\n", __if->node_path);
		list_del(&__if->__if.node);
		rte_free(__if);
	}

	close(fman_ccsr_map_fd);
	fman_ccsr_map_fd = -1;
}
