..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Features Overview
=================

This section explains the supported features that are listed in the
:doc:`overview`.

As a guide to implementers it also shows the structs where the features are
defined and the APIs that can be use to get/set the values.

Following tags used for feature details, these are from driver point of view:

``[uses]``       : Driver uses some kind of input from the application.

``[implements]`` : Driver implements a functionality.

``[provides]``   : Driver provides some kind of data to the application. It is possible
to provide data by implementing some function, but "provides" is used
for cases where provided data can't be represented simply by a function.

``[related]``    : Related API with that feature.


.. _nic_features_speed_capabilities:

Speed capabilities
------------------

Supports getting the speed capabilities that the current device is capable of.

* **[provides] rte_eth_dev_info**: ``speed_capa:ETH_LINK_SPEED_*``.
* **[related]  API**: ``rte_eth_dev_info_get()``.


.. _nic_features_link_status:

Link status
-----------

Supports getting the link speed, duplex mode and link state (up/down).

* **[implements] eth_dev_ops**: ``link_update``.
* **[implements] rte_eth_dev_data**: ``dev_link``.
* **[related]    API**: ``rte_eth_link_get()``, ``rte_eth_link_get_nowait()``.


.. _nic_features_link_status_event:

Link status event
-----------------

Supports Link Status Change interrupts.

* **[uses]       user config**: ``dev_conf.intr_conf.lsc``.
* **[uses]       rte_eth_dev_data**: ``dev_flags:RTE_ETH_DEV_INTR_LSC``.
* **[uses]       rte_eth_event_type**: ``RTE_ETH_EVENT_INTR_LSC``.
* **[implements] rte_eth_dev_data**: ``dev_link``.
* **[provides]   rte_pci_driver.drv_flags**: ``RTE_PCI_DRV_INTR_LSC``.
* **[related]    API**: ``rte_eth_link_get()``, ``rte_eth_link_get_nowait()``.


.. _nic_features_removal_event:

Removal event
-------------

Supports device removal interrupts.

* **[uses]     user config**: ``dev_conf.intr_conf.rmv``.
* **[uses]     rte_eth_dev_data**: ``dev_flags:RTE_ETH_DEV_INTR_RMV``.
* **[uses]     rte_eth_event_type**: ``RTE_ETH_EVENT_INTR_RMV``.
* **[provides] rte_pci_driver.drv_flags**: ``RTE_PCI_DRV_INTR_RMV``.


.. _nic_features_queue_status_event:

Queue status event
------------------

Supports queue enable/disable events.

* **[uses] rte_eth_event_type**: ``RTE_ETH_EVENT_QUEUE_STATE``.


.. _nic_features_rx_interrupt:

Rx interrupt
------------

Supports Rx interrupts.

* **[uses]       user config**: ``dev_conf.intr_conf.rxq``.
* **[implements] eth_dev_ops**: ``rx_queue_intr_enable``, ``rx_queue_intr_disable``.
* **[related]    API**: ``rte_eth_dev_rx_intr_enable()``, ``rte_eth_dev_rx_intr_disable()``.


.. _nic_features_lock-free_tx_queue:

Lock-free Tx queue
------------------

If a PMD advertises DEV_TX_OFFLOAD_MT_LOCKFREE capable, multiple threads can
invoke rte_eth_tx_burst() concurrently on the same Tx queue without SW lock.

* **[uses]    rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_MT_LOCKFREE``.
* **[provides] rte_eth_dev_info**: ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_MT_LOCKFREE``.
* **[related]  API**: ``rte_eth_tx_burst()``.


.. _nic_features_fast_mbuf_free:

Fast mbuf free
--------------

Supports optimization for fast release of mbufs following successful Tx.
Requires that per queue, all mbufs come from the same mempool and has refcnt = 1.

* **[uses]       rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_MBUF_FAST_FREE``.
* **[provides]   rte_eth_dev_info**: ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_MBUF_FAST_FREE``.


.. _nic_features_free_tx_mbuf_on_demand:

Free Tx mbuf on demand
----------------------

Supports freeing consumed buffers on a Tx ring.

* **[implements] eth_dev_ops**: ``tx_done_cleanup``.
* **[related]    API**: ``rte_eth_tx_done_cleanup()``.


.. _nic_features_queue_start_stop:

Queue start/stop
----------------

