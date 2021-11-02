/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <cnxk_ethdev.h>

#define CNXK_NIX_INL_SELFTEST	      "selftest"
#define CNXK_NIX_INL_IPSEC_IN_MAX_SPI "ipsec_in_max_spi"
#define CNXK_INL_CPT_CHANNEL	      "inl_cpt_channel"

struct inl_cpt_channel {
	bool is_multi_channel;
	uint16_t channel;
	uint16_t mask;
};

#define CNXK_NIX_INL_DEV_NAME RTE_STR(cnxk_nix_inl_dev_)
#define CNXK_NIX_INL_DEV_NAME_LEN                                              \
	(sizeof(CNXK_NIX_INL_DEV_NAME) + PCI_PRI_STR_SIZE)

static inline int
bitmap_ctzll(uint64_t slab)
{
	if (slab == 0)
		return 0;

	return __builtin_ctzll(slab);
}

int
cnxk_eth_outb_sa_idx_get(struct cnxk_eth_dev *dev, uint32_t *idx_p)
{
	uint32_t pos, idx;
	uint64_t slab;
	int rc;

	if (!dev->outb.sa_bmap)
		return -ENOTSUP;

	pos = 0;
	slab = 0;
	/* Scan from the beginning */
	plt_bitmap_scan_init(dev->outb.sa_bmap);
	/* Scan bitmap to get the free sa index */
	rc = plt_bitmap_scan(dev->outb.sa_bmap, &pos, &slab);
	/* Empty bitmap */
	if (rc == 0) {
		plt_err("Outbound SA' exhausted, use 'ipsec_out_max_sa' "
			"devargs to increase");
		return -ERANGE;
	}

	/* Get free SA index */
	idx = pos + bitmap_ctzll(slab);
	plt_bitmap_clear(dev->outb.sa_bmap, idx);
	*idx_p = idx;
	return 0;
}

int
cnxk_eth_outb_sa_idx_put(struct cnxk_eth_dev *dev, uint32_t idx)
{
	if (idx >= dev->outb.max_sa)
		return -EINVAL;

	/* Check if it is already free */
	if (plt_bitmap_get(dev->outb.sa_bmap, idx))
		return -EINVAL;

	/* Mark index as free */
	plt_bitmap_set(dev->outb.sa_bmap, idx);
	return 0;
}

struct cnxk_eth_sec_sess *
cnxk_eth_sec_sess_get_by_spi(struct cnxk_eth_dev *dev, uint32_t spi, bool inb)
{
	struct cnxk_eth_sec_sess_list *list;
	struct cnxk_eth_sec_sess *eth_sec;

	list = inb ? &dev->inb.list : &dev->outb.list;
	TAILQ_FOREACH(eth_sec, list, entry) {
		if (eth_sec->spi == spi)
			return eth_sec;
	}

	return NULL;
}

struct cnxk_eth_sec_sess *
cnxk_eth_sec_sess_get_by_sess(struct cnxk_eth_dev *dev,
			      struct rte_security_session *sess)
{
	struct cnxk_eth_sec_sess *eth_sec = NULL;

	/* Search in inbound list */
	TAILQ_FOREACH(eth_sec, &dev->inb.list, entry) {
		if (eth_sec->sess == sess)
			return eth_sec;
	}

	/* Search in outbound list */
	TAILQ_FOREACH(eth_sec, &dev->outb.list, entry) {
		if (eth_sec->sess == sess)
			return eth_sec;
	}

	return NULL;
}

static unsigned int
cnxk_eth_sec_session_get_size(void *device __rte_unused)
{
	return sizeof(struct cnxk_eth_sec_sess);
}

struct rte_security_ops cnxk_eth_sec_ops = {
	.session_get_size = cnxk_eth_sec_session_get_size
};

