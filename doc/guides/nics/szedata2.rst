..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2015 - 2016 CESNET

SZEDATA2 poll mode driver library
=================================

The SZEDATA2 poll mode driver library implements support for the Netcope
FPGA Boards (**NFB-***), FPGA-based programmable NICs.
The SZEDATA2 PMD uses interface provided by the libsze2 library to communicate
with the NFB cards over the sze2 layer.

More information about the
`NFB cards <http://www.netcope.com/en/products/fpga-boards>`_
and used technology
(`Netcope Development Kit <http://www.netcope.com/en/products/fpga-development-kit>`_)
can be found on the `Netcope Technologies website <http://www.netcope.com/>`_.

.. note::

   This driver has external dependencies.
   Therefore it is disabled in default configuration files.
   It can be enabled by setting ``CONFIG_RTE_LIBRTE_PMD_SZEDATA2=y``
   and recompiling.

.. note::

   Currently the driver is supported only on x86_64 architectures.
   Only x86_64 versions of the external libraries are provided.

Prerequisites
-------------

This PMD requires kernel modules which are responsible for initialization and
allocation of resources needed for sze2 layer function.
Communication between PMD and kernel modules is mediated by libsze2 library.
These kernel modules and library are not part of DPDK and must be installed
separately:

*  **libsze2 library**

   The library provides API for initialization of sze2 transfers, receiving and
   transmitting data segments.

*  **Kernel modules**

   * combo6core
   * combov3
   * szedata2
   * szedata2_cv3 or szedata2_cv3_fdt

   Kernel modules manage initialization of hardware, allocation and
   sharing of resources for user space applications.

Information about getting the dependencies can be found `here
<http://www.netcope.com/en/company/community-support/dpdk-libsze2>`_.

Versions of the packages
~~~~~~~~~~~~~~~~~~~~~~~~

The minimum version of the provided packages:

* for DPDK from 18.05: **4.4.1**

* for DPDK up to 18.02 (including): **3.0.5**

Configuration
-------------

These configuration options can be modified before compilation in the
``.config`` file:

*  ``CONFIG_RTE_LIBRTE_PMD_SZEDATA2`` default value: **n**

   Value **y** enables compilation of szedata2 PMD.

Using the SZEDATA2 PMD
----------------------

From DPDK version 16.04 the type of SZEDATA2 PMD is changed to PMD_PDEV.
SZEDATA2 device is automatically recognized during EAL initialization.
No special command line options are needed.

Kernel modules have to be loaded before running the DPDK application.

NFB card architecture
---------------------

The NFB cards are multi-port multi-queue cards, where (generally) data from any
Ethernet port may be sent to any queue.
They were historically represented in DPDK as a single port.

However, the new NFB-200G2QL card employs an add-on cable which allows to connect
it to two physical PCI-E slots at the same time (see the diagram below).
This is done to allow 200 Gbps of traffic to be transferred through the PCI-E
bus (note that a single PCI-E 3.0 x16 slot provides only 125 Gbps theoretical
throughput).

Since each slot may be connected to a different CPU and therefore to a different
NUMA node, the card is represented as two ports in DPDK (each with half of the
queues), which allows DPDK to work with data from the individual queues on the
right NUMA node.

.. figure:: img/szedata2_nfb200g_architecture.*
    :align: center

    NFB-200G2QL high-level diagram

Limitations
-----------

The SZEDATA2 PMD does not support operations related to Ethernet ports
(link_up, link_down, set_mac_address, etc.).

NFB cards employ multiple Ethernet ports.
Until now, Ethernet port-related operations were performed on all of them
(since the whole card was represented as a single port).
With NFB-200G2QL card, this is no longer viable (see above).

Since there is no fixed mapping between the queues and Ethernet ports, and since
a single card can be represented as two ports in DPDK, there is no way of
telling which (if any) physical ports should be associated with individual
ports in DPDK.

Example of usage
----------------

Read packets from 0. and 1. receive channel and write them to 0. and 1.
transmit channel:

.. code-block:: console

   $RTE_TARGET/app/testpmd -l 0-3 -n 2 \
   -- --port-topology=chained --rxq=2 --txq=2 --nb-cores=2 -i -a

Example output:

.. code-block:: console

   [...]
   EAL: PCI device 0000:06:00.0 on NUMA socket -1
   EAL:   probe driver: 1b26:c1c1 rte_szedata2_pmd
   PMD: Initializing szedata2 device (0000:06:00.0)
   PMD: SZEDATA2 path: /dev/szedataII0
   PMD: Available DMA channels RX: 8 TX: 8
   PMD: resource0 phys_addr = 0xe8000000 len = 134217728 virt addr = 7f48f8000000
   PMD: szedata2 device (0000:06:00.0) successfully initialized
   Interactive-mode selected
   Auto-start selected
   Configuring Port 0 (socket 0)
   Port 0: 00:11:17:00:00:00
   Checking link statuses...
   Port 0 Link Up - speed 10000 Mbps - full-duplex
   Done
   Start automatic packet forwarding
     io packet forwarding - CRC stripping disabled - packets/burst=32
     nb forwarding cores=2 - nb forwarding ports=1
     RX queues=2 - RX desc=128 - RX free threshold=0
     RX threshold registers: pthresh=0 hthresh=0 wthresh=0
     TX queues=2 - TX desc=512 - TX free threshold=0
     TX threshold registers: pthresh=0 hthresh=0 wthresh=0
     TX RS bit threshold=0 - TXQ flags=0x0
   testpmd>