Supports starting/stopping a specific Rx/Tx queue of a port.

* **[implements] eth_dev_ops**: ``rx_queue_start``, ``rx_queue_stop``, ``tx_queue_start``,
  ``tx_queue_stop``.
* **[related]    API**: ``rte_eth_dev_rx_queue_start()``, ``rte_eth_dev_rx_queue_stop()``,
  ``rte_eth_dev_tx_queue_start()``, ``rte_eth_dev_tx_queue_stop()``.


.. _nic_features_mtu_update:

MTU update
----------

Supports updating port MTU.

* **[implements] eth_dev_ops**: ``mtu_set``.
* **[implements] rte_eth_dev_data**: ``mtu``.
* **[provides]   rte_eth_dev_info**: ``max_rx_pktlen``.
* **[related]    API**: ``rte_eth_dev_set_mtu()``, ``rte_eth_dev_get_mtu()``.


.. _nic_features_jumbo_frame:

Jumbo frame
-----------

Supports Rx jumbo frames.

* **[uses]    rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_JUMBO_FRAME``.
  ``dev_conf.rxmode.max_rx_pkt_len``.
* **[related] rte_eth_dev_info**: ``max_rx_pktlen``.
* **[related] API**: ``rte_eth_dev_set_mtu()``.


.. _nic_features_scattered_rx:

Scattered Rx
------------

Supports receiving segmented mbufs.

* **[uses]       rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_SCATTER``.
* **[implements] datapath**: ``Scattered Rx function``.
* **[implements] rte_eth_dev_data**: ``scattered_rx``.
* **[provides]   eth_dev_ops**: ``rxq_info_get:scattered_rx``.
* **[related]    eth_dev_ops**: ``rx_pkt_burst``.


.. _nic_features_lro:

LRO
---

Supports Large Receive Offload.

* **[uses]       rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_TCP_LRO``.
  ``dev_conf.rxmode.max_lro_pkt_size``.
* **[implements] datapath**: ``LRO functionality``.
* **[implements] rte_eth_dev_data**: ``lro``.
* **[provides]   mbuf**: ``mbuf.ol_flags:PKT_RX_LRO``, ``mbuf.tso_segsz``.
* **[provides]   rte_eth_dev_info**: ``rx_offload_capa,rx_queue_offload_capa:DEV_RX_OFFLOAD_TCP_LRO``.
* **[provides]   rte_eth_dev_info**: ``max_lro_pkt_size``.


.. _nic_features_tso:

TSO
---

Supports TCP Segmentation Offloading.

* **[uses]       rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_TCP_TSO``.
* **[uses]       rte_eth_desc_lim**: ``nb_seg_max``, ``nb_mtu_seg_max``.
* **[uses]       mbuf**: ``mbuf.ol_flags:`` ``PKT_TX_TCP_SEG``, ``PKT_TX_IPV4``, ``PKT_TX_IPV6``, ``PKT_TX_IP_CKSUM``.
* **[uses]       mbuf**: ``mbuf.tso_segsz``, ``mbuf.l2_len``, ``mbuf.l3_len``, ``mbuf.l4_len``.
* **[implements] datapath**: ``TSO functionality``.
* **[provides]   rte_eth_dev_info**: ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_TCP_TSO,DEV_TX_OFFLOAD_UDP_TSO``.


.. _nic_features_promiscuous_mode:

Promiscuous mode
----------------

Supports enabling/disabling promiscuous mode for a port.

* **[implements] eth_dev_ops**: ``promiscuous_enable``, ``promiscuous_disable``.
* **[implements] rte_eth_dev_data**: ``promiscuous``.
* **[related]    API**: ``rte_eth_promiscuous_enable()``, ``rte_eth_promiscuous_disable()``,
  ``rte_eth_promiscuous_get()``.


.. _nic_features_allmulticast_mode:

Allmulticast mode
-----------------

Supports enabling/disabling receiving multicast frames.

* **[implements] eth_dev_ops**: ``allmulticast_enable``, ``allmulticast_disable``.
* **[implements] rte_eth_dev_data**: ``all_multicast``.
* **[related]    API**: ``rte_eth_allmulticast_enable()``,
  ``rte_eth_allmulticast_disable()``, ``rte_eth_allmulticast_get()``.


.. _nic_features_unicast_mac_filter:

Unicast MAC filter
------------------

