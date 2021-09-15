..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

DPDK Release 18.11
==================

New Features
------------

* **Added support for using externally allocated memory in DPDK.**

  DPDK has added support for creating new ``rte_malloc`` heaps referencing
  memory that was created outside of DPDK's own page allocator, and using that
  memory natively with any other DPDK library or data structure.

* **Added check for ensuring allocated memory is addressable by devices.**

  Some devices can have addressing limitations so a new function,
  ``rte_mem_check_dma_mask()``, has been added for checking that allocated
  memory is not out of the device range. Since memory can now be allocated
  dynamically after initialization, a DMA mask is stored and any new allocated
  memory will be checked against it and rejected if it is out of range. If
  more than one device has addressing limitations, the DMA mask is the more
  restrictive one.

* **Updated the C11 memory model version of the ring library.**

  Added changes to decrease latency for architectures using the C11 memory
  model version of the ring library.

  On Cavium ThunderX2 platform, the changes decreased latency by 27-29%
  and 3-15% for MPMC and SPSC cases respectively (with 2 lcores). The
  real improvements may vary with the number of contending lcores and
  the size of the ring.

* **Added hot-unplug handle mechanism.**

  Added ``rte_dev_hotplug_handle_enable()`` and
  ``rte_dev_hotplug_handle_disable()`` for enabling or disabling the hotplug
  handle mechanism.

* **Added support for device multi-process hotplug.**

  Added support for hotplug and hot-unplug in a multiprocessing scenario. Any
  ethdev devices created in the primary process will be regarded as shared and
  will be available for all DPDK processes. Synchronization between processes
  will be done using DPDK IPC.

* **Added new Flow API actions to rewrite fields in packet headers.**

  Added new Flow API actions to:

  * Modify source and destination IP addresses in the outermost IPv4/IPv6
    headers.
  * Modify source and destination port numbers in the outermost TCP/UDP
    headers.

* **Added new Flow API action to swap MAC addresses in Ethernet header.**

  Added new Flow API action to swap the source and destination MAC
  addresses in the outermost Ethernet header.

* **Add support to offload more flow match and actions for CXGBE PMD.**

  The Flow API support has been enhanced for the CXGBE Poll Mode Driver to
  offload:

  * Match items: destination MAC address.
  * Action items: push/pop/rewrite vlan header,
    rewrite IP addresses in outermost IPv4/IPv6 header,
    rewrite port numbers in outermost TCP/UDP header,
    swap MAC addresses in outermost Ethernet header.

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

  Added the new enetc driver for the NXP enetc platform. See the
  :doc:`../nics/enetc` NIC driver guide for more details on this new driver.

* **Added Ethernet poll mode driver for Aquantia aQtion family of 10G devices.**

  Added the new ``atlantic`` ethernet poll mode driver for Aquantia XGBE devices.
  See the :doc:`../nics/atlantic` NIC driver guide for more details on this
  driver.

* **Updated mlx5 driver.**

  Updated the mlx5 driver including the following changes:

  * Improved security of PMD to prevent the NIC from getting stuck when
    the application misbehaves.
  * Reworked flow engine to supported e-switch flow rules (transfer attribute).
  * Added support for header re-write(L2-L4), VXLAN encap/decap, count, match
    on TCP flags and multiple flow groups with e-switch flow rules.
  * Added support for match on metadata, VXLAN and MPLS encap/decap with flow
    rules.
  * Added support for ``RTE_ETH_DEV_CLOSE_REMOVE`` flag to provide better
    support for representors.
  * Added support for meson build.
  * Fixed build issue with PPC.
  * Added support for BlueField VF.
  * Added support for externally allocated static memory for DMA.

* **Updated Solarflare network PMD.**

  Updated the sfc_efx driver including the following changes:

  * Added support for Rx scatter in EF10 datapath implementation.
  * Added support for Rx descriptor status API in EF10 datapath implementation.
  * Added support for TSO in EF10 datapath implementation.
  * Added support for Tx descriptor status API in EF10 (ef10 and ef10_simple)
    datapaths implementation.

