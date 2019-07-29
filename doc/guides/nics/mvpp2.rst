..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Marvell International Ltd.
    Copyright(c) 2017 Semihalf.

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
- Tx Queue start/stop
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
- :ref:`Extended stats <extstats>`
- RX flow control
- Scattered TX frames
- :ref:`QoS <qossupport>`
- :ref:`Flow API <flowapi>`
- :ref:`Traffic metering and policing <mtrapi>`
- :ref:`Traffic Management API <tmapi>`

Limitations
-----------

- Number of lcores is limited to 9 by MUSDK internal design. If more lcores
  need to be allocated, locking will have to be considered. Number of available
  lcores can be changed via ``MRVL_MUSDK_HIFS_RESERVED`` define in
  ``mrvl_ethdev.c`` source file.

- Flushing vlans added for filtering is not possible due to MUSDK missing
  functionality. Current workaround is to reset board so that PPv2 has a
  chance to start in a sane state.

- MUSDK architecture does not support changing configuration in run time.
  All necessary configurations should be done before first dev_start().

- RX queue start/stop is not supported.

- Current implementation does not support replacement of buffers in the HW buffer pool
  at run time, so it is responsibility of the application to ensure that MTU does not exceed the configured buffer size.

- Configuring TX flow control currently is not supported.

- In current implementation, mechanism for acknowledging transmitted packets (``tx_done_cleanup``) is not supported.

- Running more than one DPDK-MUSDK application simultaneously is not supported.


Prerequisites
-------------

- Custom Linux Kernel sources

  .. code-block:: console

     git clone https://github.com/MarvellEmbeddedProcessors/linux-marvell.git -b linux-4.4.120-armada-18.09

- Out of tree `mvpp2x_sysfs` kernel module sources

  .. code-block:: console

     git clone https://github.com/MarvellEmbeddedProcessors/mvpp2x-marvell.git -b mvpp2x-armada-18.09

- MUSDK (Marvell User-Space SDK) sources

  .. code-block:: console

     git clone https://github.com/MarvellEmbeddedProcessors/musdk-marvell.git -b musdk-armada-18.09

  MUSDK is a light-weight library that provides direct access to Marvell's
  PPv2 (Packet Processor v2). Alternatively prebuilt MUSDK library can be
  requested from `Marvell Extranet <https://extranet.marvell.com>`_. Once
  approval has been granted, library can be found by typing ``musdk`` in
  the search box.

  To get better understanding of the library one can consult documentation
  available in the ``doc`` top level directory of the MUSDK sources.

- DPDK environment

  Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup
  DPDK environment.


Config File Options
-------------------

The following options can be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_MVPP2_PMD`` (default ``n``)

    Toggle compilation of the librte mvpp2 driver.

    .. Note::

       When MVPP2 PMD is enabled ``CONFIG_RTE_LIBRTE_MVNETA_PMD`` must be disabled


Building DPDK
-------------

Driver needs precompiled MUSDK library during compilation.

.. code-block:: console

   export CROSS_COMPILE=<toolchain>/bin/aarch64-linux-gnu-
   ./bootstrap
   ./configure --host=aarch64-linux-gnu
   make install

MUSDK will be installed to `usr/local` under current directory.
For the detailed build instructions please consult ``doc/musdk_get_started.txt``.

Before the DPDK build process the environmental variable ``LIBMUSDK_PATH`` with
the path to the MUSDK installation directory needs to be exported.

For additional instructions regarding DPDK cross compilation please refer to :doc:`Cross compile DPDK for ARM64 <../linux_gsg/cross_build_dpdk_for_arm64>`.

.. code-block:: console

   export LIBMUSDK_PATH=<musdk>/usr/local
   export CROSS=<toolchain>/bin/aarch64-linux-gnu-
   export RTE_KERNELDIR=<kernel-dir>
   export RTE_TARGET=arm64-armv8a-linux-gcc

   make config T=arm64-armv8a-linux-gcc
   sed -i "s/MVNETA_PMD=y/MVNETA_PMD=n/" build/.config
   sed -i "s/MVPP2_PMD=n/MVPP2_PMD=y/" build/.config
   make

Usage Example
-------------

MVPP2 PMD requires extra out of tree kernel modules to function properly.
`musdk_cma` sources are part of the MUSDK. Please consult
``doc/musdk_get_started.txt`` for the detailed build instructions.
For `mvpp2x_sysfs` please consult ``Documentation/pp22_sysfs.txt`` for the
detailed build instructions.

.. code-block:: console

   insmod musdk_cma.ko
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

.. _extstats:

Extended stats
--------------

