..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

ICE Poll Mode Driver
======================

The ice PMD (**librte_net_ice**) provides poll mode driver support for
10/25/50/100/200 Gbps Intel® Ethernet 800 Series Network Adapters based on
the following controllers,

- Intel Ethernet Controller E810
- Intel Ethernet Connection E822
- Intel Ethernet Connection E823
- Intel Ethernet Connection E825
- Intel Ethernet Controller E830

Linux Prerequisites
-------------------

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

- To get better performance on Intel platforms, please follow the "How to get best performance with NICs on Intel platforms"
  section of the :ref:`Getting Started Guide for Linux <linux_gsg>`.

- Please follow the matching list to download specific kernel driver, firmware and DDP package from
  `https://www.intel.com/content/www/us/en/search.html?ws=text#q=e810&t=Downloads&layout=table`.

- To understand what is DDP package and how it works, please review `Intel® Ethernet Controller E810 Dynamic
  Device Personalization (DDP) for Telecommunications Technology Guide <https://cdrdv2.intel.com/v1/dl/getContent/617015>`_.

- To understand DDP for COMMs usage with DPDK, please review `Intel® Ethernet 800 Series Telecommunication (Comms)
  Dynamic Device Personalization (DDP) Package <https://cdrdv2.intel.com/v1/dl/getContent/618651>`_.

Windows Prerequisites
---------------------

- Follow the :doc:`guide for Windows <../windows_gsg/run_apps>`
  to setup the basic DPDK environment.

- Identify the Intel® Ethernet adapter and get the latest NVM/FW version.

- To access any Intel® Ethernet hardware, load the NetUIO driver in place of existing built-in (inbox) driver.

- To load NetUIO driver, follow the steps mentioned in `dpdk-kmods repository
  <https://git.dpdk.org/dpdk-kmods/tree/windows/netuio/README.rst>`_.

- Loading of private Dynamic Device Personalization (DDP) package is not supported on Windows.


Kernel driver, DDP and Firmware Matching List
---------------------------------------------

It is highly recommended to upgrade the ice kernel driver, firmware and DDP package
to avoid the compatibility issues with ice PMD.
The table below shows a summary of the DPDK versions
with corresponding out-of-tree Linux kernel drivers, DDP package and firmware.
The full list of in-tree and out-of-tree Linux kernel drivers from kernel.org
and Linux distributions that were tested and verified
are listed in the Tested Platforms section of the Release Notes for each release.

For E810,

   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    DPDK   | Kernel Driver | OS Default DDP  | COMMS DDP | Wireless DDP | Firmware  |
   +===========+===============+=================+===========+==============+===========+
   |    21.11  |     1.7.16    |      1.3.27     |  1.3.31   |    1.3.7     |    3.1    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    22.03  |     1.8.3     |      1.3.28     |  1.3.35   |    1.3.8     |    3.2    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    22.07  |     1.9.11    |      1.3.30     |  1.3.37   |    1.3.10    |    4.0    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    22.11  |     1.10.1    |      1.3.30     |  1.3.37   |    1.3.10    |    4.1    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    23.03  |     1.11.1    |      1.3.30     |  1.3.40   |    1.3.10    |    4.2    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    23.07  |     1.12.6    |      1.3.35     |  1.3.45   |    1.3.13    |    4.3    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    23.11  |     1.13.7    |      1.3.36     |  1.3.46   |    1.3.14    |    4.4    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    24.03  |     1.13.7    |      1.3.35     |  1.3.45   |    1.3.13    |    4.4    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    24.07  |     1.14.11   |      1.3.36     |  1.3.46   |    1.3.14    |    4.5    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    24.11  |     1.15.4    |      1.3.36     |  1.3.46   |    1.3.14    |    4.6    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    25.03  |     1.16.3    |      1.3.39     |  1.3.53   |    1.3.14    |    4.7    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    25.07  |     2.2.8     |      1.3.43     |  1.3.55   |    1.3.23    |    4.8    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    25.11  |     2.3.14    |      1.3.43     |  1.3.55   |    1.3.25    |    4.9    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+

For E830,

   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    DPDK   | Kernel Driver | OS Default DDP  | COMMS DDP | Wireless DDP | Firmware  |
   +===========+===============+=================+===========+==============+===========+
   |    25.07  |     2.2.8     |      1.3.43     |  1.3.55   |    1.3.23    |    1.0    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+
   |    25.11  |     2.3.14    |      1.3.43     |  1.3.55   |    1.3.25    |    1.2    |
   +-----------+---------------+-----------------+-----------+--------------+-----------+

Dynamic Device Personalization (DDP) package loading
----------------------------------------------------

