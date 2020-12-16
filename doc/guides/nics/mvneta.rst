..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Marvell International Ltd.
    Copyright(c) 2018 Semihalf.
    All rights reserved.

MVNETA Poll Mode Driver
=======================

The MVNETA PMD (**librte_net_mvneta**) provides poll mode driver support
for the Marvell NETA 1/2.5 Gbps adapter.

Detailed information about SoCs that use PPv2 can be obtained here:

* https://www.marvell.com/embedded-processors/armada-3700/


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

     git clone https://github.com/MarvellEmbeddedProcessors/musdk-marvell.git -b musdk-release-SDK-10.3.5.0-PR2

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


Runtime options
---------------

The following ``devargs`` options can be enabled at runtime. They must
be passed as part of EAL arguments.

- ``iface`` (mandatory, with no default value)

  The name of port (owned by MUSDK) that should be enabled in DPDK.
  This options can be repeated resulting in a list of ports to be
  enabled.  For instance below will enable ``eth0`` and ``eth1`` ports.

.. code-block:: console

   ./dpdk-testpmd --vdev=net_mvneta,iface=eth0,iface=eth1 \
    -c 3 -- -i --p 3 -a


Building MUSDK
--------------

Driver needs precompiled MUSDK library during compilation.

.. code-block:: console

   export CROSS_COMPILE=<toolchain>/bin/aarch64-linux-gnu-
   ./bootstrap
   ./configure --host=aarch64-linux-gnu --enable-pp2=no --enable-neta
   make install

MUSDK will be installed to `usr/local` under current directory.
For the detailed build instructions please consult ``doc/musdk_get_started.txt``.

Building DPDK
-------------

Add path to libmusdk.pc in PKG_CONFIG_PATH environment variable.

.. code-block:: console

   export PKG_CONFIG_PATH=$<musdk_install_dir>/lib/pkgconfig/:$PKG_CONFIG_PATH
   meson build --cross-file config/arm/arm64_armada_linux_gcc
   ninja -C build


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

   ./dpdk-testpmd --vdev=net_mvneta,iface=eth0,iface=eth1 -c 3 -- \
     -i --p 3 -a --txd 256 --rxd 128 --rxq=1 --txq=1  --nb-cores=1


In order to run l2fwd example application following command can be used:

.. code-block:: console

   ./dpdk-l2fwd --vdev=net_mvneta,iface=eth0,iface=eth1 -c 3 -- -T 1 -p 3
