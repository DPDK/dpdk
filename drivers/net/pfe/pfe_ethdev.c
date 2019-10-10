/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 NXP
 */

#include <rte_kvargs.h>
#include <rte_ethdev_vdev.h>
#include <rte_bus_vdev.h>
#include <dpaa_of.h>

#include "pfe_logs.h"
#include "pfe_mod.h"

#define PFE_MAX_MACS 1 /*we can support upto 4 MACs per IF*/
#define PFE_VDEV_GEM_ID_ARG	"intf"

struct pfe_vdev_init_params {
	int8_t	gem_id;
};
static struct pfe *g_pfe;

/* TODO: make pfe_svr a runtime option.
 * Driver should be able to get the SVR
 * information from HW.
 */
unsigned int pfe_svr = SVR_LS1012A_REV1;
static void *cbus_emac_base[3];
static void *cbus_gpi_base[3];

int pfe_logtype_pmd;

/* pfe_gemac_init
 */
static int
pfe_gemac_init(struct pfe_eth_priv_s *priv)
{
	struct gemac_cfg cfg;

	cfg.speed = SPEED_1000M;
	cfg.duplex = DUPLEX_FULL;

	gemac_set_config(priv->EMAC_baseaddr, &cfg);
	gemac_allow_broadcast(priv->EMAC_baseaddr);
	gemac_enable_1536_rx(priv->EMAC_baseaddr);
	gemac_enable_stacked_vlan(priv->EMAC_baseaddr);
	gemac_enable_pause_rx(priv->EMAC_baseaddr);
	gemac_set_bus_width(priv->EMAC_baseaddr, 64);
	gemac_enable_rx_checksum_offload(priv->EMAC_baseaddr);

	return 0;
}

static void
pfe_soc_version_get(void)
{
	FILE *svr_file = NULL;
	unsigned int svr_ver = 0;

	PMD_INIT_FUNC_TRACE();

	svr_file = fopen(PFE_SOC_ID_FILE, "r");
	if (!svr_file) {
		PFE_PMD_ERR("Unable to open SoC device");
		return; /* Not supported on this infra */
	}

	if (fscanf(svr_file, "svr:%x", &svr_ver) > 0)
		pfe_svr = svr_ver;
	else
		PFE_PMD_ERR("Unable to read SoC device");

	fclose(svr_file);
}

static int
pfe_eth_open_cdev(struct pfe_eth_priv_s *priv)
{
	int pfe_cdev_fd;

	if (priv == NULL)
		return -1;

	pfe_cdev_fd = open(PFE_CDEV_PATH, O_RDONLY);
	if (pfe_cdev_fd < 0) {
		PFE_PMD_WARN("Unable to open PFE device file (%s).\n",
			     PFE_CDEV_PATH);
		PFE_PMD_WARN("Link status update will not be available.\n");
		priv->link_fd = PFE_CDEV_INVALID_FD;
		return -1;
	}

	priv->link_fd = pfe_cdev_fd;

	return 0;
}

static void
pfe_eth_close_cdev(struct pfe_eth_priv_s *priv)
{
	if (priv == NULL)
		return;

	if (priv->link_fd != PFE_CDEV_INVALID_FD) {
		close(priv->link_fd);
		priv->link_fd = PFE_CDEV_INVALID_FD;
	}
}

static void
pfe_eth_exit(struct rte_eth_dev *dev, struct pfe *pfe)
{
	PMD_INIT_FUNC_TRACE();

	/* Close the device file for link status */
	pfe_eth_close_cdev(dev->data->dev_private);

	rte_free(dev->data->mac_addrs);
	rte_eth_dev_release_port(dev);
	pfe->nb_devs--;
}