The Intel E810 requires a programmable pipeline package
be downloaded by the driver to support normal operations.
The E810 has limited functionality built in to allow PXE boot and other use cases,
but for DPDK use the driver must download a package file during the driver initialization stage.

The default DDP package file name is ``ice.pkg``.
For a specific NIC, the DDP package supposed to be loaded can have a filename:
``ice-xxxxxx.pkg``, where 'xxxxxx' is the 64-bit PCIe Device Serial Number of the NIC.
For example, if the NIC's device serial number is 00-CC-BB-FF-FF-AA-05-68,
the device-specific DDP package filename is ``ice-00ccbbffffaa0568.pkg`` (in hex and all low case).
A symbolic link to the DDP package file is also ok.
The same package file is used by both the kernel driver and the ICE PMD.
For more information, please review the README file from
`Intel® Ethernet 800 Series Dynamic Device Personalization (DDP) for Telecommunication (Comms) Package
<https://www.intel.com/content/www/us/en/download/19660/intel-ethernet-800-series-dynamic-device-personalization-ddp-for-telecommunication-comms-package.html>`_.

ICE PMD supports using a customized DDP search path.
The driver will read the search path from
``/sys/module/firmware_class/parameters/path`` as a ``CUSTOMIZED_PATH``.
During initialization, the driver searches in the following paths in order:
``CUSTOMIZED_PATH``, ``/lib/firmware/updates/intel/ice/ddp`` and ``/lib/firmware/intel/ice/ddp``.
The device-specific DDP package has a higher loading priority than default DDP package, ``ice.pkg``.

.. note::

   Windows support: DDP packages are not supported on Windows.

Configuration
-------------

Runtime Configuration
~~~~~~~~~~~~~~~~~~~~~

- ``Safe Mode Support`` (default ``0``)

  If driver failed to load OS package, by default driver's initialization failed.
  But if user intend to use the device without OS package, user can take ``devargs``
  parameter ``safe-mode-support``, for example::

    -a 80:00.0,safe-mode-support=1

  Then the driver will be initialized successfully and the device will enter Safe Mode.
  NOTE: In Safe mode, only very limited features are available, features like RSS,
  checksum, fdir, tunneling ... are all disabled.

- ``Default MAC Disable`` (default ``0``)

  Disable the default MAC make the device drop all packets by default,
  only packets hit on filter rules will pass.

  Default MAC can be disabled by setting the devargs parameter ``default-mac-disable``,
  for example::

    -a 80:00.0,default-mac-disable=1

- ``DDP Package File``

  Rather than have the driver search for the DDP package to load,
  or to override what package is used,
  the ``ddp_pkg_file`` option can be used to provide the path to a specific package file.
  For example::

    -a 80:00.0,ddp_pkg_file=/path/to/ice-version.pkg

- ``Traffic Management Scheduling Levels``

  The DPDK Traffic Management (rte_tm) APIs can be used to configure the Tx scheduler on the NIC.
  From 24.11 release, all available hardware layers are available to software.
  Earlier versions of DPDK only supported 3 levels in the scheduling hierarchy.
  To help with backward compatibility the ``tm_sched_levels`` parameter
  can be used to limit the scheduler levels to the provided value.
  The provided value must be between 3 and 8.
  If the value provided is greater than the number of levels provided by the HW,
  SW will use the hardware maximum value.

- ``Source Prune Enable`` (default ``0``)

  Enable Source Prune to automatically drop incoming packets
  when their source MAC address matches one of the MAC addresses
  assigned to that same NIC port.

  Source Prune can be enabled by setting the devargs parameter ``source-prune``,
  for example::

    -a 80:00.0,source-prune=1