MVPP2 PMD supports the following extended statistics:

	- ``rx_bytes``:	number of RX bytes
	- ``rx_packets``: number of RX packets
	- ``rx_unicast_packets``: number of RX unicast packets
	- ``rx_errors``: number of RX MAC errors
	- ``rx_fullq_dropped``: number of RX packets dropped due to full RX queue
	- ``rx_bm_dropped``: number of RX packets dropped due to no available buffers in the HW pool
	- ``rx_early_dropped``: number of RX packets that were early dropped
	- ``rx_fifo_dropped``: number of RX packets dropped due to RX fifo overrun
	- ``rx_cls_dropped``: number of RX packets dropped by classifier
	- ``tx_bytes``: number of TX bytes
	- ``tx_packets``: number of TX packets
	- ``tx_unicast_packets``: number of TX unicast packets
	- ``tx_errors``: number of TX MAC errors


.. _qossupport:

QoS Configuration
-----------------

QoS configuration is done through external configuration file. Path to the
file must be given as `cfg` in driver's vdev parameter list.

Configuration syntax
~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   [policer <policer_id>]
   token_unit = <token_unit>
   color = <color_mode>
   cir = <cir>
   ebs = <ebs>
   cbs = <cbs>

   [port <portnum> default]
   default_tc = <default_tc>
   mapping_priority = <mapping_priority>

   rate_limit_enable = <rate_limit_enable>
   rate_limit = <rate_limit>
   burst_size = <burst_size>

   default_policer = <policer_id>

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

- ``<default_policer>``: Id of the policer configuration section to be used as default.

- ``<policer_id>``: Id of the policer configuration section (0..31).

- ``<token_unit>``: Policer token unit (`bytes` or `packets`).

- ``<color_mode>``: Policer color mode (`aware` or `blind`).

- ``<cir>``: Committed information rate in unit of kilo bits per second (data rate) or packets per second.

- ``<cbs>``: Committed burst size in unit of kilo bytes or number of packets.

- ``<ebs>``: Excess burst size in unit of kilo bytes or number of packets.

- ``<default_color>``: Default color for specific tc.

- ``<rate_limit_enable>``: Enables per port or per txq rate limiting (`0`/`1` to disable/enable).

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

   [policer 0]
   token_unit = bytes
   color = blind
   cir = 100000
   ebs = 64
   cbs = 64

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

   default_policer = 0

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

.. _flowapi:

Flow API
--------

PPv2 offers packet classification capabilities via classifier engine which
can be configured via generic flow API offered by DPDK.

The :ref:`flow_isolated_mode` is supported.

For an additional description please refer to DPDK :doc:`../prog_guide/rte_flow`.

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

.. _mtrapi:

Traffic metering and policing
-----------------------------

MVPP2 PMD supports DPDK traffic metering and policing that allows the following:

1. Meter ingress traffic.
2. Do policing.
3. Gather statistics.

For an additional description please refer to DPDK :doc:`Traffic Metering and Policing API <../prog_guide/traffic_metering_and_policing>`.

The policer objects defined by this feature can work with the default policer defined via config file as described in :ref:`QoS Support <qossupport>`.

Limitations
~~~~~~~~~~~

The following capabilities are not supported:

- MTR object meter DSCP table update
- MTR object policer action update
- MTR object enabled statistics

Usage example
~~~~~~~~~~~~~

1. Run testpmd user app:

   .. code-block:: console

		./testpmd --vdev=eth_mvpp2,iface=eth0,iface=eth2 -c 6 -- -i -p 3 -a --txd 1024 --rxd 1024

2. Create meter profile:

   .. code-block:: console

		testpmd> add port meter profile 0 0 srtcm_rfc2697 2000 256 256

3. Create meter:

   .. code-block:: console

		testpmd> create port meter 0 0 0 yes d d d 0 1 0

4. Create flow rule witch meter attached:

   .. code-block:: console

		testpmd> flow create 0 ingress pattern ipv4 src is 10.10.10.1 / end actions meter mtr_id 0 / end

For a detailed usage description please refer to "Traffic Metering and Policing" section in DPDK :doc:`Testpmd Runtime Functions <../testpmd_app_ug/testpmd_funcs>`.



.. _tmapi:

Traffic Management API
----------------------

MVPP2 PMD supports generic DPDK Traffic Management API which allows to
configure the following features:

1. Hierarchical scheduling
2. Traffic shaping
3. Congestion management
4. Packet marking

Internally TM is represented by a hierarchy (tree) of nodes.
Node which has a parent is called a leaf whereas node without
parent is called a non-leaf (root).
MVPP2 PMD supports two level hierarchy where level 0 represents ports and level 1 represents tx queues of a given port.

.. figure:: img/mvpp2_tm.*

Nodes hold following types of settings:

