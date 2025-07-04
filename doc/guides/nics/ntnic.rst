..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2024 Napatech A/S

NTNIC Poll Mode Driver
======================

The NTNIC PMD provides poll mode driver support for Napatech smartNICs.


Design
------

The NTNIC PMD is designed as a pure user-space driver,
and requires no special Napatech kernel modules.

The Napatech smartNIC presents one control PCI device (PF0).
NTNIC PMD accesses smartNIC PF0 via vfio-pci kernel driver.
Access to PF0 for all purposes is exclusive,
so only one process should access it.
The physical ports are located behind PF0 as DPDK port 0 and 1.


Supported NICs
--------------

- NT200A02 2x100G SmartNIC

    - FPGA ID 9563 (Inline Flow Management)

- NT400D13 2x100G SmartNIC

    - FPGA ID 9574 (Inline Flow Management)

All information about NT200A02 and NT400D13 can be found by links below:

- https://www.napatech.com/products/nt200a02-smartnic-inline/
- https://www.napatech.com/support/resources/data-sheets/link-inline-software-for-napatech/


Features
--------

- FW version
- Speed capabilities
- Link status (Link update only)
- Unicast MAC filter
- Multicast MAC filter
- Promiscuous mode (Enable only. The device always run promiscuous mode)
- Flow API support.
- Support for multiple rte_flow groups.
- Multiple TX and RX queues.
- Scattered and gather for TX and RX.
- Jumbo frame support.
- Traffic mirroring.
- VLAN filtering.
- Packet modification: NAT, TTL decrement, DSCP tagging
- Tunnel types: GTP.
- Encapsulation and decapsulation of GTP data.
- RX VLAN stripping via raw decap.
- TX VLAN insertion via raw encap.
- CAM and TCAM based matching.
- Exact match of 140 million flows and policies.
- Tunnel HW offload: Packet type, inner/outer RSS, IP and UDP checksum verification.
- RSS hash
- RSS key update
- RSS based on VLAN or 5-tuple.
- RSS using different combinations of fields: L3 only, L4 only or both,
  and source only, destination only or both.
- Several RSS hash keys, one for each flow type.
- Default RSS operation with no hash key specification.
- Port and queue statistics.
- RMON statistics in extended stats.
- Link state information.
- Flow statistics
- Flow aging support
- Flow metering, including meter policy API.
- Flow update. Update of the action list for specific flow
- Asynchronous flow support
- MTU update

Limitations
~~~~~~~~~~~

Linux kernel versions before 5.7 are not supported.
Kernel version 5.7 added vfio-pci support for creating VFs from the PF
which is required for the PMD to use vfio-pci on the PF.
This support has been back-ported to older Linux distributions
and they are also supported.
If vfio-pci is not required, kernel version 4.18 is supported.


Configuration
-------------

Command line arguments
~~~~~~~~~~~~~~~~~~~~~~

Following standard DPDK command line arguments are used by the PMD:

``-a``
   Used to specifically define the NT adapter by PCI ID.

``--iova-mode``
   Must be set to ``pa`` for Physical Address mode.

NTNIC specific arguments can be passed to the PMD in the PCI device parameter list::

   <application> ... -a 0000:03:00.0[{,<NTNIC specific argument>}]

The NTNIC specific argument format is::

   <object>.<attribute>=[<object-ids>:]<value>

Multiple arguments for the same device are separated by ‘,’ comma.
<object-ids> can be a single value or a range.

``rxqs`` parameter [int]

   Specify number of Rx queues to use::

      -a <domain>:<bus>:00.0,rxqs=4,txqs=4

   By default, the value is set to 1.

``txqs`` parameter [int]

   Specify number of Tx queues to use::

      -a <domain>:<bus>:00.0,rxqs=4,txqs=4

   By default, the value is set to 1.


Logging and Debugging
---------------------

NTNIC supports several groups of logging
that can be enabled with ``--log-level`` parameter:

NTNIC
   Logging info from the main PMD code. i.e. code that is related to DPDK::

      --log-level=pmd.net.ntnic.ntnic,8

NTHW
   Logging info from NTHW. i.e. code that is related to the FPGA and the adapter::

      --log-level=pmd.net.ntnic.nthw,8

FILTER
   Logging info from filter. i.e. code that is related to the binary filter::

        --log-level=pmd.net.ntnic.filter,8

To enable logging on all levels use wildcard in the following way::

   --log-level=pmd.net.ntnic.*,8

Flow Scanner
------------

Flow Scanner is DPDK mechanism that constantly and periodically scans
the flow tables to check for aged-out flows.
When flow timeout is reached,
i.e. no packets were matched by the flow within timeout period,
``RTE_ETH_EVENT_FLOW_AGED`` event is reported, and flow is marked as aged-out.

Therefore, flow scanner functionality is closely connected to the flows' age action.

There are list of characteristics that age timeout action has:

- functions only in group > 0;
- flow timeout is specified in seconds;
- flow scanner checks flows age timeout once in 1-480 seconds,
  therefore, flows may not age-out immediately,
  depending on how big are intervals of flow scanner mechanism checks;
- aging counters can display maximum of **n - 1** aged flows
  when aging counters are set to **n**;
- overall 15 different timeouts can be specified for the flows at the same time
  (note that this limit is combined for all actions, therefore,
  15 different actions can be created at the same time,
  maximum limit of 15 can be reached only across different groups -
  when 5 flows with different timeouts are created per one group,
  otherwise the limit within one group is 14 distinct flows);
- after flow is aged-out it's not automatically deleted;
- aged-out flow can be updated with ``flow update`` command,
  and its aged-out status will be reverted;
