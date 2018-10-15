..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

DPDK Release 18.11
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_18_11.html


New Features
------------

.. This section should contain new features added in this release.
   Sample format:

   * **Add a title in the past tense with a full stop.**

     Add a short 1-2 sentence description in the past tense.
     The description should be enough to allow someone scanning
     the release notes to understand the new feature.

     If the feature adds a lot of sub-features you can use a bullet list
     like this:

     * Added feature foo to do something.
     * Enhanced feature bar to do something else.

     Refer to the previous release notes for examples.

     Suggested order in release notes items:
     * Core libs (EAL, mempool, ring, mbuf, buses)
     * Device abstraction libs and PMDs
       - ethdev (lib, PMDs)
       - cryptodev (lib, PMDs)
       - eventdev (lib, PMDs)
       - etc
     * Other libs
     * Apps, Examples, Tools (if significative)

     This section is a comment. Do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =========================================================

* **Added support for using externally allocated memory in DPDK.**

  DPDK has gained support for creating new ``rte_malloc`` heaps referencing
  memory that was created outside of DPDK's own page allocator, and using that
  memory natively with any other DPDK library or data structure.

* **Added hot-unplug handle mechanism.**

  ``rte_dev_hotplug_handle_enable`` and ``rte_dev_hotplug_handle_disable`` are
  for enabling or disabling hotplug handle mechanism.

* **Added new Flow API actions to rewrite fields in packet headers.**

  Added new Flow API actions to:

  * Modify source and destination IP addresses in the outermost IPv4/IPv6
    headers.
  * Modify source and destination port numbers in the outermost TCP/UDP
    headers.

* **Added new Flow API action to swap MAC addresses in Ethernet header.**

  Added new Flow API action to swap the source and destination MAC
  addresses in the outermost Ethernet header.

* **Add support to offload more flow match and actions for CXGBE PMD**

  Flow API support has been enhanced for CXGBE Poll Mode Driver to offload:

  * Match items: destination MAC address.
  * Action items: push/pop/rewrite vlan header,
    rewrite IP addresses in outermost IPv4/IPv6 header,
    rewrite port numbers in outermost TCP/UDP header.

* **Added a devarg to use the latest supported vector path in i40e.**
  A new devarg ``use-latest-supported-vec`` was introduced to allow users to
  choose the latest vector path that the platform supported. For example, users
  can use AVX2 vector path on BDW/HSW to get better performance.

* **Added support for SR-IOV in netvsc PMD.**

  The ``netvsc`` poll mode driver now supports the Accelerated Networking
  SR-IOV option in Hyper-V and Azure. This is an alternative to the previous
  vdev_netvsc, tap, and failsafe drivers combination.

* **Added a new net driver for Marvell Armada 3k device.**

  Added the new ``mvneta`` net driver for Marvell Armada 3k device. See the
  :doc:`../nics/mvneta` NIC guide for more details on this new driver.

* **Added NXP ENETC PMD.**

  Added the new enetc driver for NXP enetc platform. See the
  "ENETC Poll Mode Driver" document for more details on this new driver.

* **Updated Solarflare network PMD.**

  Updated the sfc_efx driver including the following changes:

  * Added support for Rx scatter in EF10 datapath implementation.
  * Added support for Rx descriptor status API in EF10 datapath implementation.
  * Added support for TSO in EF10 datapath implementation.
  * Added support for Tx descriptor status API in EF10 (ef10 and ef10_simple)
    datapaths implementation.

* **Updated failsafe driver.**

  Updated the failsafe driver including the following changes:

  * Support for Rx and Tx queues start and stop.
  * Support for Rx and Tx queues deferred start.
  * Support for runtime Rx and Tx queues setup.
  * Support multicast MAC address set.

* **Added a devarg to use PCAP interface physical MAC address.**
  A new devarg ``phy_mac`` was introduced to allow users to use physical
  MAC address of the selected PCAP interface.

* **Added Event Ethernet Tx Adapter.**

  Added event ethernet Tx adapter library that  provides configuration and
  data path APIs for the ethernet transmit stage of an event driven packet
  processing application. These APIs abstract the implementation of the
  transmit stage and allow the application to use eventdev PMD support or
  a common implementation.

* **Added Distributed Software Eventdev PMD.**

  Added the new Distributed Software Event Device (DSW), which is a
  pure-software eventdev driver distributing the work of scheduling
  among all eventdev ports and the lcores using them. DSW, compared to
  the SW eventdev PMD, sacrifices load balancing performance to
  gain better event scheduling throughput and scalability.

* **Added ability to switch queue deferred start flag on testpmd app.**

  Added a console command to testpmd app, giving ability to switch
  ``rx_deferred_start`` or ``tx_deferred_start`` flag of the specified queue of
  the specified port. The port must be stopped before the command call in order
  to reconfigure queues.

* **Add a new sample for vDPA**

  The vdpa sample application creates vhost-user sockets by using the
  vDPA backend. vDPA stands for vhost Data Path Acceleration which utilizes
  virtio ring compatible devices to serve virtio driver directly to enable
  datapath acceleration. As vDPA driver can help to set up vhost datapath,
  this application doesn't need to launch dedicated worker threads for vhost
  enqueue/dequeue operations.


API Changes
-----------