static int
pfe_eth_init(struct rte_vdev_device *vdev, struct pfe *pfe, int id)
{
	struct rte_eth_dev *eth_dev = NULL;
	struct pfe_eth_priv_s *priv = NULL;
	struct ls1012a_eth_platform_data *einfo;
	struct ls1012a_pfe_platform_data *pfe_info;
	int err;

	eth_dev = rte_eth_vdev_allocate(vdev, sizeof(*priv));
	if (eth_dev == NULL)
		return -ENOMEM;

	/* Extract pltform data */
	pfe_info = (struct ls1012a_pfe_platform_data *)&pfe->platform_data;
	if (!pfe_info) {
		PFE_PMD_ERR("pfe missing additional platform data");
		err = -ENODEV;
		goto err0;
	}

	einfo = (struct ls1012a_eth_platform_data *)pfe_info->ls1012a_eth_pdata;

	/* einfo never be NULL, but no harm in having this check */
	if (!einfo) {
		PFE_PMD_ERR("pfe missing additional gemacs platform data");
		err = -ENODEV;
		goto err0;
	}

	priv = eth_dev->data->dev_private;
	priv->ndev = eth_dev;
	priv->id = einfo[id].gem_id;
	priv->pfe = pfe;

	pfe->eth.eth_priv[id] = priv;

	/* Set the info in the priv to the current info */
	priv->einfo = &einfo[id];
	priv->EMAC_baseaddr = cbus_emac_base[id];
	priv->PHY_baseaddr = cbus_emac_base[id];
	priv->GPI_baseaddr = cbus_gpi_base[id];

#define HIF_GEMAC_TMUQ_BASE	6
	priv->low_tmu_q = HIF_GEMAC_TMUQ_BASE + (id * 2);
	priv->high_tmu_q = priv->low_tmu_q + 1;

	rte_spinlock_init(&priv->lock);

	/* Copy the station address into the dev structure, */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr",
			ETHER_ADDR_LEN * PFE_MAX_MACS, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PFE_PMD_ERR("Failed to allocate mem %d to store MAC addresses",
			ETHER_ADDR_LEN * PFE_MAX_MACS);
		err = -ENOMEM;
		goto err0;
	}

	eth_dev->data->mtu = 1500;
	pfe_gemac_init(priv);

	eth_dev->data->nb_rx_queues = 1;
	eth_dev->data->nb_tx_queues = 1;

	/* For link status, open the PFE CDEV; Error from this function
	 * is silently ignored; In case of error, the link status will not
	 * be available.
	 */
	pfe_eth_open_cdev(priv);
	rte_eth_dev_probing_finish(eth_dev);

	return 0;
err0:
	rte_eth_dev_release_port(eth_dev);
	return err;
}

static int
pfe_get_gemac_if_proprties(struct pfe *pfe,
		__rte_unused const struct device_node *parent,
		unsigned int port, unsigned int if_cnt,
		struct ls1012a_pfe_platform_data *pdata)
{
	const struct device_node *gem = NULL;
	size_t size;
	unsigned int ii = 0, phy_id = 0;
	const u32 *addr;
	const void *mac_addr;

	for (ii = 0; ii < if_cnt; ii++) {
		gem = of_get_next_child(parent, gem);
		if (!gem)
			goto err;
		addr = of_get_property(gem, "reg", &size);
		if (addr && (rte_be_to_cpu_32((unsigned int)*addr) == port))
			break;
	}

	if (ii >= if_cnt) {
		PFE_PMD_ERR("Failed to find interface = %d", if_cnt);
		goto err;
	}

	pdata->ls1012a_eth_pdata[port].gem_id = port;

	mac_addr = of_get_mac_address(gem);

	if (mac_addr) {
		memcpy(pdata->ls1012a_eth_pdata[port].mac_addr, mac_addr,
		       ETH_ALEN);
	}

	addr = of_get_property(gem, "fsl,mdio-mux-val", &size);
	if (!addr) {
		PFE_PMD_ERR("Invalid mdio-mux-val....");
	} else {
		phy_id = rte_be_to_cpu_32((unsigned int)*addr);
		pdata->ls1012a_eth_pdata[port].mdio_muxval = phy_id;
	}
	if (pdata->ls1012a_eth_pdata[port].phy_id < 32)
		pfe->mdio_muxval[pdata->ls1012a_eth_pdata[port].phy_id] =
			 pdata->ls1012a_eth_pdata[port].mdio_muxval;

	return 0;

err:
	return -1;
}

/* Parse integer from integer argument */
static int
parse_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int i;
	char *end;
	errno = 0;

	i = strtol(value, &end, 10);
	if (*end != 0 || errno != 0 || i < 0 || i > 1) {
		PFE_PMD_ERR("Supported Port IDS are 0 and 1");
		return -EINVAL;
	}

	*((uint32_t *)extra_args) = i;

	return 0;
}