* **Updated the enic driver.**

  * Added AVX2-based vectorized Rx handler.
  * Added VLAN and checksum offloads to the simple Tx handler.
  * Added the "count" flow action.
  * Enabled the virtual address IOVA mode.

* **Updated the failsafe driver.**

  Updated the failsafe driver including the following changes:

  * Added support for Rx and Tx queues start and stop.
  * Added support for Rx and Tx queues deferred start.
  * Added support for runtime Rx and Tx queues setup.
  * Added support multicast MAC address set.

* **Added a devarg to use a PCAP interface physical MAC address.**

  A new devarg ``phy_mac`` was introduced to allow users to use the physical
  MAC address of the selected PCAP interface.

* **Added TAP Rx/Tx queues sharing with a secondary process.**

  Added support to allow a secondary process to attach a TAP device created
  in the primary process, probe the queues, and process Rx/Tx in a secondary
  process.

* **Added classification and metering support to SoftNIC PMD.**

  Added support for flow classification (rte_flow API), and metering and
  policing (rte_mtr API) to the SoftNIC PMD.

* **Added Crypto support to the Softnic PMD.**

  The Softnic is now capable of processing symmetric crypto workloads such
  as cipher, cipher-authentication chaining, and AEAD encryption and
  decryption. This is achieved by calling DPDK Cryptodev APIs.

* **Added cryptodev port to port library.**

  Cryptodev port is a shim layer in the port library that interacts with DPDK
  Cryptodev PMDs including burst enqueuing and dequeuing crypto operations.

* **Added symmetric cryptographic actions to the pipeline library.**

  In the pipeline library support was added for symmetric crypto action
  parsing and an action handler was implemented. The action allows automatic
  preparation of the crypto operation with the rules specified such as
  algorithm, key, and IV, etc. for the cryptodev port to process.

* **Updated the AESNI MB PMD.**

  The AESNI MB PMD has been updated with additional support for the AES-GCM
  algorithm.

* **Added NXP CAAM JR PMD.**

  Added the new caam job ring driver for NXP platforms. See the
  :doc:`../cryptodevs/caam_jr` guide for more details on this new driver.

* **Added support for GEN3 devices to Intel QAT driver.**

  Added support for the third generation of Intel QuickAssist devices.

* **Updated the QAT PMD.**

  The QAT PMD was updated with additional support for:

  * The AES-CMAC algorithm.

* **Added support for Dynamic Huffman Encoding to Intel QAT comp PMD.**

  The Intel QuickAssist (QAT) compression PMD has been updated with support
  for Dynamic Huffman Encoding for the Deflate algorithm.

* **Added Event Ethernet Tx Adapter.**

  Added event ethernet Tx adapter library that provides configuration and
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

* **Added extendable bucket feature to hash library (rte_hash).**

  This new "extendable bucket" feature provides 100% insertion guarantee to
  the capacity specified by the user by extending hash table with extra
  buckets when needed to accommodate the unlikely event of intensive hash
  collisions. In addition, the internal hashing algorithm was changed to use
  partial-key hashing to improve memory efficiency and lookup performance.

* **Added lock free reader/writer concurrency to hash library (rte_hash).**

  Lock free reader/writer concurrency prevents the readers from getting
  blocked due to a preempted writer thread. This allows the hash library
  to be used in scenarios where the writer thread runs on the control plane.

* **Added Traffic Pattern Aware Power Control Library.**

  Added an experimental library that extends the Power Library and provides
  empty_poll APIs. This feature measures how many times empty_polls are
  executed per core and uses the number of empty polls as a hint for system
  power management.

  See the :doc:`../prog_guide/power_man` section of the DPDK Programmers
  Guide document for more information.

