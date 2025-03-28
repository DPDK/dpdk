..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2023 Mucse IC Design Ltd.

RNP Poll Mode driver
====================

The RNP ETHDEV PMD (**librte_net_rnp**) provides poll mode ethdev driver
support for the inbuilt network device found in the **Mucse RNP**

More information can be found at `Mucse, Official Website <https://mucse.com/en/pro/pro.aspx>`_.

Supported Chipsets and NICs
---------------------------

- MUCSE Ethernet Controller N10 Series for 10GbE or 40GbE (Dual-port)

Support of Ethernet Network Controllers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PCIE NICS
^^^^^^^^^

* ``N10G-X2-DC .... Dual-port 10 Gigabit SFP Ethernet Adapter``
* ``N10G-X4-QC .... Quad-port 10 Gigabit SFP Ethernet Adapter``
* ``N10G-X8-OC .... Octa-port 10 Gigabit SFP Ethernet Adapter``

Chip Basic Overview
-------------------
N10 has two functions, each function support muiple ports(1 to 8),which not same as normal pcie network card(one pf for each port).

.. _figure_mucse_nic:

.. figure:: img/mucse_nic_port.*

   rnp mucse nic port.

Features
--------

- Multiple queues for TX and RX
- Receiver Side Steering (RSS)
  Receiver Side Steering (RSS) on IPv4, IPv6, IPv4-TCP/UDP/SCTP, IPv6-TCP/UDP/SCTP
  Inner RSS is only support for vxlan/nvgre
- Promiscuous mode
- Link state information

Prerequisites and Pre-conditions
--------------------------------
- Prepare the system as recommended by DPDK suite.

- Bind the intended N10 device to ``igb_uio`` or ``vfio-pci`` module.

Now DPDK system is ready to detect n10 port.


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
