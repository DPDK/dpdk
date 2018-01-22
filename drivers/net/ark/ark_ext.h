/*-
 * BSD LICENSE
 *
 * Copyright (c) 2015-2017 Atomic Rules LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARK_EXT_H_
#define _ARK_EXT_H_

#include <rte_ethdev_driver.h>

/*
 * This is the template file for users who which to define a dynamic
 * extension to the Arkville PMD.   User's who create an extension
 * should include this file and define the necessary and desired
 * functions.
 * Only 1 function is required for an extension, dev_init(); all other
 * functions prototyped in this file are optional.
 */

/*
 * Called post PMD init.
 * The implementation returns its private data that gets passed into
 * all other functions as user_data
 * The ARK extension implementation MUST implement this function
 */
void *dev_init(struct rte_eth_dev *dev, void *a_bar, int port_id);

/* Called during device shutdown */
void dev_uninit(struct rte_eth_dev *dev, void *user_data);

/* This call is optional and allows the
 * extension to specify the number of supported ports.
 */
uint8_t dev_get_port_count(struct rte_eth_dev *dev,
			   void *user_data);

/*
 * The following functions are optional and are directly mapped
 * from the DPDK PMD ops structure.
 * Each function if implemented is called after the ARK PMD
 * implementation executes.
 */

int dev_configure(struct rte_eth_dev *dev,
		  void *user_data);

int dev_start(struct rte_eth_dev *dev,
	      void *user_data);

void dev_stop(struct rte_eth_dev *dev,
	      void *user_data);

void dev_close(struct rte_eth_dev *dev,
	       void *user_data);

int link_update(struct rte_eth_dev *dev,
		int wait_to_complete,
		void *user_data);

int dev_set_link_up(struct rte_eth_dev *dev,
		    void *user_data);

int dev_set_link_down(struct rte_eth_dev *dev,
		      void *user_data);

int stats_get(struct rte_eth_dev *dev,
	       struct rte_eth_stats *stats,
	       void *user_data);

void stats_reset(struct rte_eth_dev *dev,
		 void *user_data);

void mac_addr_add(struct rte_eth_dev *dev,
		  struct ether_addr *macadr,
		  uint32_t index,
		  uint32_t pool,
		  void *user_data);

void mac_addr_remove(struct rte_eth_dev *dev,
		     uint32_t index,
		     void *user_data);

void mac_addr_set(struct rte_eth_dev *dev,
		  struct ether_addr *mac_addr,
		  void *user_data);

int set_mtu(struct rte_eth_dev *dev,
	    uint16_t size,
	    void *user_data);

#endif
