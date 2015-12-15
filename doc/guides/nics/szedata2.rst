..  BSD LICENSE
    Copyright 2015 CESNET
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
The SZEDATA2 PMD is virtual PMD which uses interface provided by libsze2
library to communicate with COMBO cards over sze2 layer.

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
   sharing of resources for user space applications:

Information about getting the dependencies can be found `here
<https://www.liberouter.org/technologies/netcope/access-to-libsze2-library/>`_.


Using the SZEDATA2 PMD
----------------------

SZEDATA2 PMD can be created by passing ``--vdev=`` option to EAL in the
following format:

.. code-block:: console

   --vdev 'DEVICE,dev_path=PATH,rx_ifaces=RX_MASK,tx_ifaces=TX_MASK'

``DEVICE`` and options ``dev_path``, ``rx_ifaces``, ``tx_ifaces`` are mandatory
and must be separated by commas.

*  ``DEVICE``: contains prefix ``eth_szedata2`` followed by numbers or letters,
   must be unique for each virtual device

*  ``dev_path``: Defines path to szedata2 device.
   Value is valid path to szedata2 device. Example:

   .. code-block:: console

      dev_path=/dev/szedataII0

*  ``rx_ifaces``: Defines which receive channels will be used.
   For each channel is created one queue. Value is mask for selecting which
   receive channels are required. Example:

   .. code-block:: console

      rx_ifaces=0x3

*  ``tx_ifaces``: Defines which transmit channels will be used.
   For each channel is created one queue. Value is mask for selecting which
   transmit channels are required. Example:

   .. code-block:: console

      tx_ifaces=0x3

Example of usage
----------------

Read packets from 0. and 1. receive channel and write them to 0. and 1.
transmit channel:

.. code-block:: console

   $RTE_TARGET/app/testpmd -c 0xf -n 2 \
   --vdev 'eth_szedata20,dev_path=/dev/szedataII0,rx_ifaces=0x3,tx_ifaces=0x3' \
   -- --port-topology=chained --rxq=2 --txq=2 --nb-cores=2