Supports adding MAC addresses to enable whitelist filtering to accept packets.

* **[implements] eth_dev_ops**: ``mac_addr_set``, ``mac_addr_add``, ``mac_addr_remove``.
* **[implements] rte_eth_dev_data**: ``mac_addrs``.
* **[related]    API**: ``rte_eth_dev_default_mac_addr_set()``,
  ``rte_eth_dev_mac_addr_add()``, ``rte_eth_dev_mac_addr_remove()``,
  ``rte_eth_macaddr_get()``.


.. _nic_features_multicast_mac_filter:

Multicast MAC filter
--------------------

Supports setting multicast addresses to filter.

* **[implements] eth_dev_ops**: ``set_mc_addr_list``.
* **[related]    API**: ``rte_eth_dev_set_mc_addr_list()``.


.. _nic_features_rss_hash:

RSS hash
--------

Supports RSS hashing on RX.

* **[uses]     user config**: ``dev_conf.rxmode.mq_mode`` = ``ETH_MQ_RX_RSS_FLAG``.
* **[uses]     user config**: ``dev_conf.rx_adv_conf.rss_conf``.
* **[uses]     rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_RSS_HASH``.
* **[provides] rte_eth_dev_info**: ``flow_type_rss_offloads``.
* **[provides] mbuf**: ``mbuf.ol_flags:PKT_RX_RSS_HASH``, ``mbuf.rss``.


.. _nic_features_inner_rss:

Inner RSS
---------

Supports RX RSS hashing on Inner headers.

* **[uses]    rte_flow_action_rss**: ``level``.
* **[uses]    rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_RSS_HASH``.
* **[provides] mbuf**: ``mbuf.ol_flags:PKT_RX_RSS_HASH``, ``mbuf.rss``.


.. _nic_features_rss_key_update:

RSS key update
--------------

Supports configuration of Receive Side Scaling (RSS) hash computation. Updating
Receive Side Scaling (RSS) hash key.

* **[implements] eth_dev_ops**: ``rss_hash_update``, ``rss_hash_conf_get``.
* **[provides]   rte_eth_dev_info**: ``hash_key_size``.
* **[related]    API**: ``rte_eth_dev_rss_hash_update()``,
  ``rte_eth_dev_rss_hash_conf_get()``.


.. _nic_features_rss_reta_update:

RSS reta update
---------------

Supports updating Redirection Table of the Receive Side Scaling (RSS).

* **[implements] eth_dev_ops**: ``reta_update``, ``reta_query``.
* **[provides]   rte_eth_dev_info**: ``reta_size``.
* **[related]    API**: ``rte_eth_dev_rss_reta_update()``, ``rte_eth_dev_rss_reta_query()``.


.. _nic_features_vmdq:

VMDq
----

Supports Virtual Machine Device Queues (VMDq).

* **[uses] user config**: ``dev_conf.rxmode.mq_mode`` = ``ETH_MQ_RX_VMDQ_FLAG``.
* **[uses] user config**: ``dev_conf.rx_adv_conf.vmdq_dcb_conf``.
* **[uses] user config**: ``dev_conf.rx_adv_conf.vmdq_rx_conf``.
* **[uses] user config**: ``dev_conf.tx_adv_conf.vmdq_dcb_tx_conf``.
* **[uses] user config**: ``dev_conf.tx_adv_conf.vmdq_tx_conf``.


.. _nic_features_sriov:

SR-IOV
------

Driver supports creating Virtual Functions.

* **[implements] rte_eth_dev_data**: ``sriov``.

.. _nic_features_dcb:

DCB
---

Supports Data Center Bridging (DCB).

* **[uses]       user config**: ``dev_conf.rxmode.mq_mode`` = ``ETH_MQ_RX_DCB_FLAG``.
* **[uses]       user config**: ``dev_conf.rx_adv_conf.vmdq_dcb_conf``.
* **[uses]       user config**: ``dev_conf.rx_adv_conf.dcb_rx_conf``.
* **[uses]       user config**: ``dev_conf.tx_adv_conf.vmdq_dcb_tx_conf``.
* **[uses]       user config**: ``dev_conf.tx_adv_conf.vmdq_tx_conf``.
* **[implements] eth_dev_ops**: ``get_dcb_info``.
* **[related]    API**: ``rte_eth_dev_get_dcb_info()``.


