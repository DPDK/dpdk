.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2023 Mucse IC Design Ltd.

RNP Poll Mode driver
====================

The RNP ethdev PMD (**librte_net_rnp**) provides poll mode ethdev driver
support for the inbuilt network device found in the **Mucse RNP**

More information can be found at `Mucse, Official Website <https://mucse.com/en/pro/pro.aspx>`_.


Supported Chipsets and NICs
---------------------------

- MUCSE Ethernet Controller N10 Series for 10GbE or 40GbE (Dual-port)

Support of Ethernet Network Controllers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PCIe NICs
^^^^^^^^^

* ``N10G-X2-DC .... Dual-port 10 Gigabit SFP Ethernet Adapter``
* ``N10G-X4-QC .... Quad-port 10 Gigabit SFP Ethernet Adapter``
* ``N10G-X8-OC .... Octa-port 10 Gigabit SFP Ethernet Adapter``


Chip Basic Overview
-------------------

N10 has two functions, each function support multiple ports (1 to 8),
which is different of normal PCIe network card (one PF for each port).

.. _figure_mucse_nic:

.. figure:: img/mucse_nic_port.*

   rnp Mucse NIC port.


Features
--------

- Multiple queues for Tx and Rx
- Receiver Side Steering (RSS)
  Receiver Side Steering (RSS) on IPv4, IPv6, IPv4-TCP/UDP/SCTP, IPv6-TCP/UDP/SCTP
  Inner RSS is only supported for VXLAN/NVGRE
- Promiscuous mode
- Link state information
- MTU update
- MAC filtering
- Jumbo frames
- Scatter-Gather IO support
- Port hardware statistics
- Packet type parsing
- Checksum offload
- TSO offload
- VLAN stripping and VLAN/QINQ insertion


Prerequisites and Pre-conditions
--------------------------------

#. Prepare the system as recommended by DPDK suite.

#. Bind the intended N10 device to ``igb_uio`` or ``vfio-pci`` module.

Now DPDK system is ready to detect N10 ports.


Limitations or Known issues
---------------------------

X86-32, BSD, Armv7, RISC-V, Windows, are not supported yet.


Supported APIs
--------------

rte_eth APIs
~~~~~~~~~~~~

Listed below are the rte_eth functions supported:

* ``rte_eth_dev_close``
* ``rte_eth_dev_stop``
* ``rte_eth_dev_infos_get``
* ``rte_eth_dev_rx_queue_start``
* ``rte_eth_dev_rx_queue_stop``
* ``rte_eth_dev_tx_queue_start``
* ``rte_eth_dev_tx_queue_stop``
* ``rte_eth_dev_start``
* ``rte_eth_dev_stop``
* ``rte_eth_dev_rss_hash_conf_get``
* ``rte_eth_dev_rss_hash_update``
* ``rte_eth_dev_rss_reta_query``
* ``rte_eth_dev_rss_reta_update``
* ``rte_eth_dev_set_link_down``
* ``rte_eth_dev_set_link_up``
* ``rte_eth_dev_get_mtu``
* ``rte_eth_dev_set_mtu``
* ``rte_eth_dev_default_mac_addr_set``
* ``rte_eth_dev_mac_addr_add``
* ``rte_eth_dev_mac_addr_remove``
* ``rte_eth_dev_get_supported_ptypes``
* ``rte_eth_dev_get_vlan_offload``
* ``rte_eth_dev_set_vlan_offload``
* ``rte_eth_dev_set_vlan_strip_on_queue``
* ``rte_eth_promiscuous_disable``
* ``rte_eth_promiscuous_enable``
* ``rte_eth_allmulticast_enable``
* ``rte_eth_allmulticast_disable``
* ``rte_eth_promiscuous_get``
* ``rte_eth_allmulticast_get``
* ``rte_eth_rx_queue_setup``
* ``rte_eth_tx_queue_setup``
* ``rte_eth_link_get``
* ``rte_eth_link_get_nowait``
* ``rte_eth_stats_get``
* ``rte_eth_stats_reset``
* ``rte_eth_xstats_get``
* ``rte_eth_xstats_reset``
* ``rte_eth_xstats_get_names``
* ``rte_eth_macaddr_get``
* ``rte_eth_macaddrs_get``