* **Added JSON power policy interface for containers.**

  Extended the Power Library and vm_power_manager sample app to allow power
  policies to be submitted via a FIFO using JSON formatted strings. Previously
  limited to Virtual Machines, this feature extends power policy functionality
  to containers and host applications that need to have their cores frequency
  controlled based on the rules contained in the policy.

* **Added Telemetry API.**

  Added a new telemetry API which allows applications to transparently expose
  their telemetry in JSON via a UNIX socket. The JSON can be consumed by any
  Service Assurance agent, such as CollectD.

* **Updated KNI kernel module, rte_kni library, and KNI sample application.**

  Updated the KNI kernel module with a new kernel module parameter,
  ``carrier=[on|off]`` to allow the user to control the default carrier
  state of the KNI kernel network interfaces. The default carrier state
  is now set to ``off``, so the interfaces cannot be used until the
  carrier state is set to ``on`` via ``rte_kni_update_link`` or
  by writing ``1`` to ``/sys/devices/virtual/net/<iface>/carrier``.
  In previous versions the default carrier state was left undefined.
  See :doc:`../prog_guide/kernel_nic_interface` for more information.

  Also added the new API function ``rte_kni_update_link()`` to allow the user
  to set the carrier state of the KNI kernel network interface.

  Also added a new command line flag ``-m`` to the KNI sample application to
  monitor and automatically reflect the physical NIC carrier state to the
  KNI kernel network interface with the new ``rte_kni_update_link()`` API.
  See :doc:`../sample_app_ug/kernel_nic_interface` for more information.

* **Added ability to switch queue deferred start flag on testpmd app.**

  Added a console command to testpmd app, giving ability to switch
  ``rx_deferred_start`` or ``tx_deferred_start`` flag of the specified queue of
  the specified port. The port must be stopped before the command call in order
  to reconfigure queues.

* **Add a new sample application for vDPA.**

  The vdpa sample application creates vhost-user sockets by using the
  vDPA backend. vDPA stands for vhost Data Path Acceleration which utilizes
  virtio ring compatible devices to serve virtio driver directly to enable
  datapath acceleration. As vDPA driver can help to set up vhost datapath,
  this application doesn't need to launch dedicated worker threads for vhost
  enqueue/dequeue operations.

* **Added cryptodev FIPS validation example application.**

  Added an example application to parse and perform symmetric cryptography
  computation to the NIST Cryptographic Algorithm Validation Program (CAVP)
  test vectors.

* **Allow unit test binary to take parameters from the environment.**

  The unit test "test", or "dpdk-test", binary is often called from scripts,
  which can make passing additional parameters, such as a coremask,
  difficult. Support has been added to the application to allow it to take
  additional command-line parameter values from the ``DPDK_TEST_PARAMS``
  environment variable to make this application easier to use.


API Changes
-----------

* eal: ``rte_memseg_list`` structure now has an additional flag indicating
  whether the memseg list is externally allocated. This will have implications
  for any users of memseg-walk-related functions, as they will now have to skip
  externally allocated segments in most cases if the intent is to only iterate
  over internal DPDK memory.

  In addition the ``socket_id`` parameter across the entire DPDK has gained
  additional meaning, as some socket ID's will now be representing externally
  allocated memory. No changes will be required for existing code as backwards
  compatibility will be kept, and those who do not use this feature will not
  see these extra socket ID's. Any new API's must not check socket ID
  parameters themselves, and must instead leave it to the memory subsystem to
  decide whether socket ID is a valid one.

* eal: The following devargs functions, which were deprecated in 18.05,
  were removed in 18.11:
  ``rte_eal_parse_devargs_str()``, ``rte_eal_devargs_add()``,
  ``rte_eal_devargs_type_count()``, and ``rte_eal_devargs_dump()``.

* eal: The parameters of the function ``rte_devargs_remove()`` have changed
  from bus and device names to ``struct rte_devargs``.

