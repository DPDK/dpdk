/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_EXT_H_
#define _ARK_EXT_H_

#include <ethdev_driver.h>

/* The following section lists function prototypes for Arkville's
 * dynamic PMD extension. User's who create an extension
 * must include this file and define the necessary and desired
 * functions. Only 1 function is required for an extension,
 * rte_pmd_ark_dev_init(); all other functions prototypes in this
 * section are optional.
 * See documentation for compiling and use of extensions.
 */

/**
 * Extension prototype, required implementation if extensions are used.
 * Called during device probe to initialize the user structure
 * passed to other extension functions.  This is called once for each
 * port of the device.
 *
 * @param dev
 *   current device.
 * @param a_bar
 *   access to PCIe device bar (application bar) and hence access to
 *   user's portion of FPGA.
 * @param port_id
 *   port identifier.
 * @return user_data
 *   which will be passed to other extension functions.
 */
void *rte_pmd_ark_dev_init(struct rte_eth_dev *dev, void *a_bar, int port_id);

/**
 * Extension prototype, optional implementation.
 * Called during device uninit.
 *
 * @param dev
 *   current device.
 * @param user_data
 *   user argument from dev_init() call.
 */
void rte_pmd_ark_dev_uninit(struct rte_eth_dev *dev, void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during device probe to change the port count from 1.
 *
 * @param dev
 *   current device.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
uint8_t dev_get_port_count(struct rte_eth_dev *dev, void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_dev_configure().
 *
 * @param dev
 *   current device.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
int rte_pmd_ark_dev_configure(struct rte_eth_dev *dev, void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_dev_start().
 *
 * @param dev
 *   current device.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
int rte_pmd_ark_dev_start(struct rte_eth_dev *dev, void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during  rte_eth_dev_stop().
 *
 * @param dev
 *   current device.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
void rte_pmd_ark_dev_stop(struct rte_eth_dev *dev, void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_dev_close().
 *
 * @param dev
 *   current device.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
void rte_pmd_ark_dev_close(struct rte_eth_dev *dev, void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during link_update status event.
 *
 * @param dev
 *   current device.
 * @param wait_to_complete
 *    argument from update event.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
int rte_pmd_ark_link_update(struct rte_eth_dev *dev,
			    int wait_to_complete,
			    void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_dev_set_link_up().
 *
 * @param dev
 *   current device.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
int rte_pmd_ark_dev_set_link_up(struct rte_eth_dev *dev, void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_dev_set_link_down().
 *
 * @param dev
 *   current device.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
int rte_pmd_ark_dev_set_link_down(struct rte_eth_dev *dev, void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_stats_get(); allows updates to the stats
 * struct in addition Ark's PMD operations.
 *
 * @param dev
 *   current device.
 * @param stats
 *   statistics struct already populated by Ark PMD.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
int rte_pmd_ark_stats_get(struct rte_eth_dev *dev,
			  struct rte_eth_stats *stats,
			  void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_dev_stats_reset().
 *
 * @param dev
 *   current device.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
void rte_pmd_ark_stats_reset(struct rte_eth_dev *dev, void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_dev_mac_addr_add().
 *
 * @param dev
 *   current device.
 * @param macaddr
 *   The MAC address to add
 * @param index
 *   The index into the MAC address array.
 * @param pool
 *   VMDq pool index from caller
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
void rte_pmd_ark_mac_addr_add(struct rte_eth_dev *dev,
			      struct rte_ether_addr *macaddr,
			      uint32_t index,
			      uint32_t pool,
			      void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_dev_mac_addr_remove().
 *
 * @param dev
 *   current device.
 * @param index
 *   The index into the MAC address array.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
void rte_pmd_ark_mac_addr_remove(struct rte_eth_dev *dev,
				 uint32_t index,
				 void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_dev_default_mac_addr_set().
 *
 * @param dev
 *   current device.
 * @param mac_addr
 *   The new default MAC address.
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
void rte_pmd_ark_mac_addr_set(struct rte_eth_dev *dev,
			      struct rte_ether_addr *mac_addr,
			      void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_dev_set_mtu().
 *
 * @param dev
 *   current device.
 * @param size
 *   The MTU to be applied
 * @param user_data
 *   user argument from dev_init() call.
 * @return (0) if successful.
 */
int rte_pmd_ark_set_mtu(struct rte_eth_dev *dev,
			uint16_t size,
			void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_rx_burst() for each packet. This extension
 * function allows the transfer of meta data from the user's FPGA to
 * mbuf fields.
 *
 * @param mbuf
 *   The newly received mbuf
 * @param meta
 *   The meta data from the user, up to 20 bytes. The
 *   underlying data in the PMD is of type uint32_t meta[5];
 * @param user_data
 *   user argument from dev_init() call.
 */
void rte_pmd_ark_rx_user_meta_hook(struct rte_mbuf *mbuf,
				   const uint32_t *meta,
				   void *user_data);

/**
 * Extension prototype, optional implementation.
 * Called during rte_eth_tx_burst() for each packet. This extension
 * function allows the transfer of data from the mbuf to the user's
 * FPGA.  Up to 20 bytes (5 32-bit words) are transferable
 *
 * @param mbuf
 *   The mbuf about to be transmitted.
 * @param meta
 *   The meta data to be populate by this call. The
 *   underlying in the PMD is of type uint32_t meta[5];
 * @param meta_cnt
 *   The count in 32-bit words of the meta data populated, 0 to 5.
 * @param user_data
 *   user argument from dev_init() call.
 */
void rte_pmd_ark_tx_user_meta_hook(const struct rte_mbuf *mbuf,
				   uint32_t *meta,
				   uint8_t *meta_cnt,
				   void *user_data);

#endif