- ``Protocol extraction for per queue``

  Configure the RX queues to do protocol extraction into mbuf for protocol
  handling acceleration, like checking the TCP SYN packets quickly.

  The argument format is::

      18:00.0,proto_xtr=<queues:protocol>[<queues:protocol>...],field_offs=<offset>, \
      field_name=<name>
      18:00.0,proto_xtr=<protocol>,field_offs=<offset>,field_name=<name>

  Queues are grouped by ``(`` and ``)`` within the group. The ``-`` character
  is used as a range separator and ``,`` is used as a single number separator.
  The grouping ``()`` can be omitted for single element group. If no queues are
  specified, PMD will use this protocol extraction type for all queues.
  ``field_offs`` is the offset of mbuf dynamic field for protocol extraction data.
  ``field_name`` is the name of mbuf dynamic field for protocol extraction data.
  ``field_offs`` and ``field_name`` will be checked whether it is valid. If invalid,
  an error print will be returned: ``Invalid field offset or name, no match dynfield``,
  and the proto_ext function will not be enabled.

  Protocol is : ``vlan, ipv4, ipv6, ipv6_flow, tcp, ip_offset``.

  .. code-block:: console

    dpdk-testpmd -l 0-7 -- -i
    port stop 0
    port detach 0
    port attach 18:00.0,proto_xtr='[(1,2-3,8-9):tcp,10-13:vlan]',field_offs=92,field_name=pmd_dyn

  This setting means queues 1, 2-3, 8-9 are TCP extraction, queues 10-13 are
  VLAN extraction, other queues run with no protocol extraction. The offset of mbuf
  dynamic field is 92 for all queues with protocol extraction.

  .. code-block:: console

    dpdk-testpmd -l 0-7 -- -i
    port stop 0
    port detach 0
    port attach 18:00.0,proto_xtr=vlan,proto_xtr='[(1,2-3,8-9):tcp,10-23:ipv6]', \
    field_offs=92,field_name=pmd_dyn

  This setting means queues 1, 2-3, 8-9 are TCP extraction, queues 10-23 are
  IPv6 extraction, other queues use the default VLAN extraction. The offset of mbuf
  dynamic field is 92 for all queues with protocol extraction.

  The extraction metadata is copied into the registered dynamic mbuf field, and
  the related dynamic mbuf flags is set.

  .. table:: Protocol extraction : ``vlan``

   +----------------------------+----------------------------+
   |           VLAN2            |           VLAN1            |
   +======+===+=================+======+===+=================+
   |  PCP | D |       VID       |  PCP | D |       VID       |
   +------+---+-----------------+------+---+-----------------+

  VLAN1 - single or EVLAN (first for QinQ).

  VLAN2 - C-VLAN (second for QinQ).

  .. table:: Protocol extraction : ``ipv4``

   +----------------------------+----------------------------+
   |           IPHDR2           |           IPHDR1           |
   +======+=======+=============+==============+=============+
   |  Ver |Hdr Len|    ToS      |      TTL     |  Protocol   |
   +------+-------+-------------+--------------+-------------+

  IPHDR1 - IPv4 header word 4, "TTL" and "Protocol" fields.

  IPHDR2 - IPv4 header word 0, "Ver", "Hdr Len" and "Type of Service" fields.

  .. table:: Protocol extraction : ``ipv6``

   +----------------------------+----------------------------+
   |           IPHDR2           |           IPHDR1           |
   +=====+=============+========+=============+==============+
   | Ver |Traffic class|  Flow  | Next Header |   Hop Limit  |
   +-----+-------------+--------+-------------+--------------+

  IPHDR1 - IPv6 header word 3, "Next Header" and "Hop Limit" fields.

  IPHDR2 - IPv6 header word 0, "Ver", "Traffic class" and high 4 bits of
  "Flow Label" fields.

  .. table:: Protocol extraction : ``ipv6_flow``

   +----------------------------+----------------------------+
   |           IPHDR2           |           IPHDR1           |
   +=====+=============+========+============================+
   | Ver |Traffic class|            Flow Label               |
   +-----+-------------+-------------------------------------+

  IPHDR1 - IPv6 header word 1, 16 low bits of the "Flow Label" field.

  IPHDR2 - IPv6 header word 0, "Ver", "Traffic class" and high 4 bits of
  "Flow Label" fields.

  .. table:: Protocol extraction : ``tcp``

   +----------------------------+----------------------------+
   |           TCPHDR2          |           TCPHDR1          |
   +============================+======+======+==============+
   |          Reserved          |Offset|  RSV |     Flags    |
   +----------------------------+------+------+--------------+

  TCPHDR1 - TCP header word 6, "Data Offset" and "Flags" fields.

  TCPHDR2 - Reserved

  .. table:: Protocol extraction : ``ip_offset``

   +----------------------------+----------------------------+
   |           IPHDR2           |           IPHDR1           |
   +============================+============================+
   |       IPv6 HDR Offset      |       IPv4 HDR Offset      |
   +----------------------------+----------------------------+

  IPHDR1 - Outer/Single IPv4 Header offset.

  IPHDR2 - Outer/Single IPv6 Header offset.

- ``Hardware debug mask log support`` (default ``0``)

  User can enable the related hardware debug mask such as ICE_DBG_NVM::

    -a 0000:88:00.0,hw_debug_mask=0x80 --log-level=pmd.net.ice.driver:8

  These ICE_DBG_XXX are defined in ``drivers/net/intel/ice/base/ice_type.h``.

- ``1PPS out support``

  The E810 supports four single-ended GPIO signals (SDP[20:23]). The 1PPS
  signal outputs via SDP[20:23]. User can select GPIO pin index flexibly.
  Pin index 0 means SDP20, 1 means SDP21 and so on. For example::

    -a af:00.0,pps_out='[pin:0]'

