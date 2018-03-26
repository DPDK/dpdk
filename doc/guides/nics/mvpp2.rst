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

.. _mvpp2_poll_mode_driver:

MVPP2 Poll Mode Driver
======================

The MVPP2 PMD (librte_pmd_mvpp2) provides poll mode driver support
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

Features of the MVPP2 PMD are:

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
- Extended stats
- QoS
- RX flow control
- TX queue start/stop


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

  To get better understanding of the library one can consult documentation
  available in the ``doc`` top level directory of the MUSDK sources.

  MUSDK must be configured with the following features:

  .. code-block:: console

     --enable-bpool-dma=64

- DPDK environment

  Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup
  DPDK environment.


Config File Options
-------------------

The following options can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_MVPP2_PMD`` (default ``n``)

    Toggle compilation of the librte mvpp2 driver.


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
   policer_enable = <policer_enable>
   token_unit = <token_unit>
   color = <color_mode>
   cir = <cir>
   ebs = <ebs>
   cbs = <cbs>

   rate_limit_enable = <rate_limit_enable>
   rate_limit = <rate_limit>
   burst_size = <burst_size>

   [port <portnum> tc <traffic_class>]
   rxq = <rx_queue_list>
   pcp = <pcp_list>
   dscp = <dscp_list>
   default_color = <default_color>

   [port <portnum> tc <traffic_class>]
   rxq = <rx_queue_list>
   pcp = <pcp_list>
   dscp = <dscp_list>

   [port <portnum> txq <txqnum>]
   sched_mode = <sched_mode>
   wrr_weight = <wrr_weight>

   rate_limit_enable = <rate_limit_enable>
   rate_limit = <rate_limit>
   burst_size = <burst_size>

Where:

- ``<portnum>``: DPDK Port number (0..n).

- ``<default_tc>``: Default traffic class (e.g. 0)

- ``<mapping_priority>``: QoS priority for mapping (`ip`, `vlan`, `ip/vlan` or `vlan/ip`).

- ``<traffic_class>``: Traffic Class to be configured.

- ``<rx_queue_list>``: List of DPDK RX queues (e.g. 0 1 3-4)

- ``<pcp_list>``: List of PCP values to handle in particular TC (e.g. 0 1 3-4 7).

- ``<dscp_list>``: List of DSCP values to handle in particular TC (e.g. 0-12 32-48 63).

- ``<policer_enable>``: Enable ingress policer.

- ``<token_unit>``: Policer token unit (`bytes` or `packets`).

- ``<color_mode>``: Policer color mode (`aware` or `blind`).

- ``<cir>``: Committed information rate in unit of kilo bits per second (data rate) or packets per second.

- ``<cbs>``: Committed burst size in unit of kilo bytes or number of packets.

- ``<ebs>``: Excess burst size in unit of kilo bytes or number of packets.

- ``<default_color>``: Default color for specific tc.

- ``<rate_limit_enable>``: Enables per port or per txq rate limiting.

- ``<rate_limit>``: Committed information rate, in kilo bits per second.

- ``<burst_size>``: Committed burst size, in kilo bytes.

- ``<sched_mode>``: Egress scheduler mode (`wrr` or `sp`).

- ``<wrr_weight>``: Txq weight.

Setting PCP/DSCP values for the default TC is not required. All PCP/DSCP
values not assigned explicitly to particular TC will be handled by the
default TC.

Configuration file example
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: console

   [port 0 default]
   default_tc = 0
   mapping_priority = ip

   rate_limit_enable = 1
   rate_limit = 1000
   burst_size = 2000

   [port 0 tc 0]
   rxq = 0 1

   [port 0 txq 0]
   sched_mode = wrr
   wrr_weight = 10

   [port 0 txq 1]
   sched_mode = wrr
   wrr_weight = 100

   [port 0 txq 2]
   sched_mode = sp

   [port 0 tc 1]
   rxq = 2
   pcp = 5 6 7
   dscp = 26-38

   [port 1 default]
   default_tc = 0
   mapping_priority = vlan/ip

   policer_enable = 1
   token_unit = bytes
   color = blind
   cir = 100000
   ebs = 64
   cbs = 64

   [port 1 tc 0]
   rxq = 0
   dscp = 10

   [port 1 tc 1]
   rxq = 1
   dscp = 11-20

   [port 1 tc 2]
   rxq = 2
   dscp = 30

   [port 1 txq 0]
   rate_limit_enable = 1
   rate_limit = 10000
   burst_size = 2000

Usage example
^^^^^^^^^^^^^

.. code-block:: console

   ./testpmd --vdev=eth_mvpp2,iface=eth0,iface=eth2,cfg=/home/user/mrvl.conf \
     -c 7 -- -i -a --disable-hw-vlan-strip --rxq=3 --txq=3


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
   sed -ri 's,(MVPP2_PMD=)n,\1y,' build/.config
   make

Flow API
--------

PPv2 offers packet classification capabilities via classifier engine which
can be configured via generic flow API offered by DPDK.

Supported flow actions
~~~~~~~~~~~~~~~~~~~~~~

Following flow action items are supported by the driver:

* DROP
* QUEUE

Supported flow items
~~~~~~~~~~~~~~~~~~~~

Following flow items and their respective fields are supported by the driver:

* ETH

  * source MAC
  * destination MAC
  * ethertype