* eal: The deprecated functions attach/detach were removed in 18.11.
  ``rte_eal_dev_attach`` can be replaced by
  ``rte_dev_probe`` or ``rte_eal_hotplug_add``.
  ``rte_eal_dev_detach`` can be replaced by
  ``rte_dev_remove`` or ``rte_eal_hotplug_remove``.

* eal: The scope of ``rte_eal_hotplug_add()``/``rte_dev_probe()``
  and ``rte_eal_hotplug_remove()``/``rte_dev_remove()`` has been extended.
  In the multi-process model, they will guarantee that the device is
  attached or detached on all processes.

* mbuf: The ``__rte_mbuf_raw_free()`` and ``__rte_pktmbuf_prefree_seg()``
  functions were deprecated since 17.05 and are replaced by
  ``rte_mbuf_raw_free()`` and ``rte_pktmbuf_prefree_seg()``.

* ethdev: The deprecated functions attach/detach were removed in 18.11.
  ``rte_eth_dev_attach()`` can be replaced by ``RTE_ETH_FOREACH_MATCHING_DEV``
  and ``rte_dev_probe()`` or ``rte_eal_hotplug_add()``.
  ``rte_eth_dev_detach()`` can be replaced by
  ``rte_dev_remove()`` or ``rte_eal_hotplug_remove()``.

* ethdev: A call to ``rte_eth_dev_release_port()`` has been added in
  ``rte_eth_dev_close()``. As a consequence, a closed port is freed
  and seen as invalid because of its state ``RTE_ETH_DEV_UNUSED``.
  This new behavior is enabled per driver for a migration period.

* A new device flag, ``RTE_ETH_DEV_NOLIVE_MAC_ADDR``, changes the order of
  actions inside ``rte_eth_dev_start()`` regarding MAC set. Some NICs do not
  support MAC changes once the port has started and with this new device
  flag the MAC can be properly configured in any case. This is particularly
  important for bonding.

* The default behavior of CRC strip offload has changed in this
  release. Without any specific Rx offload flag, default behavior by a PMD is
  now to strip CRC. ``DEV_RX_OFFLOAD_CRC_STRIP`` offload flag has been removed.
  To request keeping CRC, application should set ``DEV_RX_OFFLOAD_KEEP_CRC``
  Rx offload.

* eventdev: The type of the second parameter to
  ``rte_event_eth_rx_adapter_caps_get()`` has been changed from uint8_t to
  uint16_t.

* kni: By default, interface carrier status is ``off`` which means there won't
  be any traffic. It can be set to ``on`` via ``rte_kni_update_link()`` API or
  via ``sysfs`` interface: ``echo 1 > /sys/class/net/vEth0/carrier``.

  Note interface should be ``up`` to be able to read/write sysfs interface.
  When KNI sample application is used, ``-m`` parameter can be used to
  automatically update the carrier status for the interface.

* kni: When ethtool support is enabled (``CONFIG_RTE_KNI_KMOD_ETHTOOL=y``)
  ethtool commands ``ETHTOOL_GSET & ETHTOOL_SSET`` are no longer supported for
  kernels that have ``ETHTOOL_GLINKSETTINGS & ETHTOOL_SLINKSETTINGS`` support.
  This means ``ethtool "-a|--show-pause", "-s|--change"`` won't work, and
  ``ethtool <iface>`` output will have less information.


ABI Changes
-----------

* eal: added ``legacy_mem`` and ``single_file_segments`` values to
  ``rte_config`` structure on account of improving DPDK usability when
  using either ``--legacy-mem`` or ``--single-file-segments`` flags.