- ``Link state on close`` (default ``down``)

  The user can request that the link be set to up or down
  or restored to its original state when the device is closed::

    -a af:00.0,link_state_on_close=<state>

  Supported values for the ``<state>`` parameter:

  * ``down``: Leave the link in the down state.
  * ``up``: Leave the link in the up state.
  * ``initial``: Restore the link to the state it was in when the device started.

- ``Low Rx latency`` (default ``0``)

  vRAN workloads require low latency DPDK interface for the front haul
  interface connection to Radio. By specifying ``1`` for parameter
  ``rx_low_latency``, each completed Rx descriptor can be written immediately
  to host memory and the Rx interrupt latency can be reduced to 2us::

    -a 0000:88:00.0,rx_low_latency=1

  As a trade-off, this configuration may cause the packet processing performance
  degradation due to the PCI bandwidth limitation.

- ``Tx Scheduler Topology Download``

  The default Tx scheduler topology exposed by the NIC,
  generally a 9-level topology of which 8 levels are SW configurable,
  may be updated by a new topology loaded from a DDP package file.
  The ``ddp_load_sched_topo`` option can be used to specify that the scheduler topology,
  if any, in the DDP package file being used should be loaded into the NIC.
  For example::

    -a 0000:88:00.0,ddp_load_sched_topo=1

  or::

    -a 0000:88:00.0,ddp_pkg_file=/path/to/pkg.file,ddp_load_sched_topo=1

- ``Tx diagnostics`` (default ``not enabled``)

  Set the ``devargs`` parameter ``mbuf_check`` to enable Tx diagnostics.
  For example, ``-a 81:00.0,mbuf_check=<case>`` or ``-a 81:00.0,mbuf_check=[<case1>,<case2>...]``.
  Thereafter, ``rte_eth_xstats_get()`` can be used to get the error counts,
  which are collected in ``tx_mbuf_error_packets`` xstats.
  In testpmd these can be shown via: ``testpmd> show port xstats all``.
  Supported values for the ``case`` parameter are:

  * ``mbuf``: Check for corrupted mbuf.
  * ``size``: Check min/max packet length according to HW spec.
  * ``segment``: Check number of mbuf segments does not exceed HW limits.
  * ``offload``: Check for use of an unsupported offload flag.

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Features
--------

Vector PMD
~~~~~~~~~~

Vector PMD for RX and TX path are selected automatically. The paths
are chosen based on 2 conditions.

- ``CPU``
  On the X86 platform, the driver checks if the CPU supports AVX2.
  If it's supported, AVX2 paths will be chosen. If not, SSE is chosen.
  If the CPU supports AVX512 and EAL argument ``--force-max-simd-bitwidth``
  is set to 512, AVX512 paths will be chosen.

- ``Offload features``
  The supported HW offload features are described in the document ice.ini,
  A value "P" means the offload feature is not supported by vector path.
  If any not supported features are used, ICE vector PMD is disabled and the
  normal paths are chosen.

Malicious driver detection (MDD)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It's not appropriate to send a packet, if this packet's destination MAC address
is just this port's MAC address. If SW tries to send such packets, HW will
report a MDD event and drop the packets.

The APPs based on DPDK should avoid providing such packets.

Device Config Function (DCF)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section demonstrates ICE DCF PMD, which shares the core module with ICE
PMD and iAVF PMD.

A DCF (Device Config Function) PMD bounds to the device's trusted VF with ID 0,
it can act as a sole controlling entity to exercise advance functionality (such
as switch, ACL) for the rest VFs.

The DCF PMD needs to advertise and acquire DCF capability which allows DCF to
send AdminQ commands that it would like to execute over to the PF and receive
responses for the same from PF.


Data Center Bridging (DCB) and Priority Flow Control (PFC)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ice PMD supports Data Center Bridging (DCB) and Priority Flow Control (PFC).
These features enable Quality of Service (QoS) in data center environments by allowing
traffic classification and prioritization across multiple traffic classes.

DCB Configuration
+++++++++++++++++

DCB can be enabled by configuring the device with ``RTE_ETH_MQ_RX_DCB_FLAG``
in the Rx mode during device configuration.

The ice PMD supports:

* 4 or 8 traffic classes (TCs)
* VLAN Priority to Traffic Class mapping

Limitations:

* All TCs are configured with Enhanced Transmission Selection (ETS) with even bandwidth allocation
* Queues are evenly distributed across configured TCs
* The number of queues must be evenly divisible by the number of traffic classes
* The number of queues per TC must be a power of 2
* Traffic classes must be configured contiguously starting from TC 0