static int
pfe_parse_vdev_init_params(struct pfe_vdev_init_params *params,
			   struct rte_vdev_device *dev)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	static const char * const pfe_vdev_valid_params[] = {
		PFE_VDEV_GEM_ID_ARG,
		NULL
	};

	const char *input_args = rte_vdev_device_args(dev);

	if (!input_args)
		return -1;

	kvlist = rte_kvargs_parse(input_args, pfe_vdev_valid_params);
	if (kvlist == NULL)
		return -1;

	ret = rte_kvargs_process(kvlist,
				PFE_VDEV_GEM_ID_ARG,
				&parse_integer_arg,
				&params->gem_id);
	rte_kvargs_free(kvlist);
	return ret;
}

static int
pmd_pfe_probe(struct rte_vdev_device *vdev)
{
	const u32 *prop;
	const struct device_node *np;
	const char *name;
	const uint32_t *addr;
	uint64_t cbus_addr, ddr_size, cbus_size;
	int rc = -1, fd = -1, gem_id;
	unsigned int ii, interface_count = 0;
	size_t size = 0;
	struct pfe_vdev_init_params init_params = {
		.gem_id = -1
	};

	name = rte_vdev_device_name(vdev);
	rc = pfe_parse_vdev_init_params(&init_params, vdev);
	if (rc < 0)
		return -EINVAL;

	RTE_LOG(INFO, PMD, "Initializing pmd_pfe for %s Given gem-id %d\n",
		name, init_params.gem_id);

	if (g_pfe) {
		if (g_pfe->nb_devs >= g_pfe->max_intf) {
			PFE_PMD_ERR("PFE %d dev already created Max is %d",
				g_pfe->nb_devs, g_pfe->max_intf);
			return -EINVAL;
		}
		goto eth_init;
	}

	g_pfe = rte_zmalloc(NULL, sizeof(*g_pfe), RTE_CACHE_LINE_SIZE);
	if (g_pfe == NULL)
		return  -EINVAL;

	/* Load the device-tree driver */
	rc = of_init();
	if (rc) {
		PFE_PMD_ERR("of_init failed with ret: %d", rc);
		goto err;
	}

	np = of_find_compatible_node(NULL, NULL, "fsl,pfe");
	if (!np) {
		PFE_PMD_ERR("Invalid device node");
		rc = -EINVAL;
		goto err;
	}

	addr = of_get_address(np, 0, &cbus_size, NULL);
	if (!addr) {
		PFE_PMD_ERR("of_get_address cannot return qman address\n");
		goto err;
	}
	cbus_addr = of_translate_address(np, addr);
	if (!cbus_addr) {
		PFE_PMD_ERR("of_translate_address failed\n");
		goto err;
	}

	addr = of_get_address(np, 1, &ddr_size, NULL);
	if (!addr) {
		PFE_PMD_ERR("of_get_address cannot return qman address\n");
		goto err;
	}

	g_pfe->ddr_phys_baseaddr = of_translate_address(np, addr);
	if (!g_pfe->ddr_phys_baseaddr) {
		PFE_PMD_ERR("of_translate_address failed\n");
		goto err;
	}

	g_pfe->ddr_baseaddr = pfe_mem_ptov(g_pfe->ddr_phys_baseaddr);
	g_pfe->ddr_size = ddr_size;
	g_pfe->cbus_size = cbus_size;

	fd = open("/dev/mem", O_RDWR);
	g_pfe->cbus_baseaddr = mmap(NULL, cbus_size, PROT_READ | PROT_WRITE,
					MAP_SHARED, fd, cbus_addr);
	close(fd);
	if (g_pfe->cbus_baseaddr == MAP_FAILED) {
		PFE_PMD_ERR("Can not map cbus base");
		rc = -EINVAL;
		goto err;
	}

	/* Read interface count */
	prop = of_get_property(np, "fsl,pfe-num-interfaces", &size);
	if (!prop) {
		PFE_PMD_ERR("Failed to read number of interfaces");
		rc = -ENXIO;
		goto err_prop;
	}

	interface_count = rte_be_to_cpu_32((unsigned int)*prop);
	if (interface_count <= 0) {
		PFE_PMD_ERR("No ethernet interface count : %d",
				interface_count);
		rc = -ENXIO;
		goto err_prop;
	}
	PFE_PMD_INFO("num interfaces = %d ", interface_count);

	g_pfe->max_intf  = interface_count;
	g_pfe->platform_data.ls1012a_mdio_pdata[0].phy_mask = 0xffffffff;

	for (ii = 0; ii < interface_count; ii++) {
		pfe_get_gemac_if_proprties(g_pfe, np, ii, interface_count,
					   &g_pfe->platform_data);
	}

	pfe_lib_init(g_pfe->cbus_baseaddr, g_pfe->ddr_baseaddr,
		     g_pfe->ddr_phys_baseaddr, g_pfe->ddr_size);

	PFE_PMD_INFO("CLASS version: %x", readl(CLASS_VERSION));
	PFE_PMD_INFO("TMU version: %x", readl(TMU_VERSION));

	PFE_PMD_INFO("BMU1 version: %x", readl(BMU1_BASE_ADDR + BMU_VERSION));
	PFE_PMD_INFO("BMU2 version: %x", readl(BMU2_BASE_ADDR + BMU_VERSION));

	PFE_PMD_INFO("EGPI1 version: %x", readl(EGPI1_BASE_ADDR + GPI_VERSION));
	PFE_PMD_INFO("EGPI2 version: %x", readl(EGPI2_BASE_ADDR + GPI_VERSION));
	PFE_PMD_INFO("HGPI version: %x", readl(HGPI_BASE_ADDR + GPI_VERSION));

	PFE_PMD_INFO("HIF version: %x", readl(HIF_VERSION));
	PFE_PMD_INFO("HIF NOPCY version: %x", readl(HIF_NOCPY_VERSION));

	cbus_emac_base[0] = EMAC1_BASE_ADDR;
	cbus_emac_base[1] = EMAC2_BASE_ADDR;

	cbus_gpi_base[0] = EGPI1_BASE_ADDR;
	cbus_gpi_base[1] = EGPI2_BASE_ADDR;

	rc = pfe_hif_lib_init(g_pfe);
	if (rc < 0)
		goto err_hif_lib;

	rc = pfe_hif_init(g_pfe);
	if (rc < 0)
		goto err_hif;
	pfe_soc_version_get();
eth_init:
	if (init_params.gem_id < 0)
		gem_id = g_pfe->nb_devs;
	else
		gem_id = init_params.gem_id;

	RTE_LOG(INFO, PMD, "Init pmd_pfe for %s gem-id %d(given =%d)\n",
		name, gem_id, init_params.gem_id);

	rc = pfe_eth_init(vdev, g_pfe, gem_id);
	if (rc < 0)
		goto err_eth;
	else
		g_pfe->nb_devs++;

	return 0;

err_eth:
	pfe_hif_exit(g_pfe);

err_hif:
	pfe_hif_lib_exit(g_pfe);

err_hif_lib:
err_prop:
	munmap(g_pfe->cbus_baseaddr, cbus_size);
err:
	rte_free(g_pfe);
	return rc;
}

