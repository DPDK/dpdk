..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 Cesnet
    Copyright 2019 Netcope Technologies

NFB poll mode driver library
=================================

The NFB poll mode driver library implements support for the Netcope
FPGA Boards (**NFB-40G2, NFB-100G2, NFB-200G2QL**) and Silicom **FB2CGG3** card,
FPGA-based programmable NICs. The NFB PMD uses interface provided by the libnfb
library to communicate with these cards over the nfb layer.

More information about the
`NFB cards <http://www.netcope.com/en/products/fpga-boards>`_
and used technology
(`Netcope Development Kit <http://www.netcope.com/en/products/fpga-development-kit>`_)
can be found on the `Netcope Technologies website <http://www.netcope.com/>`_.

.. note::

   Currently the driver is supported only on x86_64 architectures.
   Only x86_64 versions of the external libraries are provided.

Prerequisites
-------------

This PMD requires kernel modules which are responsible for initialization and
allocation of resources needed for nfb layer function.
Communication between PMD and kernel modules is mediated by libnfb library.
These kernel modules and library are not part of DPDK and must be installed
separately:

*  **libnfb library**

   The library provides API for initialization of nfb transfers, receiving and
   transmitting data segments.

*  **Kernel modules**

   * nfb

   Kernel modules manage initialization of hardware, allocation and
   sharing of resources for user space applications.

Dependencies can be found here:
`Netcope common <https://www.netcope.com/en/company/community-support/dpdk-libsze2#NFB>`_.

Versions of the packages
~~~~~~~~~~~~~~~~~~~~~~~~

The minimum version of the provided packages:

* for DPDK from 19.05

Configuration
-------------

Timestamps

The PMD supports hardware timestamps of frame receipt on physical network interface. In order to use
the timestamps, the hardware timestamping unit must be enabled (follow the documentation of the NFB
products) and the device argument `timestamp=1` must be used.

.. code-block:: console

    ./<build_dir>/app/dpdk-testpmd -a b3:00.0,timestamp=1 <other EAL params> -- <testpmd params>

When the timestamps are enabled with the *devarg*, a timestamp validity flag is set in the MBUFs
containing received frames and timestamp is inserted into the `rte_mbuf` struct.

The timestamp is an `uint64_t` field. Its lower 32 bits represent *seconds* portion of the timestamp
(number of seconds elapsed since 1.1.1970 00:00:00 UTC) and its higher 32 bits represent
*nanosecond* portion of the timestamp (number of nanoseconds elapsed since the beginning of the
second in the *seconds* portion.


Using the NFB PMD
----------------------

Kernel modules have to be loaded before running the DPDK application.

NFB card architecture
---------------------

The NFB cards are multi-port multi-queue cards, where (generally) data from any
Ethernet port may be sent to any queue.
They are represented in DPDK as a single port.

NFB-200G2QL card employs an add-on cable which allows to connect it to two
physical PCI-E slots at the same time (see the diagram below).
This is done to allow 200 Gbps of traffic to be transferred through the PCI-E
bus (note that a single PCI-E 3.0 x16 slot provides only 125 Gbps theoretical
throughput).

Although each slot may be connected to a different CPU and therefore to a different
NUMA node, the card is represented as a single port in DPDK. To work with data
from the individual queues on the right NUMA node, connection of NUMA nodes on
first and last queue (each NUMA node has half of the queues) need to be checked.

.. figure:: img/szedata2_nfb200g_architecture.*
    :align: center

    NFB-200G2QL high-level diagram

Limitations
-----------

Driver is usable only on Linux architecture, namely on CentOS.

Since a card is always represented as a single port, but can be connected to two
NUMA nodes, there is need for manual check where master/slave is connected.

Example of usage
----------------

Read packets from 0. and 1. receive queue and write them to 0. and 1.
transmit queue:

.. code-block:: console

   ./<build_dir>/app/dpdk-testpmd -l 0-3 -n 2 \
   -- --port-topology=chained --rxq=2 --txq=2 --nb-cores=2 -i -a

Example output:

.. code-block:: console

   [...]
   EAL: PCI device 0000:06:00.0 on NUMA socket -1
   EAL:   probe driver: 1b26:c1c1 net_nfb
   PMD: Initializing NFB device (0000:06:00.0)
   PMD: Available DMA queues RX: 8 TX: 8
   PMD: NFB device (0000:06:00.0) successfully initialized
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