.. _nic_features_vlan_filter:

VLAN filter
-----------

Supports filtering of a VLAN Tag identifier.

* **[uses]       rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_VLAN_FILTER``.
* **[implements] eth_dev_ops**: ``vlan_filter_set``.
* **[related]    API**: ``rte_eth_dev_vlan_filter()``.


.. _nic_features_flow_control:

Flow control
------------

Supports configuring link flow control.

* **[implements] eth_dev_ops**: ``flow_ctrl_get``, ``flow_ctrl_set``,
  ``priority_flow_ctrl_set``.
* **[related]    API**: ``rte_eth_dev_flow_ctrl_get()``, ``rte_eth_dev_flow_ctrl_set()``,
  ``rte_eth_dev_priority_flow_ctrl_set()``.


.. _nic_features_flow_api:

Flow API
--------

Supports the DPDK Flow API for generic filtering.

* **[implements] eth_dev_ops**: ``filter_ctrl:RTE_ETH_FILTER_GENERIC``.
* **[implements] rte_flow_ops**: ``All``.


.. _nic_features_rate_limitation:

Rate limitation
---------------

Supports Tx rate limitation for a queue.

* **[implements] eth_dev_ops**: ``set_queue_rate_limit``.
* **[related]    API**: ``rte_eth_set_queue_rate_limit()``.


.. _nic_features_traffic_mirroring:

Traffic mirroring
-----------------

Supports adding traffic mirroring rules.

* **[implements] eth_dev_ops**: ``mirror_rule_set``, ``mirror_rule_reset``.
* **[related]    API**: ``rte_eth_mirror_rule_set()``, ``rte_eth_mirror_rule_reset()``.


.. _nic_features_inline_crypto_doc:

Inline crypto
-------------

Supports inline crypto processing defined by rte_security library to perform crypto
operations of security protocol while packet is received in NIC. NIC is not aware
of protocol operations. See Security library and PMD documentation for more details.

* **[uses]       rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_SECURITY``,
* **[uses]       rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_SECURITY``.
* **[implements] rte_security_ops**: ``session_create``, ``session_update``,
  ``session_stats_get``, ``session_destroy``, ``set_pkt_metadata``, ``capabilities_get``.
* **[provides] rte_eth_dev_info**: ``rx_offload_capa,rx_queue_offload_capa:DEV_RX_OFFLOAD_SECURITY``,
  ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_SECURITY``.
* **[provides]   mbuf**: ``mbuf.ol_flags:PKT_RX_SEC_OFFLOAD``,
  ``mbuf.ol_flags:PKT_TX_SEC_OFFLOAD``, ``mbuf.ol_flags:PKT_RX_SEC_OFFLOAD_FAILED``.
* **[provides]   rte_security_ops, capabilities_get**:  ``action: RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO``


.. _nic_features_inline_protocol_doc:

Inline protocol
---------------

Supports inline protocol processing defined by rte_security library to perform
protocol processing for the security protocol (e.g. IPsec, MACSEC) while the
packet is received at NIC. The NIC is capable of understanding the security
protocol operations. See security library and PMD documentation for more details.

* **[uses]       rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_SECURITY``,
* **[uses]       rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_SECURITY``.
* **[implements] rte_security_ops**: ``session_create``, ``session_update``,
  ``session_stats_get``, ``session_destroy``, ``set_pkt_metadata``, ``get_userdata``,
  ``capabilities_get``.
* **[provides] rte_eth_dev_info**: ``rx_offload_capa,rx_queue_offload_capa:DEV_RX_OFFLOAD_SECURITY``,
  ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_SECURITY``.
* **[provides]   mbuf**: ``mbuf.ol_flags:PKT_RX_SEC_OFFLOAD``,
  ``mbuf.ol_flags:PKT_TX_SEC_OFFLOAD``, ``mbuf.ol_flags:PKT_RX_SEC_OFFLOAD_FAILED``.
* **[provides]   rte_security_ops, capabilities_get**:  ``action: RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL``


.. _nic_features_crc_offload:

CRC offload
-----------

Supports CRC stripping by hardware.
A PMD assumed to support CRC stripping by default. PMD should advertise if it supports keeping CRC.

* **[uses] rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_KEEP_CRC``.


.. _nic_features_vlan_offload:

VLAN offload
------------

Supports VLAN offload to hardware.

