/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */

#include <ethdev_vdev.h>
#include <ethdev_driver.h>
#include <rte_io.h>
#include "enet_pmd_logs.h"
#include "enet_ethdev.h"
#include "enet_regs.h"
#include "enet_uio.h"

#define ENETFEC_NAME_PMD                net_enetfec

/* FEC receive acceleration */
#define ENETFEC_RACC_IPDIS		RTE_BIT32(1)
#define ENETFEC_RACC_PRODIS		RTE_BIT32(2)
#define ENETFEC_RACC_SHIFT16		RTE_BIT32(7)
#define ENETFEC_RACC_OPTIONS		(ENETFEC_RACC_IPDIS | \
						ENETFEC_RACC_PRODIS)

#define ENETFEC_PAUSE_FLAG_AUTONEG	0x1
#define ENETFEC_PAUSE_FLAG_ENABLE	0x2

/* Pause frame field and FIFO threshold */
#define ENETFEC_FCE			RTE_BIT32(5)
#define ENETFEC_RSEM_V			0x84
#define ENETFEC_RSFL_V			16
#define ENETFEC_RAEM_V			0x8
#define ENETFEC_RAFL_V			0x8
#define ENETFEC_OPD_V			0xFFF0

#define NUM_OF_BD_QUEUES		6

/*
 * This function is called to start or restart the ENETFEC during a link
 * change, transmit timeout, or to reconfigure the ENETFEC. The network
 * packet processing for this device must be stopped before this call.
 */
static void
enetfec_restart(struct rte_eth_dev *dev)
{
	struct enetfec_private *fep = dev->data->dev_private;
	uint32_t rcntl = OPT_FRAME_SIZE | 0x04;
	uint32_t ecntl = ENETFEC_ETHEREN;
	uint32_t val;

	/* Clear any outstanding interrupt. */
	writel(0xffffffff, (uint8_t *)fep->hw_baseaddr_v + ENETFEC_EIR);

	/* Enable MII mode */
	if (fep->full_duplex == FULL_DUPLEX) {
		/* FD enable */
		rte_write32(rte_cpu_to_le_32(0x04),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_TCR);
	} else {
	/* No Rcv on Xmit */
		rcntl |= 0x02;
		rte_write32(0, (uint8_t *)fep->hw_baseaddr_v + ENETFEC_TCR);
	}

	if (fep->quirks & QUIRK_RACC) {
		val = rte_read32((uint8_t *)fep->hw_baseaddr_v + ENETFEC_RACC);
		/* align IP header */
		val |= ENETFEC_RACC_SHIFT16;
		val &= ~ENETFEC_RACC_OPTIONS;
		rte_write32(rte_cpu_to_le_32(val),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_RACC);
		rte_write32(rte_cpu_to_le_32(PKT_MAX_BUF_SIZE),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_FRAME_TRL);
	}

	/*
	 * The phy interface and speed need to get configured
	 * differently on enet-mac.
	 */
	if (fep->quirks & QUIRK_HAS_ENETFEC_MAC) {
		/* Enable flow control and length check */
		rcntl |= 0x40000000 | 0x00000020;

		/* RGMII, RMII or MII */
		rcntl |= RTE_BIT32(6);
		ecntl |= RTE_BIT32(5);
	}

	/* enable pause frame*/
	if ((fep->flag_pause & ENETFEC_PAUSE_FLAG_ENABLE) ||
		((fep->flag_pause & ENETFEC_PAUSE_FLAG_AUTONEG)
		/*&& ndev->phydev && ndev->phydev->pause*/)) {
		rcntl |= ENETFEC_FCE;

		/* set FIFO threshold parameter to reduce overrun */
		rte_write32(rte_cpu_to_le_32(ENETFEC_RSEM_V),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_R_FIFO_SEM);
		rte_write32(rte_cpu_to_le_32(ENETFEC_RSFL_V),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_R_FIFO_SFL);
		rte_write32(rte_cpu_to_le_32(ENETFEC_RAEM_V),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_R_FIFO_AEM);
		rte_write32(rte_cpu_to_le_32(ENETFEC_RAFL_V),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_R_FIFO_AFL);

		/* OPD */
		rte_write32(rte_cpu_to_le_32(ENETFEC_OPD_V),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_OPD);
	} else {
		rcntl &= ~ENETFEC_FCE;
	}

	rte_write32(rte_cpu_to_le_32(rcntl),
		(uint8_t *)fep->hw_baseaddr_v + ENETFEC_RCR);

	rte_write32(0, (uint8_t *)fep->hw_baseaddr_v + ENETFEC_IAUR);
	rte_write32(0, (uint8_t *)fep->hw_baseaddr_v + ENETFEC_IALR);

	if (fep->quirks & QUIRK_HAS_ENETFEC_MAC) {
		/* enable ENETFEC endian swap */
		ecntl |= (1 << 8);
		/* enable ENETFEC store and forward mode */
		rte_write32(rte_cpu_to_le_32(1 << 8),
			(uint8_t *)fep->hw_baseaddr_v + ENETFEC_TFWR);
	}
	if (fep->bufdesc_ex)
		ecntl |= (1 << 4);
	if (fep->quirks & QUIRK_SUPPORT_DELAYED_CLKS &&
		fep->rgmii_txc_delay)
		ecntl |= ENETFEC_TXC_DLY;
	if (fep->quirks & QUIRK_SUPPORT_DELAYED_CLKS &&
		fep->rgmii_rxc_delay)
		ecntl |= ENETFEC_RXC_DLY;
	/* Enable the MIB statistic event counters */
	rte_write32(0, (uint8_t *)fep->hw_baseaddr_v + ENETFEC_MIBC);

	ecntl |= 0x70000000;
	fep->enetfec_e_cntl = ecntl;
	/* And last, enable the transmit and receive processing */
	rte_write32(rte_cpu_to_le_32(ecntl),
		(uint8_t *)fep->hw_baseaddr_v + ENETFEC_ECR);
	rte_delay_us(10);
}

