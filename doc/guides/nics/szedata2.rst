..  BSD LICENSE
    Copyright 2015 - 2016 CESNET
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of CESNET nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

SZEDATA2 poll mode driver library
=================================

The SZEDATA2 poll mode driver library implements support for cards from COMBO
family (**COMBO-80G**, **COMBO-100G**).
The SZEDATA2 PMD uses interface provided by libsze2 library to communicate
with COMBO cards over sze2 layer.

More information about family of
`COMBO cards <https://www.liberouter.org/technologies/cards/>`_
and used technology
(`NetCOPE platform <https://www.liberouter.org/technologies/netcope/>`_) can be
found on the `Liberouter website <https://www.liberouter.org/>`_.

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

   * combov3
   * szedata2_cv3

   Kernel modules manage initialization of hardware, allocation and
   sharing of resources for user space applications.

Information about getting the dependencies can be found `here
<https://www.liberouter.org/technologies/netcope/access-to-libsze2-library/>`_.

Configuration
-------------

These configuration options can be modified before compilation in the
``.config`` file:

*  ``CONFIG_RTE_LIBRTE_PMD_SZEDATA2`` default value: **n**

   Value **y** enables compilation of szedata2 PMD.

*  ``CONFIG_RTE_LIBRTE_PMD_SZEDATA2_AS`` default value: **0**

   This option defines type of firmware address space.
   Currently supported value is:

   * **0** for firmwares:

      * NIC_100G1_LR4
      * HANIC_100G1_LR4
      * HANIC_100G1_SR10

Using the SZEDATA2 PMD
----------------------

From DPDK version 16.04 the type of SZEDATA2 PMD is changed to PMD_PDEV.
SZEDATA2 device is automatically recognized during EAL initialization.
No special command line options are needed.

Kernel modules have to be loaded before running the DPDK application.

Example of usage
----------------

Read packets from 0. and 1. receive channel and write them to 0. and 1.
transmit channel:

.. code-block:: console

   $RTE_TARGET/app/testpmd -c 0xf -n 2 \
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
