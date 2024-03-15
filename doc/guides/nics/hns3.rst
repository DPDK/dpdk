..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 HiSilicon Limited.

HNS3 Poll Mode Driver
===============================

The hns3 PMD (**librte_net_hns3**) provides poll mode driver support
for the inbuilt HiSilicon Network Subsystem(HNS) network engine
found in the HiSilicon Kunpeng 920 SoC (HIP08) and Kunpeng 930 SoC (HIP09/HIP10).

Features
--------

Features of the HNS3 PMD are:

- Multiple queues for TX and RX
- Receive Side Scaling (RSS)
- Packet type information
- Checksum offload
- TSO offload
- LRO offload
- Promiscuous mode
- Multicast mode
- Port hardware statistics
- Jumbo frames
- Link state information
- Interrupt mode for RX
- VLAN stripping and inserting
- QinQ inserting
- DCB
- Scattered and gather for TX and RX
- Vector Poll mode driver
- SR-IOV VF
- Multi-process
- MAC/VLAN filter
- MTU update
- NUMA support
- Generic flow API
- IEEE1588/802.1AS timestamping
- Basic stats
- Extended stats
- Traffic Management API
- Speed capabilities
- Link Auto-negotiation
- Link flow control
- Dump register
- Dump private info from device
- FW version

Prerequisites
-------------
- Get the information about Kunpeng920 chip using
  `<https://www.hisilicon.com/en/products/Kunpeng>`_.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to
  setup the basic DPDK environment.

Link status event Pre-conditions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Firmware 1.8.0.0 and later versions support reporting link changes to the PF.
Therefore, to use the LSC for the PF driver, ensure that the firmware version
also supports reporting link changes.
If the VF driver needs to support LSC, special patch must be added:
`<https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=18b6e31f8bf4ac7af7b057228f38a5a530378e4e>`_.

Note: The patch has been uploaded to 5.13 of the Linux kernel mainline.


Configuration
-------------

Compilation Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config/rte_config.h`` file.

- ``RTE_LIBRTE_HNS3_MAX_TQP_NUM_PER_PF`` (default ``256``)

  Number of MAX queues reserved for PF on HIP09 and HIP10.
  The MAX queue number is also determined by the value the firmware report.

Runtime Configuration
~~~~~~~~~~~~~~~~~~~~~

- ``rx_func_hint`` (default ``none``)

  Used to select Rx burst function, supported value are ``vec``, ``sve``,
  ``simple``, ``common``.
  ``vec``, if supported use the ``vec`` Rx function which indicates the
  default vector algorithm, neon for Kunpeng Arm platform.
  ``sve``, if supported use the ``sve`` Rx function which indicates the
  sve algorithm.
  ``simple``, if supported use the ``simple`` Rx function which indicates
  the scalar simple algorithm.
  ``common``, if supported use the ``common`` Rx function which indicates
  the scalar scattered algorithm.

  When provided parameter is not supported, ``vec`` usage condition will
  be first checked, if meets, use the ``vec``. Then, ``simple``, at last
  ``common``.

  For example::

    -a 0000:7d:00.0,rx_func_hint=simple

- ``tx_func_hint`` (default ``none``)

  Used to select Tx burst function, supported value are ``vec``, ``sve``,
  ``simple``, ``common``.
  ``vec``, if supported use the ``vec`` Tx function which indicates the
  default vector algorithm, neon for Kunpeng Arm platform.
  ``sve``, if supported use the ``sve`` Tx function which indicates the
  sve algorithm.
  ``simple``, if supported use the ``simple`` Tx function which indicates
  the scalar simple algorithm.
  ``common``, if supported use the ``common`` Tx function which indicates
  the scalar algorithm.

  When provided parameter is not supported, ``vec`` usage condition will
  be first checked, if meets, use the ``vec``. Then, ``simple``, at last
  ``common``.

  For example::

    -a 0000:7d:00.0,tx_func_hint=common

- ``dev_caps_mask`` (default ``0``)

  Used to mask the capability which queried from firmware.
  This args take hexadecimal bitmask where each bit represents whether mask
  corresponding capability. eg. If the capability is 0xFFFF queried from
  firmware, and the args value is 0xF which means the bit0~bit3 should be
  masked off, then the capability will be 0xFFF0.
  Its main purpose is to debug and avoid problems.

  For example::

    -a 0000:7d:00.0,dev_caps_mask=0xF

- ``mbx_time_limit_ms`` (default ``500``)

  Used to define the mailbox time limit by user.
  Current, the max waiting time for MBX response is 500ms, but in
  some scenarios, it is not enough. Since it depends on the response
  of the kernel mode driver, and its response time is related to the
  scheduling of the system. In this special scenario, most of the
  cores are isolated, and only a few cores are used for system
  scheduling. When a large number of services are started, the
  scheduling of the system will be very busy, and the reply of the
  mbx message will time out, which will cause our PMD initialization
  to fail. So provide access to set mailbox time limit for user.

  For example::

    -a 0000:7d:00.0,mbx_time_limit_ms=600

- ``fdir_vlan_match_mode`` (default ``strict``)

  Used to select VLAN match mode. This runtime config can be ``strict``
  or ``nostrict`` and is only valid for PF devices.
  If driver works on ``strict`` mode (default mode), hardware does strictly
  match the input flow base on VLAN number.

  For the following scenarios with two rules:

  .. code-block:: console

     rule0:
       pattern: eth type is 0x0806
       actions: queue index 3
     rule1:
       pattern: eth type is 0x0806 / vlan vid is 20
       actions: queue index 4

  If application select ``strict`` mode, only the ARP packets with VLAN
  20 are directed to queue 4, and the ARP packets with other VLAN ID
  cannot be directed to the specified queue. If application want to all
  ARP packets with or without VLAN to be directed to the specified queue,
  application can select ``nostrict`` mode and just need to set rule0.

  For example::

    -a 0000:7d:00.0,fdir_vlan_match_mode=nostrict

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Sample Application Notes
------------------------