* **[uses]       rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_VLAN_STRIP,DEV_RX_OFFLOAD_VLAN_FILTER,DEV_RX_OFFLOAD_VLAN_EXTEND``.
* **[uses]       rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_VLAN_INSERT``.
* **[uses]       mbuf**: ``mbuf.ol_flags:PKT_TX_VLAN``, ``mbuf.vlan_tci``.
* **[implements] eth_dev_ops**: ``vlan_offload_set``.
* **[provides]   mbuf**: ``mbuf.ol_flags:PKT_RX_VLAN_STRIPPED``, ``mbuf.ol_flags:PKT_RX_VLAN`` ``mbuf.vlan_tci``.
* **[provides]   rte_eth_dev_info**: ``rx_offload_capa,rx_queue_offload_capa:DEV_RX_OFFLOAD_VLAN_STRIP``,
  ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_VLAN_INSERT``.
* **[related]    API**: ``rte_eth_dev_set_vlan_offload()``,
  ``rte_eth_dev_get_vlan_offload()``.


.. _nic_features_qinq_offload:

QinQ offload
------------

Supports QinQ (queue in queue) offload.

* **[uses]     rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_QINQ_STRIP``.
* **[uses]     rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_QINQ_INSERT``.
* **[uses]     mbuf**: ``mbuf.ol_flags:PKT_TX_QINQ``, ``mbuf.vlan_tci_outer``.
* **[provides] mbuf**: ``mbuf.ol_flags:PKT_RX_QINQ_STRIPPED``, ``mbuf.ol_flags:PKT_RX_QINQ``,
  ``mbuf.ol_flags:PKT_RX_VLAN_STRIPPED``, ``mbuf.ol_flags:PKT_RX_VLAN``
  ``mbuf.vlan_tci``, ``mbuf.vlan_tci_outer``.
* **[provides] rte_eth_dev_info**: ``rx_offload_capa,rx_queue_offload_capa:DEV_RX_OFFLOAD_QINQ_STRIP``,
  ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_QINQ_INSERT``.


.. _nic_features_l3_checksum_offload:

L3 checksum offload
-------------------

Supports L3 checksum offload.

* **[uses]     rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_IPV4_CKSUM``.
* **[uses]     rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_IPV4_CKSUM``.
* **[uses]     mbuf**: ``mbuf.ol_flags:PKT_TX_IP_CKSUM``,
  ``mbuf.ol_flags:PKT_TX_IPV4`` | ``PKT_TX_IPV6``.
* **[uses]     mbuf**: ``mbuf.l2_len``, ``mbuf.l3_len``.
* **[provides] mbuf**: ``mbuf.ol_flags:PKT_RX_IP_CKSUM_UNKNOWN`` |
  ``PKT_RX_IP_CKSUM_BAD`` | ``PKT_RX_IP_CKSUM_GOOD`` |
  ``PKT_RX_IP_CKSUM_NONE``.
* **[provides] rte_eth_dev_info**: ``rx_offload_capa,rx_queue_offload_capa:DEV_RX_OFFLOAD_IPV4_CKSUM``,
  ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_IPV4_CKSUM``.


.. _nic_features_l4_checksum_offload:

L4 checksum offload
-------------------

Supports L4 checksum offload.

* **[uses]     rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_UDP_CKSUM,DEV_RX_OFFLOAD_TCP_CKSUM,DEV_RX_OFFLOAD_SCTP_CKSUM``.
* **[uses]     rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_UDP_CKSUM,DEV_TX_OFFLOAD_TCP_CKSUM,DEV_TX_OFFLOAD_SCTP_CKSUM``.
* **[uses]     mbuf**: ``mbuf.ol_flags:PKT_TX_IPV4`` | ``PKT_TX_IPV6``,
  ``mbuf.ol_flags:PKT_TX_L4_NO_CKSUM`` | ``PKT_TX_TCP_CKSUM`` |
  ``PKT_TX_SCTP_CKSUM`` | ``PKT_TX_UDP_CKSUM``.
* **[uses]     mbuf**: ``mbuf.l2_len``, ``mbuf.l3_len``.
* **[provides] mbuf**: ``mbuf.ol_flags:PKT_RX_L4_CKSUM_UNKNOWN`` |
  ``PKT_RX_L4_CKSUM_BAD`` | ``PKT_RX_L4_CKSUM_GOOD`` |
  ``PKT_RX_L4_CKSUM_NONE``.
