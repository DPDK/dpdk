/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Aquantia Corporation
 */

#include <ethdev_driver.h>

#include <eal_export.h>
#include "rte_pmd_atlantic.h"
#include "atl_ethdev.h"


RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_atl_macsec_enable, 19.05)
int
rte_pmd_atl_macsec_enable(uint16_t port,
			  uint8_t encr, uint8_t repl_prot)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_atlantic_supported(dev))
		return -ENOTSUP;

	return atl_macsec_enable(dev, encr, repl_prot);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_atl_macsec_disable, 19.05)
int
rte_pmd_atl_macsec_disable(uint16_t port)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_atlantic_supported(dev))
		return -ENOTSUP;

	return atl_macsec_disable(dev);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_atl_macsec_config_txsc, 19.05)
int
rte_pmd_atl_macsec_config_txsc(uint16_t port, uint8_t *mac)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_atlantic_supported(dev))
		return -ENOTSUP;

	return atl_macsec_config_txsc(dev, mac);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_atl_macsec_config_rxsc, 19.05)
int
rte_pmd_atl_macsec_config_rxsc(uint16_t port, uint8_t *mac, uint16_t pi)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_atlantic_supported(dev))
		return -ENOTSUP;

	return atl_macsec_config_rxsc(dev, mac, pi);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_atl_macsec_select_txsa, 19.05)
int
rte_pmd_atl_macsec_select_txsa(uint16_t port, uint8_t idx, uint8_t an,
				 uint32_t pn, uint8_t *key)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_atlantic_supported(dev))
		return -ENOTSUP;

	return atl_macsec_select_txsa(dev, idx, an, pn, key);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_atl_macsec_select_rxsa, 19.05)
int
rte_pmd_atl_macsec_select_rxsa(uint16_t port, uint8_t idx, uint8_t an,
				 uint32_t pn, uint8_t *key)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_atlantic_supported(dev))
		return -ENOTSUP;

	return atl_macsec_select_rxsa(dev, idx, an, pn, key);
}