* eal: EAL library ABI version was changed due to previously announced work on
  supporting external memory in DPDK:

  - Structure ``rte_memseg_list`` now has a new field indicating length
    of memory addressed by the segment list
  - Structure ``rte_memseg_list`` now has a new flag indicating whether
    the memseg list refers to external memory
  - Structure ``rte_malloc_heap`` now has a new field indicating socket
    ID the malloc heap belongs to
  - Structure ``rte_mem_config`` has had its ``malloc_heaps`` array
    resized from ``RTE_MAX_NUMA_NODES`` to ``RTE_MAX_HEAPS`` value
  - Structure ``rte_malloc_heap`` now has a ``heap_name`` member
  - Structure ``rte_eal_memconfig`` has been extended to contain next
    socket ID for externally allocated segments

* eal: Added ``dma_maskbits`` to ``rte_mem_config`` for keeping the most
  restrictive DMA mask based on the devices addressing limitations.

* eal: The structure ``rte_device`` has a new field to reference a
  ``rte_bus``.  It thus changes the size of the ``struct rte_device`` and the
  inherited device structures of all buses.


Shared Library Versions
-----------------------

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
     librte_compressdev.so.1
     librte_cryptodev.so.5
     librte_distributor.so.1
   + librte_eal.so.9
     librte_efd.so.1
   + librte_ethdev.so.11
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
     librte_member.so.1
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
     librte_pmd_dpaa2_qdma.so.1
     librte_pmd_ring.so.2
     librte_pmd_softnic.so.1
     librte_pmd_vhost.so.2
     librte_port.so.3
     librte_power.so.1
     librte_rawdev.so.1
     librte_reorder.so.1
     librte_ring.so.2
     librte_sched.so.1
     librte_security.so.1
     librte_table.so.3
     librte_timer.so.1
     librte_vhost.so.4


Known Issues
------------

* When using SR-IOV (VF) support with netvsc PMD and the Mellanox mlx5
  bifurcated driver the Linux netvsc device must be brought up before the
  netvsc device is unbound and passed to the DPDK.

* IBM Power8 is not supported in this release of DPDK. IBM Power9 is
  supported.

* ``AVX-512`` support has been disabled for ``GCC`` builds [1] because of a
  crash [2]. This can affect ``native`` machine type build targets on the
  platforms that support ``AVX512F`` like ``Intel Skylake`` processors, and
  can cause a possible performance drop. The immediate workaround is to use
  ``clang`` compiler on these platforms. The issue has been identified as a
  GCC defect and reported to the GCC community [3]. Further actions will be
  taken based on the GCC defect result.

  - [1]: Commit 8d07c82b239f ("mk: disable gcc AVX512F support")
  - [2]: https://bugs.dpdk.org/show_bug.cgi?id=97
  - [3]: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88096


Tested Platforms
----------------

* Intel(R) platforms with Intel(R) NICs combinations

   * CPU

     * Intel(R) Atom(TM) CPU C3758 @ 2.20GHz
     * Intel(R) Xeon(R) CPU D-1541 @ 2.10GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz
     * Intel(R) Xeon(R) CPU E5-2699 v4 @ 2.20GHz
     * Intel(R) Xeon(R) Platinum 8180 CPU @ 2.50GHz

   * OS:

     * CentOS 7.5
     * Fedora 25
     * Fedora 28
     * FreeBSD 11.2
     * Red Hat Enterprise Linux Server release 7.5
     * Open SUSE 15
     * Wind River Linux 8
     * Ubuntu 14.04
     * Ubuntu 16.04
     * Ubuntu 16.10
     * Ubuntu 17.10
     * Ubuntu 18.04

   * NICs:

     * Intel(R) 82599ES 10 Gigabit Ethernet Controller

       * Firmware version: 0x61bf0001
       * Device id (pf/vf): 8086:10fb / 8086:10ed
       * Driver version: 5.2.3 (ixgbe)

     * Intel(R) Corporation Ethernet Connection X552/X557-AT 10GBASE-T

       * Firmware version: 0x800003e7
       * Device id (pf/vf): 8086:15ad / 8086:15a8
       * Driver version: 4.4.6 (ixgbe)

     * Intel(R) Ethernet Converged Network Adapter X710-DA4 (4x10G)

       * Firmware version: 6.01 0x80003221
       * Device id (pf/vf): 8086:1572 / 8086:154c
       * Driver version: 2.4.6 (i40e)

     * Intel(R) Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

       * Firmware version: 3.33 0x80000fd5 0.0.0
       * Device id (pf/vf): 8086:37d0 / 8086:37cd
       * Driver version: 2.4.6 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

       * Firmware version: 6.01 0x80003221
       * Device id (pf/vf): 8086:158b / 8086:154c
       * Driver version: 2.4.6 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

       * Firmware version: 6.01 0x8000321c
       * Device id (pf/vf): 8086:1583 / 8086:154c
       * Driver version: 2.4.6 (i40e)

     * Intel(R) Corporation I350 Gigabit Network Connection

       * Firmware version: 1.63, 0x80000dda
       * Device id (pf/vf): 8086:1521 / 8086:1520
       * Driver version: 5.4.0-k (igb)