Example DCB configuration in testpmd:

.. code-block:: console

   dpdk-testpmd -a 0000:18:00.0 -- -i --nb-cores=8 --rxq=8 --txq=8
   port stop 0
   port config 0 dcb vt off 4 pfc off
   port start 0

This configures 4 traffic classes with 2 queues per TC.

Example DCB configuration in application code:

.. code-block:: c

   struct rte_eth_conf port_conf = {
       .rxmode = {
           .mq_mode = RTE_ETH_MQ_RX_DCB,
       },
       .rx_adv_conf = {
           .dcb_rx_conf = {
               .nb_tcs = RTE_ETH_4_TCS,
               .dcb_tc = {0, 1, 2, 3, 0, 1, 2, 3},  /* Map priorities to TCs */
           },
       },
   };

   ret = rte_eth_dev_configure(port_id, nb_rx_queues, nb_tx_queues, &port_conf);

PFC Configuration
+++++++++++++++++

Priority Flow Control (PFC) provides a mechanism to pause and resume traffic
on individual traffic classes, enabling lossless Ethernet for specific priorities.

PFC can be configured per priority using the ``rte_eth_dev_priority_flow_ctrl_set()`` API.
Each traffic class can be independently configured with:

* RX pause only
* TX pause only
* Full duplex pause
* No pause (disabled)

PFC operates in VLAN-based mode and requires DCB to be configured first.

Features:

* Per-TC pause control (XON/XOFF)
* Configurable high/low watermarks for buffer management
* Configurable pause quanta
* PFC statistics exposed via xstats

Example PFC configuration in testpmd:

.. code-block:: console

   set pfc_ctrl rx on tx on 100000 50000 65535 0 0

This enables PFC on port 0 priority 0 with high watermark of 100000 bytes,
low watermark of 50000 bytes, and pause time of 65535.

Example PFC configuration using DPDK API:

.. code-block:: c

   struct rte_eth_pfc_conf pfc_conf;

   pfc_conf.fc.mode = RTE_ETH_FC_FULL;           /* Enable full duplex pause */
   pfc_conf.fc.high_water = 100000;              /* High watermark in bytes */
   pfc_conf.fc.low_water = 50000;                /* Low watermark in bytes */
   pfc_conf.fc.pause_time = 0xFFFF;              /* Pause quanta (in 512 bit-time units) */
   pfc_conf.priority = 0;                        /* Configure PFC for priority 0 */

   ret = rte_eth_dev_priority_flow_ctrl_set(port_id, &pfc_conf);


Forward Error Correction (FEC)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Supports get/set FEC mode and get FEC capability.

Time Synchronisation
~~~~~~~~~~~~~~~~~~~~

The system operator can run a PTP (Precision Time Protocol) client application
to synchronise the time on the network card
(and optionally the time on the system) to the PTP master.

ICE PMD supports PTP client applications that use the DPDK IEEE 1588 API
to communicate with the PTP master clock.
Note that PTP client application needs to run on PF
and add the ``--force-max-simd-bitwidth=64`` startup parameter to disable vector mode.

.. code-block:: console

   examples/dpdk-ptpclient -l 0-3 -n 3 -a 0000:ec:00.1 --force-max-simd-bitwidth=64 -- -T 1 -p 0x1 -c 1

Tx Packet Pacing
~~~~~~~~~~~~~~~~

In order to deliver the timestamp with every packet,
a special type of Tx Host Queue is used, the TS Queue.
This feature is currently supported only in E830 adapters.

The flag ``RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP`` is used to enable the feature.
In order to deliver timestamps internally ``set txtimes`` is used,
where inter burst and intra burst time interval in nsecs is provided.

Note that DPDK library should be compiled using PTP support enabled
and testpmd application should run on PF.

For example:

.. code-block:: console

   dpdk-testpmd -a 0000:31:00.0 -l 0-3 -- -i --tx-offloads=0x200000
   set fwd txonly
   set txtimes <inter_burst>,<intra_burst>
   start

Based on the Tx offload initialised during port configuration time,
Tx Time Queue will be enabled during ``ice_tx_queue_setup()`` only for E830 adapters.
The initial time should be fetched using ``rte_eth_read_clock()``.
Further the timestamps should be calculated based on the inter burst and intra burst times,
then storing it into proper format (refer to ``struct tx_timestamp`` in ``tx_only.c``),
as to be placed in packet header.
The timestamps should be copied to packet mbufs
and manually adjust packet header length accordingly.

Generic Flow Support
~~~~~~~~~~~~~~~~~~~~

