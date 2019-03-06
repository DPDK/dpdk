..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Marvell International Ltd.
    Copyright(c) 2018 Semihalf.
    All rights reserved.

MVNETA Poll Mode Driver
=======================

The MVNETA PMD (librte_pmd_mvneta) provides poll mode driver support
for the Marvell NETA 1/2.5 Gbps adapter.

Detailed information about SoCs that use PPv2 can be obtained here:

* https://www.marvell.com/embedded-processors/armada-3700/

.. Note::

   Due to external dependencies, this driver is disabled by default. It must
   be enabled manually by setting relevant configuration option manually.
   Please refer to `Config File Options`_ section for further details.


Features
--------

Features of the MVNETA PMD are:

- Start/stop
- tx/rx_queue_setup
- tx/rx_burst
- Speed capabilities
- Jumbo frame
- MTU update
- Promiscuous mode
- Unicast MAC filter
- Link status
- CRC offload
- L3 checksum offload
- L4 checksum offload
- Packet type parsing
- Basic stats


Limitations
-----------

- Flushing vlans added for filtering is not possible due to MUSDK missing
  functionality. Current workaround is to reset board so that NETA has a
  chance to start in a sane state.

Prerequisites
-------------

- Custom Linux Kernel sources

  .. code-block:: console

     git clone https://github.com/MarvellEmbeddedProcessors/linux-marvell.git -b linux-4.4.120-armada-18.09


- MUSDK (Marvell User-Space SDK) sources

  .. code-block:: console

     git clone https://github.com/MarvellEmbeddedProcessors/musdk-marvell.git -b musdk-armada-18.09

  MUSDK is a light-weight library that provides direct access to Marvell's
  NETA. Alternatively prebuilt MUSDK library can be
  requested from `Marvell Extranet <https://extranet.marvell.com>`_. Once
  approval has been granted, library can be found by typing ``musdk`` in
  the search box.

  MUSDK must be configured with the following features:

  .. code-block:: console

     --enable-pp2=no --enable-neta

- DPDK environment

  Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup
  DPDK environment.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_MVNETA_PMD`` (default ``n``)

    Toggle compilation of the librte_pmd_mvneta driver.

Runtime options
~~~~~~~~~~~~~~~

The following ``devargs`` options can be enabled at runtime. They must
be passed as part of EAL arguments.

- ``iface`` (mandatory, with no default value)

  The name of port (owned by MUSDK) that should be enabled in DPDK.
  This options can be repeated resulting in a list of ports to be
  enabled.  For instance below will enable ``eth0`` and ``eth1`` ports.

.. code-block:: console

   ./testpmd --vdev=net_mvneta,iface=eth0,iface=eth1 \
    -c 3 -- -i --p 3 -a


Building DPDK
-------------

Driver needs precompiled MUSDK library during compilation.

.. code-block:: console

   export CROSS_COMPILE=<toolchain>/bin/aarch64-linux-gnu-
   ./bootstrap
   ./configure --host=aarch64-linux-gnu --enable-pp2=no --enable-neta
   make install

MUSDK will be installed to `usr/local` under current directory.
For the detailed build instructions please consult ``doc/musdk_get_started.txt``.

Before the DPDK build process the environmental variable ``LIBMUSDK_PATH`` with
the path to the MUSDK installation directory needs to be exported.

.. code-block:: console

   export LIBMUSDK_PATH=<musdk>/usr/local
   export CROSS=aarch64-linux-gnu-
   make config T=arm64-armv8a-linux-gcc
   sed -ri 's,(MVNETA_PMD=)n,\1y,' build/.config
   make

Usage Example
-------------

MVNETA PMD requires extra out of tree kernel modules to function properly.
`musdk_uio` and `mv_neta_uio` sources are part of the MUSDK. Please consult
``doc/musdk_get_started.txt`` for the detailed build instructions.

.. code-block:: console

   insmod musdk_uio.ko
   insmod mv_neta_uio.ko

Additionally interfaces used by DPDK application need to be put up:

.. code-block:: console

   ip link set eth0 up
   ip link set eth1 up

In order to run testpmd example application following command can be used:

.. code-block:: console

   ./testpmd --vdev=net_mvneta,iface=eth0,iface=eth1 -c 3 -- \
     -i --p 3 -a --txd 256 --rxd 128 --rxq=1 --txq=1  --nb-cores=1


In order to run l2fwd example application following command can be used:

.. code-block:: console

   ./l2fwd --vdev=net_mvneta,iface=eth0,iface=eth1 -c 3 -- -T 1 -p 3