static int
parse_ipsec_in_max_spi(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_selftest(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	*(uint8_t *)extra_args = !!(val == 1);
	return 0;
}

static int
parse_inl_cpt_channel(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint16_t chan = 0, mask = 0;
	char *next = 0;

	/* next will point to the separator '/' */
	chan = strtol(value, &next, 16);
	mask = strtol(++next, 0, 16);

	if (chan > GENMASK(12, 0) || mask > GENMASK(12, 0))
		return -EINVAL;

	((struct inl_cpt_channel *)extra_args)->channel = chan;
	((struct inl_cpt_channel *)extra_args)->mask = mask;
	((struct inl_cpt_channel *)extra_args)->is_multi_channel = true;

	return 0;
}

static int
nix_inl_parse_devargs(struct rte_devargs *devargs,
		      struct roc_nix_inl_dev *inl_dev)
{
	uint32_t ipsec_in_max_spi = BIT(8) - 1;
	struct inl_cpt_channel cpt_channel;
	struct rte_kvargs *kvlist;
	uint8_t selftest = 0;

	memset(&cpt_channel, 0, sizeof(cpt_channel));

	if (devargs == NULL)
		goto null_devargs;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		goto exit;

	rte_kvargs_process(kvlist, CNXK_NIX_INL_SELFTEST, &parse_selftest,
			   &selftest);
	rte_kvargs_process(kvlist, CNXK_NIX_INL_IPSEC_IN_MAX_SPI,
			   &parse_ipsec_in_max_spi, &ipsec_in_max_spi);
	rte_kvargs_process(kvlist, CNXK_INL_CPT_CHANNEL, &parse_inl_cpt_channel,
			   &cpt_channel);
	rte_kvargs_free(kvlist);

null_devargs:
	inl_dev->ipsec_in_max_spi = ipsec_in_max_spi;
	inl_dev->selftest = selftest;
	inl_dev->channel = cpt_channel.channel;
	inl_dev->chan_mask = cpt_channel.mask;
	inl_dev->is_multi_channel = cpt_channel.is_multi_channel;
	return 0;
exit:
	return -EINVAL;
}

static inline char *
nix_inl_dev_to_name(struct rte_pci_device *pci_dev, char *name)
{
	snprintf(name, CNXK_NIX_INL_DEV_NAME_LEN,
		 CNXK_NIX_INL_DEV_NAME PCI_PRI_FMT, pci_dev->addr.domain,
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	return name;
}

static int
cnxk_nix_inl_dev_remove(struct rte_pci_device *pci_dev)
{
	char name[CNXK_NIX_INL_DEV_NAME_LEN];
	const struct rte_memzone *mz;
	struct roc_nix_inl_dev *dev;
	int rc;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	mz = rte_memzone_lookup(nix_inl_dev_to_name(pci_dev, name));
	if (!mz)
		return 0;

	dev = mz->addr;

	/* Cleanup inline dev */
	rc = roc_nix_inl_dev_fini(dev);
	if (rc) {
		plt_err("Failed to cleanup inl dev, rc=%d(%s)", rc,
			roc_error_msg_get(rc));
		return rc;
	}

	rte_memzone_free(mz);
	return 0;
}

static int
cnxk_nix_inl_dev_probe(struct rte_pci_driver *pci_drv,
		       struct rte_pci_device *pci_dev)
{
	char name[CNXK_NIX_INL_DEV_NAME_LEN];
	struct roc_nix_inl_dev *inl_dev;
	const struct rte_memzone *mz;
	int rc = -ENOMEM;

	RTE_SET_USED(pci_drv);

	rc = roc_plt_init();
	if (rc) {
		plt_err("Failed to initialize platform model, rc=%d", rc);
		return rc;
	}

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	mz = rte_memzone_reserve_aligned(nix_inl_dev_to_name(pci_dev, name),
					 sizeof(*inl_dev), SOCKET_ID_ANY, 0,
					 RTE_CACHE_LINE_SIZE);
	if (mz == NULL)
		return rc;

	inl_dev = mz->addr;
	inl_dev->pci_dev = pci_dev;

	/* Parse devargs string */
	rc = nix_inl_parse_devargs(pci_dev->device.devargs, inl_dev);
	if (rc) {
		plt_err("Failed to parse devargs rc=%d", rc);
		goto free_mem;
	}

	inl_dev->attach_cptlf = true;
	rc = roc_nix_inl_dev_init(inl_dev);
	if (rc) {
		plt_err("Failed to init nix inl device, rc=%d(%s)", rc,
			roc_error_msg_get(rc));
		goto free_mem;
	}

	return 0;
free_mem:
	rte_memzone_free(mz);
	return rc;
}

static const struct rte_pci_id cnxk_nix_inl_pci_map[] = {
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CNXK_RVU_NIX_INL_PF)},
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CNXK_RVU_NIX_INL_VF)},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cnxk_nix_inl_pci = {
	.id_table = cnxk_nix_inl_pci_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = cnxk_nix_inl_dev_probe,
	.remove = cnxk_nix_inl_dev_remove,
};

RTE_PMD_REGISTER_PCI(cnxk_nix_inl, cnxk_nix_inl_pci);
RTE_PMD_REGISTER_PCI_TABLE(cnxk_nix_inl, cnxk_nix_inl_pci_map);
RTE_PMD_REGISTER_KMOD_DEP(cnxk_nix_inl, "vfio-pci");

RTE_PMD_REGISTER_PARAM_STRING(cnxk_nix_inl,
			      CNXK_NIX_INL_SELFTEST "=1"
			      CNXK_NIX_INL_IPSEC_IN_MAX_SPI "=<1-65535>"
			      CNXK_INL_CPT_CHANNEL "=<1-4095>/<1-4095>");