* Intel(R) platforms with Mellanox(R) NICs combinations

   * CPU:

     * Intel(R) Xeon(R) Gold 6154 CPU @ 3.00GHz
     * Intel(R) Xeon(R) CPU E5-2697A v4 @ 2.60GHz
     * Intel(R) Xeon(R) CPU E5-2697 v3 @ 2.60GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2650 v4 @ 2.20GHz
     * Intel(R) Xeon(R) CPU E5-2640 @ 2.50GHz
     * Intel(R) Xeon(R) CPU E5-2620 v4 @ 2.10GHz

   * OS:

     * Red Hat Enterprise Linux Server release 7.6 (Maipo)
     * Red Hat Enterprise Linux Server release 7.5 (Maipo)
     * Red Hat Enterprise Linux Server release 7.4 (Maipo)
     * Red Hat Enterprise Linux Server release 7.3 (Maipo)
     * Red Hat Enterprise Linux Server release 7.2 (Maipo)
     * Ubuntu 18.10
     * Ubuntu 18.04
     * Ubuntu 17.10
     * Ubuntu 16.04
     * SUSE Linux Enterprise Server 15

   * MLNX_OFED: 4.4-2.0.1.0
   * MLNX_OFED: 4.5-0.3.1.0

   * NICs:

     * Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1007
       * Firmware version: 2.42.5000

     * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.23.8022 and above
       * Firmware version: 12.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.23.8022 and above

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.23.8022 and above

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.23.8022 and above

     * Mellanox(R) ConnectX(R)-5 Ex EN 100G MCX516A-CDAT (2x100G)

       * Host interface: PCI Express 4.0 x16
       * Device ID: 15b3:1019
       * Firmware version: 16.23.8022 and above

* ARM platforms with Mellanox(R) NICs combinations

   * CPU:

     * Qualcomm ARM 1.1 2500MHz

   * OS:

     * Red Hat Enterprise Linux Server release 7.5 (Maipo)

   * NICs:

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.24.0220

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.24.0220

* Mellanox(R) BlueField SmartNIC

   * Mellanox(R) BlueField SmartNIC MT416842 (2x25G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:a2d2
       * Firmware version: 18.24.0246

   * SoC ARM cores running OS:

     * CentOS Linux release 7.4.1708 (AltArch)
     * MLNX_OFED 4.4-2.5.3.0

  * DPDK application running on ARM cores inside SmartNIC

* ARM SoC combinations from NXP (with integrated NICs)

   * SoC:

     * NXP/Freescale QorIQ LS1046A with ARM Cortex A72
     * NXP/Freescale QorIQ LS2088A with ARM Cortex A72

   * OS:

     * Ubuntu 18.04.1 LTS with NXP QorIQ LSDK 1809 support packages
     * Ubuntu 16.04.3 LTS with NXP QorIQ LSDK 1803 support packages