* **[provides] rte_eth_dev_info**: ``rx_offload_capa,rx_queue_offload_capa:DEV_RX_OFFLOAD_UDP_CKSUM,DEV_RX_OFFLOAD_TCP_CKSUM,DEV_RX_OFFLOAD_SCTP_CKSUM``,
  ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_UDP_CKSUM,DEV_TX_OFFLOAD_TCP_CKSUM,DEV_TX_OFFLOAD_SCTP_CKSUM``.

.. _nic_features_hw_timestamp:

Timestamp offload
-----------------

Supports Timestamp.

* **[uses]     rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_TIMESTAMP``.
* **[provides] mbuf**: ``mbuf.ol_flags:PKT_RX_TIMESTAMP``.
* **[provides] mbuf**: ``mbuf.timestamp``.
* **[provides] rte_eth_dev_info**: ``rx_offload_capa,rx_queue_offload_capa: DEV_RX_OFFLOAD_TIMESTAMP``.
* **[related] eth_dev_ops**: ``read_clock``.

.. _nic_features_macsec_offload:

MACsec offload
--------------

Supports MACsec.

* **[uses]     rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_MACSEC_STRIP``.
* **[uses]     rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_MACSEC_INSERT``.
* **[uses]     mbuf**: ``mbuf.ol_flags:PKT_TX_MACSEC``.
* **[provides] rte_eth_dev_info**: ``rx_offload_capa,rx_queue_offload_capa:DEV_RX_OFFLOAD_MACSEC_STRIP``,
  ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_MACSEC_INSERT``.


.. _nic_features_inner_l3_checksum:

Inner L3 checksum
-----------------

Supports inner packet L3 checksum.

* **[uses]     rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM``.
* **[uses]     rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM``.
* **[uses]     mbuf**: ``mbuf.ol_flags:PKT_TX_IP_CKSUM``,
  ``mbuf.ol_flags:PKT_TX_IPV4`` | ``PKT_TX_IPV6``,
  ``mbuf.ol_flags:PKT_TX_OUTER_IP_CKSUM``,
  ``mbuf.ol_flags:PKT_TX_OUTER_IPV4`` | ``PKT_TX_OUTER_IPV6``.
* **[uses]     mbuf**: ``mbuf.outer_l2_len``, ``mbuf.outer_l3_len``.
* **[provides] mbuf**: ``mbuf.ol_flags:PKT_RX_EIP_CKSUM_BAD``.
* **[provides] rte_eth_dev_info**: ``rx_offload_capa,rx_queue_offload_capa:DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM``,
  ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM``.


.. _nic_features_inner_l4_checksum:

Inner L4 checksum
-----------------

Supports inner packet L4 checksum.

* **[uses]     rte_eth_rxconf,rte_eth_rxmode**: ``offloads:DEV_RX_OFFLOAD_OUTER_UDP_CKSUM``.
* **[provides] mbuf**: ``mbuf.ol_flags:PKT_RX_OUTER_L4_CKSUM_UNKNOWN`` |
  ``PKT_RX_OUTER_L4_CKSUM_BAD`` | ``PKT_RX_OUTER_L4_CKSUM_GOOD`` | ``PKT_RX_OUTER_L4_CKSUM_INVALID``.
* **[uses]     rte_eth_txconf,rte_eth_txmode**: ``offloads:DEV_TX_OFFLOAD_OUTER_UDP_CKSUM``.
* **[uses]     mbuf**: ``mbuf.ol_flags:PKT_TX_OUTER_IPV4`` | ``PKT_TX_OUTER_IPV6``.
  ``mbuf.ol_flags:PKT_TX_OUTER_UDP_CKSUM``.
* **[uses]     mbuf**: ``mbuf.outer_l2_len``, ``mbuf.outer_l3_len``.
* **[provides] rte_eth_dev_info**: ``rx_offload_capa,rx_queue_offload_capa:DEV_RX_OFFLOAD_OUTER_UDP_CKSUM``,
  ``tx_offload_capa,tx_queue_offload_capa:DEV_TX_OFFLOAD_OUTER_UDP_CKSUM``.


.. _nic_features_packet_type_parsing:

Packet type parsing
-------------------

Supports packet type parsing and returns a list of supported types.
Allows application to set ptypes it is interested in.