.. This section should contain API changes. Sample format:

   * Add a short 1-2 sentence description of the API change.
     Use fixed width quotes for ``function_names`` or ``struct_names``.
     Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* eal: ``rte_memseg_list`` structure now has an additional flag indicating
  whether the memseg list is externally allocated. This will have implications
  for any users of memseg-walk-related functions, as they will now have to skip
  externally allocated segments in most cases if the intent is to only iterate
  over internal DPDK memory.
  ``socket_id`` parameter across the entire DPDK has gained additional meaning,
  as some socket ID's will now be representing externally allocated memory. No
  changes will be required for existing code as backwards compatibility will be
  kept, and those who do not use this feature will not see these extra socket
  ID's. Any new API's must not check socket ID parameters themselves, and must
  instead leave it to the memory subsystem to decide whether socket ID is a
  valid one.

* eal: The following devargs functions, which were deprecated in 18.05,
  were removed in 18.11:
  ``rte_eal_parse_devargs_str()``, ``rte_eal_devargs_add()``,
  ``rte_eal_devargs_type_count()``, and ``rte_eal_devargs_dump()``.

* eal: The parameters of the function ``rte_devargs_remove()`` have changed
  from bus and device names to ``struct rte_devargs``.

* mbuf: The ``__rte_mbuf_raw_free()`` and ``__rte_pktmbuf_prefree_seg()``
  functions were deprecated since 17.05 and are replaced by
  ``rte_mbuf_raw_free()`` and ``rte_pktmbuf_prefree_seg()``.

* A new device flag, RTE_ETH_DEV_NOLIVE_MAC_ADDR, changes the order of
  actions inside rte_eth_dev_start regarding MAC set. Some NICs do not
  support MAC changes once the port has started and with this new device
  flag the MAC can be properly configured in any case. This is particularly
  important for bonding.

* The default behaviour of CRC strip offload changed. Without any specific Rx
  offload flag, default behavior by PMD is now to strip CRC.
  DEV_RX_OFFLOAD_CRC_STRIP offload flag has been removed.
  To request keeping CRC, application should set ``DEV_RX_OFFLOAD_KEEP_CRC`` Rx
  offload.

* eventdev: Type of 2nd parameter to ``rte_event_eth_rx_adapter_caps_get()``
  has been changed from uint8_t to uint16_t.


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * Add a short 1-2 sentence description of the ABI change
     that was announced in the previous releases and made in this release.
     Use fixed width quotes for ``function_names`` or ``struct_names``.
     Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* eal: added ``legacy_mem`` and ``single_file_segments`` values to
       ``rte_config`` structure on account of improving DPDK usability when
       using either ``--legacy-mem`` or ``--single-file-segments`` flags.

* eal: EAL library ABI version was changed due to previously announced work on
       supporting external memory in DPDK:
         - structure ``rte_memseg_list`` now has a new field indicating length
           of memory addressed by the segment list
         - structure ``rte_memseg_list`` now has a new flag indicating whether
           the memseg list refers to external memory
         - structure ``rte_malloc_heap`` now has a new field indicating socket
           ID the malloc heap belongs to
         - structure ``rte_mem_config`` has had its ``malloc_heaps`` array
           resized from ``RTE_MAX_NUMA_NODES`` to ``RTE_MAX_HEAPS`` value
         - structure ``rte_malloc_heap`` now has a ``heap_name`` member
         - structure ``rte_eal_memconfig`` has been extended to contain next
           socket ID for externally allocated segments

* eal: The structure ``rte_device`` got a new field to reference a ``rte_bus``.
  It is changing the size of the ``struct rte_device`` and the inherited
  device structures of all buses.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


Shared Library Versions
-----------------------

.. Update any library version updated in this release
   and prepend with a ``+`` sign, like this:

     librte_acl.so.2
   + librte_cfgfile.so.2
     librte_cmdline.so.2

   This section is a comment. Do not overwrite or remove it.
   =========================================================

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
     librte_bbdev.so.1
     librte_bitratestats.so.2
     librte_bpf.so.1
   + librte_bus_dpaa.so.2
   + librte_bus_fslmc.so.2
   + librte_bus_ifpga.so.2
   + librte_bus_pci.so.2
   + librte_bus_vdev.so.2
   + librte_bus_vmbus.so.2
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_common_octeontx.so.1
     librte_compressdev.so.1
     librte_cryptodev.so.5
     librte_distributor.so.1
   + librte_eal.so.9
     librte_ethdev.so.10
   + librte_eventdev.so.6
     librte_flow_classify.so.1
     librte_gro.so.1
     librte_gso.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_latencystats.so.1
     librte_lpm.so.2
     librte_mbuf.so.4
     librte_mempool.so.5
     librte_meter.so.2
     librte_metrics.so.1
     librte_net.so.1
     librte_pci.so.1
     librte_pdump.so.2
     librte_pipeline.so.3
     librte_pmd_bnxt.so.2
     librte_pmd_bond.so.2
     librte_pmd_i40e.so.2
     librte_pmd_ixgbe.so.2
     librte_pmd_dpaa2_cmdif.so.1
     librte_pmd_dpaa2_qdma.so.1
     librte_pmd_ring.so.2
     librte_pmd_softnic.so.1
     librte_pmd_vhost.so.2
   + librte_pmd_netvsc.so.1
     librte_port.so.3
     librte_power.so.1
     librte_rawdev.so.1
     librte_reorder.so.1
     librte_ring.so.2
     librte_sched.so.1
     librte_security.so.1
     librte_table.so.3
     librte_timer.so.1
     librte_vhost.so.3


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue
     in the present tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* When using SR-IOV (VF) support with netvsc PMD and the Mellanox mlx5 bifurcated
  driver; the Linux netvsc device must be brought up before the netvsc device is
  unbound and passed to the DPDK.


Tested Platforms
----------------

.. This section should contain a list of platforms that were tested
   with this release.

   The format is:

   * <vendor> platform with <vendor> <type of devices> combinations

     * List of CPU
     * List of OS
     * List of devices
     * Other relevant details...

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================
