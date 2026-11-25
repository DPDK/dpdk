.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2025 Huawei Technologies Co., Ltd

HINIC3 Poll Mode Driver
=======================

The hinic3 PMD (**librte_net_hinic3**) provides poll mode driver support
for 25Gbps/100Gbps/200Gbps/400Gbps Huawei SP230, SP560 and SP620 series Network Adapters.


Features
--------

- Multi arch support: x86_64, ARMv8.
- Multiple queues for TX and RX
- Receiver Side Scaling (RSS)
- Flow filtering
- Checksum offload
- VLAN stripping and inserting
- TSO offload
- Promiscuous mode
- Port hardware statistics
- Jumbo frames
- Link state information
- Link flow control
- FEC
- Scattered and gather for TX and RX
- Allmulticast mode
- MTU update
- Multicast MAC filter
- NUMA support
- Generic Flow API
- Set Link down or up
- VLAN filter and VLAN offload
- SR-IOV - Partially supported at this point, VFIO only
- FW version
- LRO


Prerequisites
-------------

- Follow the DPDK :doc:`/linux_gsg/index` to setup the basic DPDK environment.


Driver compilation and testing
------------------------------

Refer to the document :doc:`build_and_test` for details.


Runtime Configuration
---------------------

The driver can be configured at initialization via EAL device parameters.
The following parameters are supported:

``tx_pending_limit`` (default ``2``)
   Tx completion coalescing pending limit, in units of 8
   packets.

``tx_coalescing_time`` (default ``2``)
   Tx completion coalescing time, in microseconds.

``rx_cqe_compact_en`` (default ``1``)
   Enable (``1``) or disable (``0``) compact CQE mode. The SP230 reports the
   HW compact CQE feature bit and always uses the compact Rx mode; the SP560
   reports the SW compact CQE feature bit and follows this setting; the SP620
   reports neither.

``rx_cqe_coalesce_num`` (default ``7``)
   Rx CQE coalescing count, in units of 8 packets.

``rx_cqe_timer_loop`` (default ``5``)
   Rx CQE coalescing timer, in microseconds.

The two RX CQE coalescing parameters above apply only to the HW
compact CQE mode; they are ignored with the SW compact and split
CQE modes.


Limitations or Known issues
---------------------------

x86-32, Windows, and BSD are not supported yet.