* **[implements] eth_dev_ops**: ``dev_supported_ptypes_get``,
* **[related]    API**: ``rte_eth_dev_get_supported_ptypes()``,
  ``rte_eth_dev_set_ptypes()``, ``dev_ptypes_set``.
* **[provides]   mbuf**: ``mbuf.packet_type``.


.. _nic_features_timesync:

Timesync
--------

Supports IEEE1588/802.1AS timestamping.

* **[implements] eth_dev_ops**: ``timesync_enable``, ``timesync_disable``
  ``timesync_read_rx_timestamp``, ``timesync_read_tx_timestamp``,
  ``timesync_adjust_time``, ``timesync_read_time``, ``timesync_write_time``.
* **[related]    API**: ``rte_eth_timesync_enable()``, ``rte_eth_timesync_disable()``,
  ``rte_eth_timesync_read_rx_timestamp()``,
  ``rte_eth_timesync_read_tx_timestamp``, ``rte_eth_timesync_adjust_time()``,
  ``rte_eth_timesync_read_time()``, ``rte_eth_timesync_write_time()``.


.. _nic_features_rx_descriptor_status:

Rx descriptor status
--------------------

Supports check the status of a Rx descriptor. When ``rx_descriptor_status`` is
used, status can be "Available", "Done" or "Unavailable". When
``rx_descriptor_done`` is used, status can be "DD bit is set" or "DD bit is
not set".

* **[implements] eth_dev_ops**: ``rx_descriptor_status``.
* **[related]    API**: ``rte_eth_rx_descriptor_status()``.
* **[implements] eth_dev_ops**: ``rx_descriptor_done``.
* **[related]    API**: ``rte_eth_rx_descriptor_done()``.


.. _nic_features_tx_descriptor_status:

Tx descriptor status
--------------------

Supports checking the status of a Tx descriptor. Status can be "Full", "Done"
or "Unavailable."

* **[implements] eth_dev_ops**: ``tx_descriptor_status``.
* **[related]    API**: ``rte_eth_tx_descriptor_status()``.


.. _nic_features_basic_stats:

Basic stats
-----------

Support basic statistics such as: ipackets, opackets, ibytes, obytes,
imissed, ierrors, oerrors, rx_nombuf.

And per queue stats: q_ipackets, q_opackets, q_ibytes, q_obytes, q_errors.

These apply to all drivers.

* **[implements] eth_dev_ops**: ``stats_get``, ``stats_reset``.
* **[related]    API**: ``rte_eth_stats_get``, ``rte_eth_stats_reset()``.


.. _nic_features_extended_stats:

Extended stats
--------------

Supports Extended Statistics, changes from driver to driver.

* **[implements] eth_dev_ops**: ``xstats_get``, ``xstats_reset``, ``xstats_get_names``.
* **[implements] eth_dev_ops**: ``xstats_get_by_id``, ``xstats_get_names_by_id``.
* **[related]    API**: ``rte_eth_xstats_get()``, ``rte_eth_xstats_reset()``,
  ``rte_eth_xstats_get_names``, ``rte_eth_xstats_get_by_id()``,
  ``rte_eth_xstats_get_names_by_id()``, ``rte_eth_xstats_get_id_by_name()``.


.. _nic_features_stats_per_queue:

Stats per queue
---------------

Supports configuring per-queue stat counter mapping.

* **[implements] eth_dev_ops**: ``queue_stats_mapping_set``.
* **[related]    API**: ``rte_eth_dev_set_rx_queue_stats_mapping()``,
  ``rte_eth_dev_set_tx_queue_stats_mapping()``.


.. _nic_features_fw_version:

FW version
----------

Supports getting device hardware firmware information.

* **[implements] eth_dev_ops**: ``fw_version_get``.
* **[related]    API**: ``rte_eth_dev_fw_version_get()``.


.. _nic_features_eeprom_dump:

EEPROM dump
-----------

Supports getting/setting device eeprom data.

* **[implements] eth_dev_ops**: ``get_eeprom_length``, ``get_eeprom``, ``set_eeprom``.
* **[related]    API**: ``rte_eth_dev_get_eeprom_length()``, ``rte_eth_dev_get_eeprom()``,
  ``rte_eth_dev_set_eeprom()``.


.. _nic_features_module_eeprom_dump:

