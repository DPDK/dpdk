..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2016-2019 Broadcom

BNXT Poll Mode Driver
=====================

The BNXT PMD (**librte_pmd_bnxt**) implements support for adapters based on
Ethernet controllers and SoCs belonging to the **Broadcom BCM5730X NetXtreme-C®
Family of Ethernet Network Controllers**, the **Broadcom BCM574XX/BCM575XX
NetXtreme-E® Family of Ethernet Network Controllers**, the **Broadcom BCM588XX
Stingray Family of SmartNIC Adapters**, and the **Broadcom StrataGX® BCM5871X
Series of Communications Processors**.  A complete list with links to reference
material is included below.


BNXT PMD Features
-----------------

The BNXT PMD includes support for the following features:

   * Multiple transmit and receive queues
   * Queue start/stop
   * RSS hash
   * RSS key configuration
   * RSS reta configuration
   * VMDq
   * Packet type parsing
   * Configurable RX CRC stripping
   * L3/L4 checksum offload
   * LRO offload
   * TSO offload
   * VLAN offload
   * SR-IOV VF
   * Basic and extended port statistics
   * Link state reporting
   * Flow control
   * Ethertype filtering
   * N-tuple filtering
   * Promiscuous mode
   * Unicast and multicast MAC filtering
   * Scatter/gather transmit and receive
   * Jumbo frames
   * Vector PMD

BNXT Vector PMD
---------------

The BNXT PMD includes support for SSE vector mode on x86 platforms. Vector
provides significantly improved performance over the base implementation,
however it does not support all of the features that are supported by the
base (non-vector) implementation. Vector mode will be selected and enabled
automatically when the port is started if allowed by the current configuration.

RX Requirements for Vector Mode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Vector mode receive will be enabled if the following constrainsts are met:
   * Packets must fit within a single mbuf (no scatter RX).
   * LRO offload must be disabled.

TX Requirements for Vector Mode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Vector mode transmit will be enabled if the following constrainsts are met:
   * Packets must be contained within a single mbuf (no gather TX).
   * All transmit offloads other than VLAN insertion must be disabled.

BNXT PMD Supported Chipsets and Adapters
----------------------------------------

Chipsets and adapters supported by the bnxt PMD include:

  * **Broadcom BCM5730X NetXtreme-C® Family of Ethernet Network Controllers**

       * M150c - Single-port 40/50 Gigabit Ethernet Adapter
       * P150c - Single-port 40/50 Gigabit Ethernet Adapter
       * P225c - Dual-port 10/25 Gigabit Ethernet Adapter

  * **Broadcom BCM574XX/BCM575XX NetXtreme-E® Family of Ethernet Network Controllers**

       * M125P - Single-port OCP 2.0 10/25 Gigabit Ethernet Adapter
       * M150P - Single-port OCP 2.0 50 Gigabit Ethernet Adapter
       * M150PM - Single-port OCP 2.0 Multi-Host 50 Gigabit Ethernet Adapter
       * M210P - Dual-port OCP 2.0 10 Gigabit Ethernet Adapter
       * M210TP - Dual-port OCP 2.0 10 Gigabit Ethernet Adapter
       * M11000G - Single-port OCP 2.0 10/25/50/100 Gigabit Ethernet Adapter
       * N150G - Single-port OCP 3.0 50 Gigabit Ethernet Adapter
       * M225P - Dual-port OCP 2.0 10/25 Gigabit Ethernet Adapter
       * N210P - Dual-port OCP 3.0 10 Gigabit Ethernet Adapter
       * N210TP - Dual-port OCP 3.0 10 Gigabit Ethernet Adapter
       * N225P - Dual-port OCP 3.0 10/25 Gigabit Ethernet Adapter
       * N250G - Dual-port OCP 3.0 50 Gigabit Ethernet Adapter
       * N410SG - Quad-port OCP 3.0 10 Gigabit Ethernet Adapter
       * N410SGBT - Quad-port OCP 3.0 10 Gigabit Ethernet Adapter
       * N425G - Quad-port OCP 3.0 10/25 Gigabit Ethernet Adapter
       * N1100G - Single-port OCP 3.0 10/25/50/100 Gigabit Ethernet Adapter
       * N2100G - Dual-port OCP 3.0 10/25/50/100 Gigabit Ethernet Adapter
       * N2200G - Dual-port OCP 3.0 10/25/50/100/200 Gigabit Ethernet Adapter
       * P150P - Single-port 50 Gigabit Ethernet Adapter
       * P210P - Dual-port 10 Gigabit Ethernet Adapter
       * P210TP - Dual-port 10 Gigabit Ethernet Adapter
       * P225P - Dual-port 10/25 Gigabit Ethernet Adapter
       * P410SG - Quad-port 10 Gigabit Ethernet Adapter
       * P410SGBT - Quad-port 10 Gigabit Ethernet Adapter
       * P425G - Quad-port 10/25 Gigabit Ethernet Adapter
       * P1100G - Single-port 10/25/50/100 Gigabit Ethernet Adapter
       * P2100G - Dual-port 10/25/50/100 Gigabit Ethernet Adapter
       * P2200G - Dual-port 10/25/50/100/200 Gigabit Ethernet Adapter

    Information about Ethernet adapters in the NetXtreme family of
    adapters can be found in the `NetXtreme® Brand section
    <https://www.broadcom.com/products/ethernet-connectivity/network-adapters/>`_
    of the `Broadcom website <http://www.broadcom.com/>`_.

  * **Broadcom BCM588XX Stingray Family of SmartNIC Adapters**

       * PS410T - Quad-port 10 Gigabit Ethernet SmartNIC
       * PS225 - Dual-port 25 Gigabit Ethernet SmartNIC
       * PS250 - Dual-Port 50 Gigabit Ethernet SmartNIC

    Information about the Stingray family of SmartNIC adapters can be found in the
    `Stingray® Brand section
    <https://www.broadcom.com/products/ethernet-connectivity/smartnic/>`_
    of the `Broadcom website <http://www.broadcom.com/>`_.

  * **Broadcom StrataGX® BCM5871X Series of Communucations Processors**

    These ARM based processors target a broad range of networking applications
    including virtual CPE (vCPE) and NFV appliances, 10G service routers and
    gateways, control plane processing for Ethernet switches and network
    attached storage (NAS).

    Information about the StrataGX family of adapters can be found in the
    `StrataGX® BCM58712
    <http://www.broadcom.com/products/embedded-and-networking-processors/communications/bcm58712>`_
    and `StrataGX® BCM58713
    <http://www.broadcom.com/products/embedded-and-networking-processors/communications/bcm58713>`_
    sections of the `Broadcom website <http://www.broadcom.com/>`_.