The ice PMD provides support for the Generic Flow API (RTE_FLOW), enabling
users to offload various flow classification tasks to the E810 NIC.
The E810 NIC's  packet processing pipeline consists of the following stages:

Switch: Supports exact match and limited wildcard matching with a large flow
capacity.

ACL: Supports wildcard matching with a smaller flow capacity (DCF mode only).

FDIR: Supports exact match with a large flow capacity (PF mode only).

Hash: Supports RSS (PF mode only)

The ice PMD utilizes the ice_flow_engine structure to represent each of these
stages and leverages the rte_flow rule's ``group`` attribute for selecting the
appropriate engine for Switch, ACL, and FDIR operations:

Group 0 maps to Switch
Group 1 maps to ACL
Group 2 maps to FDIR

In the case of RSS, it will only be selected if a ``RTE_FLOW_ACTION_RSS`` action
is targeted to no queue group, and the group attribute is ignored.

For each engine, a list of supported patterns is maintained in a global array
named ``ice_<engine>_supported_pattern``. The Ice PMD will reject any rule with
a pattern that is not included in the supported list.

Protocol Agnostic Filtering
~~~~~~~~~~~~~~~~~~~~~~~~~~~

One notable feature is the ice PMD's ability to leverage the raw pattern,
enabling protocol-agnostic flow offloading.
This feature allows users to create flow rules for any protocol recognized by the hardware parser,
by manually specifying the raw packet structure.
Therefore, flow offloading can be used
even in cases where desired protocol isn't explicitly supported by the flow API.

Raw Pattern Components
++++++++++++++++++++++

Raw patterns consist of two key components:

**Pattern Spec**
  An ASCII hexadecimal string representing the complete packet structure
  that defines the packet type and protocol layout.
  The hardware parser analyzes this structure to determine the packet type (PTYPE)
  and identify protocol headers and their offsets.
  This specification must represent a valid packet structure
  that the hardware can parse and classify.
  If the hardware parser does not support a particular protocol stack,
  it may not correctly identify the packet type.

**Pattern Mask**
  An ASCII hexadecimal string of the same length as the spec
  that determines which specific fields within the packet will be extracted and used for matching.
  The mask control field extraction without affecting the packet type identification.

.. note::

   Raw pattern must be the only flow item in the flow item list.

Generating Raw Pattern Values
+++++++++++++++++++++++++++++

To create raw patterns, follow these steps:

#. **Verify parser support**:
   Confirm that the hardware parser supports the protocol combination
   needed for the intended flow rule.
   This can be checked against the documentation for the DDP package currently in use.

#. **Build the packet template**:
   Create a complete, valid packet header
   with all necessary sections (Ethernet, IP, UDP/TCP, etc.)
   using the exact field values that need to be matched.

#. **Convert to hexadecimal**:
   Transform the entire header into a continuous ASCII hexadecimal string,
   with each byte represented as two hex characters.

#. **Create the extraction mask**:
   Generate a mask of the same length as the spec,
   where set bits would indicate the fields used for extraction/matching.

VPP project's `flow_parse.py` script can be used
to generate packet templates and masks for raw patterns.
This tool takes a human-readable flow description
and outputs the corresponding ASCII hexadecimal spec and mask.
This script can be found under ``extras/packetforge``
in `VPP project <https://github.com/FDio/vpp/blob/master/extras/packetforge/flow_parse.py>`_.

Example usage:

.. code-block:: console

   python3 flow_parse.py --show -p "mac()/ipv4(src=1.1.1.1,dst=2.2.2.2)/udp()"

Output:

.. code-block:: console

   {'flow': {'generic': {'pattern': {'spec': b'00000000000100000000000208004500001c000000000011000001010101020202020000000000080000',
   'mask': b'0000000000000000000000000000000000000000000000000000ffffffffffffffff0000000000000000'}}}}

.. note::

   Ensure the spec represents complete protocol headers,
   as the hardware parser processes fields at 16-bit boundaries.
   Incomplete or truncated headers may result in unpredictable field extraction behavior.

Action Support and Usage
^^^^^^^^^^^^^^^^^^^^^^^^

After constructing the raw pattern spec and mask,
they can be used in the flow API with pattern type "raw".

The following is an example of a minimal Ethernet + IPv4 header template.
Source and destination IPv4 addresses are part of the match key; all other fields are ignored.

Spec (packet template):

.. code-block::

  000000000001              Destination MAC (6 bytes)
  000000000002              Source MAC (6 bytes)
  0800                      EtherType = IPv4
  4500001c0000000000110000  IPv4 header, protocol = UDP
  01010101                  Source IP = 1.1.1.1
  02020202                  Destination IP = 2.2.2.2
  0000000000080000          UDP header