Module EEPROM dump
------------------

Supports getting information and data of plugin module eeprom.

* **[implements] eth_dev_ops**: ``get_module_info``, ``get_module_eeprom``.
* **[related]    API**: ``rte_eth_dev_get_module_info()``, ``rte_eth_dev_get_module_eeprom()``.


.. _nic_features_register_dump:

Registers dump
--------------

Supports retrieving device registers and registering attributes (number of
registers and register size).

* **[implements] eth_dev_ops**: ``get_reg``.
* **[related]    API**: ``rte_eth_dev_get_reg_info()``.


.. _nic_features_led:

LED
---

Supports turning on/off a software controllable LED on a device.

* **[implements] eth_dev_ops**: ``dev_led_on``, ``dev_led_off``.
* **[related]    API**: ``rte_eth_led_on()``, ``rte_eth_led_off()``.


.. _nic_features_multiprocess_aware:

Multiprocess aware
------------------

Driver can be used for primary-secondary process model.


.. _nic_features_bsd_nic_uio:

BSD nic_uio
-----------

BSD ``nic_uio`` module supported.


.. _nic_features_linux_uio:

Linux UIO
---------

Works with ``igb_uio`` kernel module.

* **[provides] RTE_PMD_REGISTER_KMOD_DEP**: ``igb_uio``.

.. _nic_features_linux_vfio:

Linux VFIO
----------

Works with ``vfio-pci`` kernel module.

* **[provides] RTE_PMD_REGISTER_KMOD_DEP**: ``vfio-pci``.

.. _nic_features_other_kdrv:

Other kdrv
----------

Kernel module other than above ones supported.


.. _nic_features_armv7:

ARMv7
-----

Support armv7 architecture.

Use ``defconfig_arm-armv7a-*-*``.


.. _nic_features_armv8:

ARMv8
-----

Support armv8a (64bit) architecture.

Use ``defconfig_arm64-armv8a-*-*``


.. _nic_features_power8:

Power8
------

Support PowerPC architecture.

Use ``defconfig_ppc_64-power8-*-*``

.. _nic_features_x86-32:

x86-32
------

Support 32bits x86 architecture.

Use ``defconfig_x86_x32-native-*-*`` and ``defconfig_i686-native-*-*``.


.. _nic_features_x86-64:

x86-64
------

Support 64bits x86 architecture.

Use ``defconfig_x86_64-native-*-*``.


.. _nic_features_usage_doc:

Usage doc
---------

Documentation describes usage.

See ``doc/guides/nics/*.rst``


.. _nic_features_design_doc:

Design doc
----------

Documentation describes design.

See ``doc/guides/nics/*.rst``.


.. _nic_features_perf_doc:

Perf doc
--------

Documentation describes performance values.

See ``dpdk.org/doc/perf/*``.

.. _nic_features_runtime_rx_queue_setup:

Runtime Rx queue setup
----------------------

Supports Rx queue setup after device started.

* **[provides] rte_eth_dev_info**: ``dev_capa:RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP``.
* **[related]  API**: ``rte_eth_dev_info_get()``.

.. _nic_features_runtime_tx_queue_setup:

Runtime Tx queue setup
----------------------

Supports Tx queue setup after device started.

* **[provides] rte_eth_dev_info**: ``dev_capa:RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP``.
* **[related]  API**: ``rte_eth_dev_info_get()``.

.. _nic_features_burst_mode_info:

Burst mode info
---------------

Supports to get Rx/Tx packet burst mode information.

* **[implements] eth_dev_ops**: ``rx_burst_mode_get``, ``tx_burst_mode_get``.
* **[related] API**: ``rte_eth_rx_burst_mode_get()``, ``rte_eth_tx_burst_mode_get()``.

.. _nic_features_other:

Other dev ops not represented by a Feature
------------------------------------------

* ``rxq_info_get``
* ``txq_info_get``
* ``vlan_tpid_set``
* ``vlan_strip_queue_set``
* ``vlan_pvid_set``
* ``rx_queue_count``
* ``l2_tunnel_offload_set``
* ``uc_hash_table_set``
* ``uc_all_hash_table_set``
* ``udp_tunnel_port_add``
* ``udp_tunnel_port_del``
* ``l2_tunnel_eth_type_conf``
* ``l2_tunnel_offload_set``
* ``tx_pkt_prepare``