static int
pmd_pfe_remove(struct rte_vdev_device *vdev)
{
	const char *name;
	struct rte_eth_dev *eth_dev = NULL;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	PFE_PMD_INFO("Closing eventdev sw device %s", name);

	if (!g_pfe)
		return 0;

	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev == NULL)
		return -ENODEV;

	pfe_eth_exit(eth_dev, g_pfe);
	munmap(g_pfe->cbus_baseaddr, g_pfe->cbus_size);

	if (g_pfe->nb_devs == 0) {
		pfe_hif_exit(g_pfe);
		pfe_hif_lib_exit(g_pfe);
		rte_free(g_pfe);
		g_pfe = NULL;
	}
	return 0;
}

static
struct rte_vdev_driver pmd_pfe_drv = {
	.probe = pmd_pfe_probe,
	.remove = pmd_pfe_remove,
};

RTE_PMD_REGISTER_VDEV(PFE_NAME_PMD, pmd_pfe_drv);
RTE_PMD_REGISTER_PARAM_STRING(PFE_NAME_PMD, PFE_VDEV_GEM_ID_ARG "=<int> ");

RTE_INIT(pfe_pmd_init_log)
{
	pfe_logtype_pmd = rte_log_register("pmd.net.pfe");
	if (pfe_logtype_pmd >= 0)
		rte_log_set_level(pfe_logtype_pmd, RTE_LOG_NOTICE);
}