- for egress scheduler configuration: weight
- for egress rate limiter: private shaper
- bitmask indicating which statistics counters will be read

Hierarchy is always constructed from the top, i.e first a root node is added
then some number of leaf nodes. Number of leaf nodes cannot exceed number
of configured tx queues.

After hierarchy is complete it can be committed.


For an additional description please refer to DPDK :doc:`Traffic Management API <../prog_guide/traffic_management>`.

Limitations
~~~~~~~~~~~

The following capabilities are not supported:

- Traffic manager WRED profile and WRED context
- Traffic manager shared shaper update
- Traffic manager packet marking
- Maximum number of levels in hierarchy is 2
- Currently dynamic change of a hierarchy is not supported

Usage example
~~~~~~~~~~~~~

For a detailed usage description please refer to "Traffic Management" section in DPDK :doc:`Testpmd Runtime Functions <../testpmd_app_ug/testpmd_funcs>`.

1. Run testpmd as follows:

   .. code-block:: console

		./testpmd --vdev=net_mrvl,iface=eth0,iface=eth2,cfg=./qos_config -c 7 -- \
		-i -p 3 --disable-hw-vlan-strip --rxq 3 --txq 3 --txd 1024 --rxd 1024

2. Stop all ports:

   .. code-block:: console

		testpmd> port stop all

3. Add shaper profile:

   .. code-block:: console

		testpmd> add port tm node shaper profile 0 0 900000 70000 0

   Parameters have following meaning::

		0       - Id of a port.
		0       - Id of a new shaper profile.
		900000  - Shaper rate in bytes/s.
		70000   - Bucket size in bytes.
		0       - Packet length adjustment - ignored.

4. Add non-leaf node for port 0:

   .. code-block:: console

		testpmd> add port tm nonleaf node 0 3 -1 0 0 0 0 0 1 3 0

   Parameters have following meaning::

		 0  - Id of a port
		 3  - Id of a new node.
		-1  - Indicate that root does not have a parent.
		 0  - Priority of the node.
		 0  - Weight of the node.
		 0  - Id of a level. Since this is a root 0 is passed.
		 0  - Id of the shaper profile.
		 0  - Number of SP priorities.
		 3  - Enable statistics for both number of transmitted packets and bytes.
		 0  - Number of shared shapers.

5. Add leaf node for tx queue 0:

   .. code-block:: console

		testpmd> add port tm leaf node 0 0 3 0 30 1 -1 0 0 1 0

   Parameters have following meaning::

		 0  - Id of a port.
		 0  - Id of a new node.
		 3  - Id of the parent node.
		 0  - Priority of a node.
		 30 - WRR weight.
		 1  - Id of a level. Since this is a leaf node 1 is passed.
		-1  - Id of a shaper. -1 indicates that shaper is not attached.
		 0  - Congestion management is not supported.
		 0  - Congestion management is not supported.
		 1  - Enable statistics counter for number of transmitted packets.
		 0  - Number of shared shapers.

6. Add leaf node for tx queue 1:

   .. code-block:: console

	testpmd> add port tm leaf node 0 1 3 0 60 1 -1 0 0 1 0

   Parameters have following meaning::

		 0  - Id of a port.
		 1  - Id of a new node.
		 3  - Id of the parent node.
		 0  - Priority of a node.
		 60 - WRR weight.
		 1  - Id of a level. Since this is a leaf node 1 is passed.
		-1  - Id of a shaper. -1 indicates that shaper is not attached.
		 0  - Congestion management is not supported.
		 0  - Congestion management is not supported.
		 1  - Enable statistics counter for number of transmitted packets.
		 0  - Number of shared shapers.

7. Add leaf node for tx queue 2:

   .. code-block:: console

		testpmd> add port tm leaf node 0 2 3 0 99 1 -1 0 0 1 0

   Parameters have following meaning::

		 0  - Id of a port.
		 2  - Id of a new node.
		 3  - Id of the parent node.
		 0  - Priority of a node.
		 99 - WRR weight.
		 1  - Id of a level. Since this is a leaf node 1 is passed.
		-1  - Id of a shaper. -1 indicates that shaper is not attached.
		 0  - Congestion management is not supported.
		 0  - Congestion management is not supported.
		 1  - Enable statistics counter for number of transmitted packets.
		 0  - Number of shared shapers.

8. Commit hierarchy:

   .. code-block:: console

		testpmd> port tm hierarchy commit 0 no

  Parameters have following meaning::

		0  - Id of a port.
		no - Do not flush TM hierarchy if commit fails.

9. Start all ports

   .. code-block:: console

		testpmd> port start all



10. Enable forwarding

   .. code-block:: console

		testpmd> start