VLAN filter
~~~~~~~~~~~

VLAN filter only works when Promiscuous mode is off.

To start ``testpmd``, and add VLAN 10 to port 0:

.. code-block:: console

    ./<build_dir>/app/dpdk-testpmd -l 0-15 -n 4 -- -i --forward-mode=mac
    ...

    testpmd> set promisc 0 off
    testpmd> vlan set filter on 0
    testpmd> rx_vlan add 10 0


Flow Director
~~~~~~~~~~~~~

The Flow Director works in receive mode to identify specific flows or sets of
flows and route them to specific queues.
The Flow Director filters can match the different fields for different type of
packet: flow type, specific input set per flow type.


Start ``testpmd``:

.. code-block:: console

   ./<build_dir>/app/dpdk-testpmd -l 0-15 -n 4 -- -i --rxq=8 --txq=8 \
				  --nb-cores=8 --nb-ports=1

Add a rule to direct ``ipv4-udp`` packet whose ``dst_ip=2.2.2.5, src_ip=2.2.2.3,
src_port=32, dst_port=32`` to queue 1:

.. code-block:: console

   testpmd> flow create 0 ingress pattern eth / ipv4 src is 2.2.2.3 \
            dst is 2.2.2.5 / udp src is 32 dst is 32 / end \
            actions mark id 1 / queue index 1 / end

Generic flow API
~~~~~~~~~~~~~~~~

- ``RSS Flow``

  RSS Flow supports for creating rule base on input tuple, hash key, queues
  and hash algorithm. But hash key, queues and hash algorithm are the global
  configuration for hardware which will affect other rules.
  The rule just setting input tuple is completely independent.

  Run ``testpmd``:

  .. code-block:: console

    dpdk-testpmd -a 0000:7d:00.0 -l 10-18 -- -i --rxq=8 --txq=8

  All IP packets can be distributed to 8 queues.

  Set IPv4-TCP packet is distributed to 8 queues based on L3/L4 SRC only.

  .. code-block:: console

    testpmd> flow create 0 ingress pattern eth / ipv4 / tcp / end actions \
             rss types ipv4-tcp l4-src-only l3-src-only end queues end / end

  Disable IPv4 packet RSS hash.

  .. code-block:: console

    testpmd> flow create 0 ingress pattern eth / ipv4 / end actions rss \
             types none end queues end / end

  Set hash function as symmetric Toeplitz.

  .. code-block:: console

    testpmd> flow create 0 ingress pattern end actions rss types end \
             queues end func symmetric_toeplitz / end

  In this case, all packets that enabled RSS are hashed using symmetric
  Toeplitz algorithm.

  Flush all RSS rules

  .. code-block:: console

    testpmd> flow flush 0

  The RSS configurations of hardwre is back to the one ethdev ops set.

Statistics
----------

HNS3 supports various methods to report statistics:

Port statistics can be queried using ``rte_eth_stats_get()``. The number
of packets received or sent successfully by the PMD. While the received and
sent packet bytes are through SW only. The imissed counter is the amount of
packets that could not be delivered to SW because a queue was full. The oerror
counter is the amount of packets that are dropped by HW in Tx.

Extended statistics can be queried using ``rte_eth_xstats_get()``. The extended
statistics expose a wider set of counters counted by the device. The extended
port statistics contains packets statistics per queue, Mac statistics, HW reset
count and IO error count.

Finally per-flow statistics can by queried using ``rte_flow_query`` when attaching
a count action for specific flow. The flow counter counts the number of packets
received successfully by the port and match the specific flow.

Performance tuning
------------------

Hardware configuration
~~~~~~~~~~~~~~~~~~~~~~
32 GB DIMMs is used to ensure that each channel is fully configured.
Dynamic CPU Tuning is disabled.

Queue depth configuration
~~~~~~~~~~~~~~~~~~~~~~~~~
According to the actual test, the performance is best when the queue depth
ranges from 1024 to 2048.

IO burst configuration
~~~~~~~~~~~~~~~~~~~~~~
According to the actual test, the performance is best when IO burst is set to 64.
IO burst is the number of packets per burst.

Queue number configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~
When the number of port queues corresponds to the number of CPU cores, the
performance will be better.

Hugepage configuration
~~~~~~~~~~~~~~~~~~~~~~
For 4K systems, 1 GB hugepages are recommended. For 64 KB systems, 512 MB
hugepages are recommended.

CPU core isolation
~~~~~~~~~~~~~~~~~~
To reduce the possibility of context switching, kernel isolation parameter should
be provided to avoid scheduling the CPU core used by DPDK application threads for
other tasks. Before starting the Linux OS, add the kernel isolation boot parameter.
For example, "isolcpus=1-18 nohz_full=1-18 rcu_nocbs=1-18".


Limitations or Known issues
---------------------------
Currently, we only support VF device driven by DPDK driver when PF is driven
by kernel mode hns3 ethdev driver. VF is not supported when PF is driven by
DPDK driver.

For sake of Rx/Tx performance, IEEE 1588 is not supported when using vec or
sve burst function. When enabling IEEE 1588, Rx/Tx burst mode should be
simple or common. It is recommended that enable IEEE 1588 before ethdev
start. In this way, the correct Rx/Tx burst function can be selected.

Build with ICC is not supported yet.
X86-32, Power8, ARMv7 and BSD are not supported yet.