Mask:

.. code-block::

  000000000000              Destination MAC (ignored)
  000000000000              Source MAC (ignored)
  0000                      EtherType (ignored)
  000000000000000000000000  IPv4/UDP header (ignored)
  ffffffff                  Source IP (match all 32 bits)
  ffffffff                  Destination IP (match all 32 bits)
  0000000000000000          UDP header (ignored)

This spec will match any non-fragmented IPv4/UDP packet
whose source IP is 1.1.1.1 and destination IP is 2.2.2.2.

Currently, the following actions are supported:

- **mark**:
  Attaches a user-defined integer value to matching packets.
  Can be specified together with another action.

- **queue**:
  Directs matching packets to a specific receive queue.

- **drop**:
  Discards matching packets at the hardware level.

- **rss**:
  Enables Receive Side Scaling (RSS) for matching packets.

Constraints:
  * For RSS, only the global configuration is used;
    per-rule queue lists or RSS keys are not supported.

To direct matching packets to a specific queue, and set mbuf FDIR metadata in:

.. code-block:: console

   flow create 0 ingress pattern raw \
     pattern spec 00000000000100000000000208004500001c000000000011000001010101020202020000000000080000 \
     pattern mask 0000000000000000000000000000000000000000000000000000ffffffffffffffff0000000000000000 / end \
     actions queue index 3 mark id 3 / end

Equivalent C code using the flow API:

.. code-block:: c

   /* Hex string for the packet spec (Ethernet + IPv4 + UDP header) */
   static const uint8_t raw_pattern_spec[] = {
       0x00, 0x00, 0x00, 0x00, 0x00, 0x01,  /* Destination MAC */
       0x00, 0x00, 0x00, 0x00, 0x00, 0x02,  /* Source MAC */
       0x08, 0x00,                          /* EtherType: IPv4 */
       0x45, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x11, 0x00, 0x00,              /* IPv4 header, protocol UDP */
       0x01, 0x01, 0x01, 0x01,              /* Source IP: 1.1.1.1 */
       0x02, 0x02, 0x02, 0x02,              /* Destination IP: 2.2.2.2 */
       0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00  /* UDP header */
   };

   /* Mask indicating which fields to match (source and destination IPs) */
   static const uint8_t raw_pattern_mask[] = {
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* MAC addresses - ignored */
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00,                          /* EtherType - ignored */
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00,              /* IPv4/UDP headers - ignored */
       0xff, 0xff, 0xff, 0xff,              /* Source IP - match all bits */
       0xff, 0xff, 0xff, 0xff,              /* Destination IP - match all bits */
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* UDP - ignored */
   };

   struct rte_flow_item_raw raw_spec = {
       .length = sizeof(raw_pattern_spec),
       .pattern = raw_pattern_spec,
   };

   struct rte_flow_item_raw raw_mask = {
       .length = sizeof(raw_pattern_mask),
       .pattern = raw_pattern_mask,
   };

   struct rte_flow_attr attr = {
       .ingress = 1,
   };

   struct rte_flow_item pattern[] = {
       {
           .type = RTE_FLOW_ITEM_TYPE_RAW,
           .spec = &raw_spec,
           .mask = &raw_mask,
       },
       {
           .type = RTE_FLOW_ITEM_TYPE_END,
       },
   };

   struct rte_flow_action actions[] = {
       /* direct flow to queue index 3 */
       {
           .type = RTE_FLOW_ACTION_TYPE_QUEUE,
           .conf = &(struct rte_flow_action_queue){ .index = 3 },
       },
       /* write id into mbuf FDIR metadata */
       {
           .type = RTE_FLOW_ACTION_TYPE_MARK,
           .conf = &(struct rte_flow_action_mark){ .id = 3 },
       },
       {
           .type = RTE_FLOW_ACTION_TYPE_END,
       },
   };

   struct rte_flow_error error;
   struct rte_flow *flow = flow = rte_flow_create(port_id, &attr, pattern, actions, &error);

To use masked bits (IPv4 source/destination addresses) to distribute such packets via RSS:

.. code-block:: console

   flow create 0 ingress pattern raw \
     pattern spec 00000000000100000000000208004500001c000000000011000001010101020202020000000000080000 \
     pattern mask 0000000000000000000000000000000000000000000000000000ffffffffffffffff0000000000000000 / end \
     actions rss / end

Equivalent C code using the flow API:

.. code-block:: c

   /* Use the same structures and code as above, only actions change */

   struct rte_flow_action actions[] = {
       {
           .type = RTE_FLOW_ACTION_TYPE_RSS,
           /* Use NULL conf for default RSS configuration */
       },
       {
           .type = RTE_FLOW_ACTION_TYPE_END,
       },
   };