* VLAN

  * PCP
  * VID

* IPV4

  * DSCP
  * protocol
  * source address
  * destination address

* IPV6

  * flow label
  * next header
  * source address
  * destination address

* UDP

  * source port
  * destination port

* TCP

  * source port
  * destination port

Classifier match engine
~~~~~~~~~~~~~~~~~~~~~~~

Classifier has an internal match engine which can be configured to
operate in either exact or maskable mode.

Mode is selected upon creation of the first unique flow rule as follows:

* maskable, if key size is up to 8 bytes.
* exact, otherwise, i.e for keys bigger than 8 bytes.

Where the key size equals the number of bytes of all fields specified
in the flow items.

.. table:: Examples of key size calculation

   +----------------------------------------------------------------------------+-------------------+-------------+
   | Flow pattern                                                               | Key size in bytes | Used engine |
   +============================================================================+===================+=============+
   | ETH (destination MAC) / VLAN (VID)                                         | 6 + 2 = 8         | Maskable    |
   +----------------------------------------------------------------------------+-------------------+-------------+
   | VLAN (VID) / IPV4 (source address)                                         | 2 + 4 = 6         | Maskable    |
   +----------------------------------------------------------------------------+-------------------+-------------+
   | TCP (source port, destination port)                                        | 2 + 2 = 4         | Maskable    |
   +----------------------------------------------------------------------------+-------------------+-------------+
   | VLAN (priority) / IPV4 (source address)                                    | 1 + 4 = 5         | Maskable    |
   +----------------------------------------------------------------------------+-------------------+-------------+
   | IPV4 (destination address) / UDP (source port, destination port)           | 6 + 2 + 2 = 10    | Exact       |
   +----------------------------------------------------------------------------+-------------------+-------------+
   | VLAN (VID) / IPV6 (flow label, destination address)                        | 2 + 3 + 16 = 21   | Exact       |
   +----------------------------------------------------------------------------+-------------------+-------------+
   | IPV4 (DSCP, source address, destination address)                           | 1 + 4 + 4 = 9     | Exact       |
   +----------------------------------------------------------------------------+-------------------+-------------+
   | IPV6 (flow label, source address, destination address)                     | 3 + 16 + 16 = 35  | Exact       |
   +----------------------------------------------------------------------------+-------------------+-------------+

From the user perspective maskable mode means that masks specified
via flow rules are respected. In case of exact match mode, masks
which do not provide exact matching (all bits masked) are ignored.

If the flow matches more than one classifier rule the first
(with the lowest index) matched takes precedence.

Flow rules usage example
~~~~~~~~~~~~~~~~~~~~~~~~

Before proceeding run testpmd user application:

.. code-block:: console

   ./testpmd --vdev=eth_mvpp2,iface=eth0,iface=eth2 -c 3 -- -i --p 3 -a --disable-hw-vlan-strip

Example #1
^^^^^^^^^^

.. code-block:: console

   testpmd> flow create 0 ingress pattern eth src is 10:11:12:13:14:15 / end actions drop / end

In this case key size is 6 bytes thus maskable type is selected. Testpmd
will set mask to ff:ff:ff:ff:ff:ff i.e traffic explicitly matching
above rule will be dropped.

Example #2
^^^^^^^^^^

.. code-block:: console

   testpmd> flow create 0 ingress pattern ipv4 src spec 10.10.10.0 src mask 255.255.255.0 / tcp src spec 0x10 src mask 0x10 / end action drop / end

In this case key size is 8 bytes thus maskable type is selected.
Flows which have IPv4 source addresses ranging from 10.10.10.0 to 10.10.10.255
and tcp source port set to 16 will be dropped.

Example #3
^^^^^^^^^^

.. code-block:: console

   testpmd> flow create 0 ingress pattern vlan vid spec 0x10 vid mask 0x10 / ipv4 src spec 10.10.1.1 src mask 255.255.0.0 dst spec 11.11.11.1 dst mask 255.255.255.0 / end actions drop / end

In this case key size is 10 bytes thus exact type is selected.
Even though each item has partial mask set, masks will be ignored.
As a result only flows with VID set to 16 and IPv4 source and destination
addresses set to 10.10.1.1 and 11.11.11.1 respectively will be dropped.

Limitations
~~~~~~~~~~~

Following limitations need to be taken into account while creating flow rules:

* For IPv4 exact match type the key size must be up to 12 bytes.
* For IPv6 exact match type the key size must be up to 36 bytes.
* Following fields cannot be partially masked (all masks are treated as
  if they were exact):

  * ETH: ethertype
  * VLAN: PCP, VID
  * IPv4: protocol
  * IPv6: next header
  * TCP/UDP: source port, destination port

* Only one classifier table can be created thus all rules in the table
  have to match table format. Table format is set during creation of
  the first unique flow rule.
* Up to 5 fields can be specified per flow rule.
* Up to 20 flow rules can be added.

For additional information about classifier please consult
``doc/musdk_cls_user_guide.txt``.

Usage Example
-------------

MVPP2 PMD requires extra out of tree kernel modules to function properly.
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

   ./testpmd --vdev=eth_mvpp2,iface=eth0,iface=eth2 -c 7 -- \
     --burst=128 --txd=2048 --rxd=1024 --rxq=2 --txq=2  --nb-cores=2 \
     -i -a --rss-udp