static int
enetfec_eth_configure(struct rte_eth_dev *dev)
{
	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		ENETFEC_PMD_ERR("PMD does not support KEEP_CRC offload");

	return 0;
}

static int
enetfec_eth_start(struct rte_eth_dev *dev)
{
	enetfec_restart(dev);

	return 0;
}

/* ENETFEC disable function.
 * @param[in] base      ENETFEC base address
 */
static void
enetfec_disable(struct enetfec_private *fep)
{
	rte_write32(rte_read32((uint8_t *)fep->hw_baseaddr_v + ENETFEC_ECR)
		    & ~(fep->enetfec_e_cntl),
		    (uint8_t *)fep->hw_baseaddr_v + ENETFEC_ECR);
}

static int
enetfec_eth_stop(struct rte_eth_dev *dev)
{
	struct enetfec_private *fep = dev->data->dev_private;

	dev->data->dev_started = 0;
	enetfec_disable(fep);

	return 0;
}

static const struct eth_dev_ops enetfec_ops = {
	.dev_configure          = enetfec_eth_configure,
	.dev_start              = enetfec_eth_start,
	.dev_stop               = enetfec_eth_stop
};

static int
enetfec_eth_init(struct rte_eth_dev *dev)
{
	struct enetfec_private *fep = dev->data->dev_private;

	fep->full_duplex = FULL_DUPLEX;
	dev->dev_ops = &enetfec_ops;
	rte_eth_dev_probing_finish(dev);
	return 0;
}

static int
pmd_enetfec_probe(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *dev = NULL;
	struct enetfec_private *fep;
	const char *name;
	int rc;
	int i;
	unsigned int bdsize;

	name = rte_vdev_device_name(vdev);
	ENETFEC_PMD_LOG(INFO, "Initializing pmd_fec for %s", name);

	dev = rte_eth_vdev_allocate(vdev, sizeof(*fep));
	if (dev == NULL)
		return -ENOMEM;

	/* setup board info structure */
	fep = dev->data->dev_private;
	fep->dev = dev;

	fep->max_rx_queues = ENETFEC_MAX_Q;
	fep->max_tx_queues = ENETFEC_MAX_Q;
	fep->quirks = QUIRK_HAS_ENETFEC_MAC | QUIRK_GBIT
		| QUIRK_RACC;

	rc = enetfec_configure();
	if (rc != 0)
		return -ENOMEM;
	rc = config_enetfec_uio(fep);
	if (rc != 0)
		return -ENOMEM;

	/* Get the BD size for distributing among six queues */
	bdsize = (fep->bd_size) / NUM_OF_BD_QUEUES;

	for (i = 0; i < fep->max_tx_queues; i++) {
		fep->dma_baseaddr_t[i] = fep->bd_addr_v;
		fep->bd_addr_p_t[i] = fep->bd_addr_p;
		fep->bd_addr_v = (uint8_t *)fep->bd_addr_v + bdsize;
		fep->bd_addr_p = fep->bd_addr_p + bdsize;
	}
	for (i = 0; i < fep->max_rx_queues; i++) {
		fep->dma_baseaddr_r[i] = fep->bd_addr_v;
		fep->bd_addr_p_r[i] = fep->bd_addr_p;
		fep->bd_addr_v = (uint8_t *)fep->bd_addr_v + bdsize;
		fep->bd_addr_p = fep->bd_addr_p + bdsize;
	}

	rc = enetfec_eth_init(dev);
	if (rc)
		goto failed_init;

	return 0;

failed_init:
	ENETFEC_PMD_ERR("Failed to init");
	return rc;
}

static int
pmd_enetfec_remove(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *eth_dev = NULL;
	int ret;

	/* find the ethdev entry */
	eth_dev = rte_eth_dev_allocated(rte_vdev_device_name(vdev));
	if (eth_dev == NULL)
		return -ENODEV;

	ret = rte_eth_dev_release_port(eth_dev);
	if (ret != 0)
		return -EINVAL;

	ENETFEC_PMD_INFO("Release enetfec sw device");
	return 0;
}

static struct rte_vdev_driver pmd_enetfec_drv = {
	.probe = pmd_enetfec_probe,
	.remove = pmd_enetfec_remove,
};

RTE_PMD_REGISTER_VDEV(ENETFEC_NAME_PMD, pmd_enetfec_drv);
RTE_LOG_REGISTER_DEFAULT(enetfec_logtype_pmd, NOTICE);
