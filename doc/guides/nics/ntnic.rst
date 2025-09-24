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

Service API
-----------

**nthw_service_add**
**nthw_service_del**
**nthw_service_get_info**

The NTNIC PMD provides a service API that allows applications to configure services

The services are responsible for handling the vital functionality of the NTNIC PMD:

- **FLM Update**: is responsible for creating and destroying flows;
- **Statistics**: is responsible for collecting statistics;
- **Port event**: is responsible for handling port events: aging, port load, and flow load;
- **Adapter monitor** is responsible for link control;

**NOTE**: Use next EAL options to configure set service cores
   * -s SERVICE COREMASK Hexadecimal bitmask of cores to be used as service cores;
   * -S SERVICE CORELIST List of cores to run services on;

**NOTE**: **At least 5 lcores must be reserved** for the ntnic services by EAL options. above.

For example

.. code-block:: console

   dpdk-testpmd -S 8,9,10,11,12

The PMD registers each service during initialization by function:

.. code-block:: c

   int nthw_service_add(struct rte_service_spec *srv_spec, const enum rte_ntnic_service_tag tag)

and unregistered by the PMD during deinitialization by the function:

.. code-block:: c

   int nthw_service_del(const enum rte_ntnic_service_tag tag)

The service info may be retrieved by function:

.. code-block:: c

   struct nt_service *nthw_service_get_info(const enum rte_ntnic_service_tag tag)

The service info includes the service ID, assigned lcore, and initialization state.

Service API for user applications
---------------------------------
**rte_pmd_ntnic_service_set_lcore**
**rte_pmd_ntnic_service_get_id**

The exported service API is available for applications to configure the services.

By API function:

.. code-block:: c

   int rte_pmd_ntnic_service_set_lcore(enum rte_ntnic_service_tag tag, uint32_t lcore_id)

For example to assign lcores 8,9,10,11,12 to the services, the application can use:

.. code-block:: c

   rte_pmd_ntnic_service_set_lcore(RTE_NTNIC_SERVICE_STAT, 8);
   rte_pmd_ntnic_service_set_lcore(RTE_NTNIC_SERVICE_ADAPTER_MON, 9);
   rte_pmd_ntnic_service_set_lcore(RTE_NTNIC_SERVICE_PORT_0_EVENT, 10);
   rte_pmd_ntnic_service_set_lcore(RTE_NTNIC_SERVICE_PORT_1_EVENT,11);
   rte_pmd_ntnic_service_set_lcore(RTE_NTNIC_SERVICE_FLM_UPDATE, 12);

The API will automatically lcore to service core list and map the service to the lcore.

.. note:: Use `rte_service_lcore_start` to start the lcore after mapping it to the service.

Each service has its own tag to identify it.

.. code-block:: c

   enum rte_ntnic_service_tag {
      RTE_NTNIC_SERVICE_FLM_UPDATE = 0,
      RTE_NTNIC_SERVICE_STAT = 1,
      RTE_NTNIC_SERVICE_PORT_0_EVENT = 2,
      RTE_NTNIC_SERVICE_PORT_1_EVENT = 3,
      RTE_NTNIC_SERVICE_ADAPTER_MON = 4,
      RTE_NTNIC_SERVICE_MAX
   };

The application may use next API function to retrieve the service id:

.. code-block:: c

  int rte_pmd_ntnic_service_get_id(enum rte_ntnic_service_tag tag);


For example, to enable statistics for flm_update service, the application can use:

.. code-block:: c

   int flm_update_id = rte_pmd_ntnic_service_get_id(RTE_NTNIC_SERVICE_FLM_UPDATE);
   rte_service_set_stats_enable(flm_update_id, 1);

All other manipulations with the service can be done with the service ID and rte_service* API.

To use the service API, an application must have included the header file:

.. code-block:: c

   #include <rte_pmd_ntnic.h>

And linked with the library: `librte_net_ntnic.so` or `librte_net_ntnic.a` for static linking.
