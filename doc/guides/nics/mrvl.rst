..  BSD LICENSE
    Copyright(c) 2017 Marvell International Ltd.
    Copyright(c) 2017 Semihalf.
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
      * Neither the name of the copyright holder nor the names of its
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

.. _mrvl_poll_mode_driver:

MRVL Poll Mode Driver
======================

The MRVL PMD (librte_pmd_mrvl) provides poll mode driver support
for the Marvell PPv2 (Packet Processor v2) 1/10 Gbps adapter.

Detailed information about SoCs that use PPv2 can be obtained here:

* https://www.marvell.com/embedded-processors/armada-70xx/
* https://www.marvell.com/embedded-processors/armada-80xx/

.. Note::

   Due to external dependencies, this driver is disabled by default. It must
   be enabled manually by setting relevant configuration option manually.
   Please refer to `Config File Options`_ section for further details.


Features
--------

Features of the MRVL PMD are:

- Speed capabilities
- Link status
- Queue start/stop
- MTU update
- Jumbo frame
- Promiscuous mode
- Allmulticast mode
- Unicast MAC filter
- Multicast MAC filter
- RSS hash
- VLAN filter
- CRC offload
- L3 checksum offload
- L4 checksum offload
- Packet type parsing
- Basic stats
- QoS


Limitations
-----------

- Number of lcores is limited to 9 by MUSDK internal design. If more lcores
  need to be allocated, locking will have to be considered. Number of available
  lcores can be changed via ``MRVL_MUSDK_HIFS_RESERVED`` define in
  ``mrvl_ethdev.c`` source file.

- Flushing vlans added for filtering is not possible due to MUSDK missing
  functionality. Current workaround is to reset board so that PPv2 has a
  chance to start in a sane state.


Prerequisites
-------------

- Custom Linux Kernel sources

  .. code-block:: console

     git clone https://github.com/MarvellEmbeddedProcessors/linux-marvell.git -b linux-4.4.52-armada-17.10

- Out of tree `mvpp2x_sysfs` kernel module sources

  .. code-block:: console

     git clone https://github.com/MarvellEmbeddedProcessors/mvpp2x-marvell.git -b mvpp2x-armada-17.10

- MUSDK (Marvell User-Space SDK) sources

  .. code-block:: console

     git clone https://github.com/MarvellEmbeddedProcessors/musdk-marvell.git -b musdk-armada-17.10

  MUSDK is a light-weight library that provides direct access to Marvell's
  PPv2 (Packet Processor v2). Alternatively prebuilt MUSDK library can be
  requested from `Marvell Extranet <https://extranet.marvell.com>`_. Once
  approval has been granted, library can be found by typing ``musdk`` in
  the search box.

  MUSDK must be configured with the following features:

  .. code-block:: console

     --enable-bpool-dma=64

- DPDK environment

  Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup
  DPDK environment.


Config File Options
-------------------

The following options can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_MRVL_PMD`` (default ``n``)

    Toggle compilation of the librte_pmd_mrvl driver.


QoS Configuration
-----------------

QoS configuration is done through external configuration file. Path to the
file must be given as `cfg` in driver's vdev parameter list.

Configuration syntax
~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   [port <portnum> default]
   default_tc = <default_tc>
   mapping_priority = <mapping_priority>

   [port <portnum> tc <traffic_class>]
   rxq = <rx_queue_list>
   pcp = <pcp_list>
   dscp = <dscp_list>

   [port <portnum> tc <traffic_class>]
   rxq = <rx_queue_list>
   pcp = <pcp_list>
   dscp = <dscp_list>

Where:

- ``<portnum>``: DPDK Port number (0..n).

- ``<default_tc>``: Default traffic class (e.g. 0)

- ``<mapping_priority>``: QoS priority for mapping (`ip`, `vlan`, `ip/vlan` or `vlan/ip`).

- ``<traffic_class>``: Traffic Class to be configured.

- ``<rx_queue_list>``: List of DPDK RX queues (e.g. 0 1 3-4)

- ``<pcp_list>``: List of PCP values to handle in particular TC (e.g. 0 1 3-4 7).

- ``<dscp_list>``: List of DSCP values to handle in particular TC (e.g. 0-12 32-48 63).

Setting PCP/DSCP values for the default TC is not required. All PCP/DSCP
values not assigned explicitly to particular TC will be handled by the
default TC.

Configuration file example
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

   [port 0 default]
   default_tc = 0
   qos_mode = ip

   [port 0 tc 0]
   rxq = 0 1

   [port 0 tc 1]
   rxq = 2
   pcp = 5 6 7
   dscp = 26-38

   [port 1 default]
   default_tc = 0
   qos_mode = vlan/ip

   [port 1 tc 0]
   rxq = 0

   [port 1 tc 1]
   rxq = 1 2
   pcp = 5 6 7
   dscp = 26-38

Usage example
^^^^^^^^^^^^^

.. code-block:: console

   ./testpmd --vdev=eth_mrvl,iface=eth0,iface=eth2,cfg=/home/user/mrvl.conf \
     -c 7 -- -i -a --rxq=2


Building DPDK
-------------

Driver needs precompiled MUSDK library during compilation.

.. code-block:: console

   export CROSS_COMPILE=<toolchain>/bin/aarch64-linux-gnu-
   ./bootstrap
   ./configure --host=aarch64-linux-gnu --enable-bpool-dma=64
   make install

MUSDK will be installed to `usr/local` under current directory.
For the detailed build instructions please consult ``doc/musdk_get_started.txt``.

Before the DPDK build process the environmental variable ``LIBMUSDK_PATH`` with
the path to the MUSDK installation directory needs to be exported.

.. code-block:: console

   export LIBMUSDK_PATH=<musdk>/usr/local
   export CROSS=aarch64-linux-gnu-
   make config T=arm64-armv8a-linuxapp-gcc
   sed -ri 's,(MRVL_PMD=)n,\1y,' build/.config
   make

Usage Example
-------------

MRVL PMD requires extra out of tree kernel modules to function properly.
`musdk_uio` and `mv_pp_uio` sources are part of the MUSDK. Please consult
``doc/musdk_get_started.txt`` for the detailed build instructions.
For `mvpp2x_sysfs` please consult ``Documentation/pp22_sysfs.txt`` for the
detailed build instructions.

.. code-block:: console

   insmod musdk_uio.ko
   insmod mv_pp_uio.ko
   insmod mvpp2x_sysfs.ko

Additionally interfaces used by DPDK application need to be put up:

.. code-block:: console

   ip link set eth0 up
   ip link set eth2 up

In order to run testpmd example application following command can be used:

.. code-block:: console

   ./testpmd --vdev=eth_mrvl,iface=eth0,iface=eth2 -c 7 -- \
     --burst=128 --txd=2048 --rxd=1024 --rxq=2 --txq=2  --nb-cores=2 \
     -i -a --rss-udp