**Limitations**

Currently, raw pattern support is limited to the FDIR and Hash engines.

.. note::

   **DDP Package Dependency**:
   Raw pattern functionality relies on the loaded DDP package
   to define available packet types and protocol parsing rules.
   Different DDP packages (OS Default, COMMS, Wireless)
   may support different protocol combinations and PTYPE mappings.

Traffic Management Support
~~~~~~~~~~~~~~~~~~~~~~~~~~

The ice PMD provides support for the Traffic Management API (RTE_TM),
enabling users to configure and manage the traffic shaping and scheduling of transmitted packets.
By default, all available transmit scheduler layers are available for configuration,
allowing up to 2000 queues to be configured in a hierarchy of up to 8 levels.
The number of levels in the hierarchy can be adjusted via driver parameters:

* the default 9-level topology (8 levels usable) can be replaced by a new topology downloaded from a DDP file,
  using the driver parameter ``ddp_load_sched_topo=1``.
  Using this mechanism, if the number of levels is reduced,
  the possible fan-out of child-nodes from each level may be increased.
  The default topology is a 9-level tree with a fan-out of 8 at each level.
  Released DDP package files contain a 5-level hierarchy (4-levels usable),
  with increased fan-out at the lower 3 levels
  e.g. 64 at levels 2 and 3, and 256 or more at the leaf-node level.

* the number of levels can be reduced
  by setting the driver parameter ``tm_sched_levels`` to a lower value.
  This scheme will reduce in software the number of editable levels,
  but will not affect the fan-out from each level.

For more details on how to configure a Tx scheduling hierarchy,
please refer to the ``rte_tm`` `API documentation <https://doc.dpdk.org/api/rte__tm_8h.html>`_.

Additional Options
++++++++++++++++++

- ``Disable ACL Engine`` (default ``enabled``)

  By default, all flow engines are enabled. But if user does not need the
  ACL engine related functions, user can set ``devargs`` parameter
  ``acl=off`` to disable the ACL engine and shorten the startup time.

    -a 18:01.0,cap=dcf,acl=off

.. _figure_ice_dcf:

.. figure:: img/ice_dcf.*

   DCF Communication flow.

#. Create the VFs::

      echo 4 > /sys/bus/pci/devices/0000\:18\:00.0/sriov_numvfs

#. Enable the VF0 trust on::

      ip link set dev enp24s0f0 vf 0 trust on

#. Bind the VF0, and run testpmd with 'cap=dcf' with port representor for VF 1 and 2::

      dpdk-testpmd -l 22-25 -a 18:01.0,cap=dcf,representor=vf[1-2] -- -i

#. Monitor the VF2 interface network traffic::

      tcpdump -e -nn -i enp24s1f2

#. Create one flow to redirect the traffic to VF2 by DCF (assume the representor port ID is 5)::

      flow create 0 priority 0 ingress pattern eth / ipv4 src is 192.168.0.2 \
      dst is 192.168.0.3 / end actions represented_port ethdev_port_id 5 / end

#. Send the packet, and it should be displayed on tcpdump::

      sendp(Ether(src='3c:fd:fe:aa:bb:78', dst='00:00:00:01:02:03')/IP(src=' \
      192.168.0.2', dst="192.168.0.3")/TCP(flags='S')/Raw(load='XXXXXXXXXX'), \
      iface="enp24s0f0", count=10)

Sample Application Notes
------------------------

Vlan filter
~~~~~~~~~~~

Vlan filter only works when Promiscuous mode is off.

To start ``testpmd``, and add vlan 10 to port 0:

.. code-block:: console

    ./app/dpdk-testpmd -l 0-15 -- -i
    ...

    testpmd> rx_vlan add 10 0

Diagnostic Utilities
--------------------

Dump DDP Package
~~~~~~~~~~~~~~~~

Dump the runtime packet processing pipeline configuration into a binary file.
This helps the support team diagnose hardware configuration issues.

Usage::

    testpmd> ddp dump <port_id> <output_file>

Dump Switch Configurations
~~~~~~~~~~~~~~~~~~~~~~~~~~

Dump detail hardware configurations related to the switch pipeline stage into a binary file.

Usage::

    testpmd> ddp dump switch <port_id> <output_file>

Dump Tx Scheduling Tree
~~~~~~~~~~~~~~~~~~~~~~~

Dump the runtime Tx scheduling tree into a DOT file.

Usage::

    testpmd> txsched dump <port_id> <brief|detail> <output_file>

In "brief" mode, all scheduling nodes in the tree are displayed.
In "detail" mode, each node's configuration parameters are also displayed.
