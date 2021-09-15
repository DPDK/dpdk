..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2017 The DPDK contributors

DPDK Release 17.11
==================

New Features
------------

* **Extended port_id range from uint8_t to uint16_t.**

  Increased the ``port_id`` range from 8 bits to 16 bits in order to support
  more than 256 ports in DPDK. All ethdev APIs which have ``port_id`` as
  parameter have been changed.

* **Modified the return type of rte_eth_stats_reset.**

  Changed return type of ``rte_eth_stats_reset`` from ``void`` to ``int`` so
  that the caller can determine whether a device supports the operation or not
  and if the operation was carried out.

* **Added a new driver for Marvell Armada 7k/8k devices.**

  Added the new ``mrvl`` net driver for Marvell Armada 7k/8k devices. See the
  :doc:`../nics/mvpp2` NIC guide for more details on this new driver.

* **Updated mlx4 driver.**

  Updated the mlx4 driver including the following changes:

   * Isolated mode (rte_flow) can now be enabled anytime, not only during
     initial device configuration.
   * Flow rules now support up to 4096 priority levels usable at will by
     applications.
   * Enhanced error message to help debugging invalid/unsupported flow rules.
   * Flow rules matching all multicast and promiscuous traffic are now allowed.
   * No more software restrictions on flow rules with the RSS action, their
     configuration is much more flexible.
   * Significantly reduced memory footprint for Rx and Tx queue objects.
   * While supported, UDP RSS is temporarily disabled due to a remaining issue
     with its support in the Linux kernel.
   * The new RSS implementation does not automatically spread traffic according
     to the inner packet of VXLAN frames anymore, only the outer one (like
     other PMDs).
   * Partial (Tx only) support for secondary processes was broken and had to be
     removed.
   * Refactored driver to get rid of dependency on the components provided by
     Mellanox OFED and instead rely on the current and public rdma-core
     package and Linux version from now on.
   * Removed compile-time limitation on number of device instances the PMD
     can support.

* **Updated mlx5 driver.**

  Updated the mlx5 driver including the following changes:

   * Enabled the PMD to run on top of upstream Linux kernel and rdma-core
     libs, removing the dependency on specific Mellanox OFED libraries.
   * Improved PMD latency performance.
   * Improved PMD memory footprint.
   * Added support for vectorized Rx/Tx burst for ARMv8.
   * Added support for secondary process.
   * Added support for flow counters.
   * Added support for Rx hardware timestamp offload.
   * Added support for device removal event.

* **Added SoftNIC PMD.**

  Added a new SoftNIC PMD. This virtual device provides applications with
  software fallback support for traffic management.

* **Added support for NXP DPAA Devices.**

  Added support for NXP's DPAA devices - LS104x series. This includes:

  * DPAA Bus driver
  * DPAA Mempool driver for supporting offloaded packet memory pool
  * DPAA PMD for DPAA devices

  See the :doc:`../nics/dpaa` document for more details of this new driver.

* **Updated support for Cavium OCTEONTX Device.**

  Updated support for Cavium's OCTEONTX device (CN83xx). This includes:

  * OCTEONTX Mempool driver for supporting offloaded packet memory pool
  * OCTEONTX Ethdev PMD
  * OCTEONTX Eventdev-Ethdev Rx adapter

  See the :doc:`../nics/octeontx` document for more details of this new driver.

* **Added PF support to the Netronome NFP PMD.**

  Added PF support to the Netronome NFP PMD. Previously the NFP PMD only
  supported VFs. PF support is just as a basic DPDK port and has no VF
  management yet.

  PF support comes with firmware upload support which allows the PMD to
  independently work from kernel netdev NFP drivers.

  NFP 4000 devices are also now supported along with previous 6000 devices.

* **Updated bnxt PMD.**

  Major enhancements include:

   * Support for Flow API
   * Support for Tx and Rx descriptor status functions

* **Added bus agnostic functions to cryptodev for PMD initialization**

  Added new PMD assist, bus independent, functions
  ``rte_cryptodev_pmd_parse_input_args()``, ``rte_cryptodev_pmd_create()`` and
  ``rte_cryptodev_pmd_destroy()`` for drivers to manage creation and
  destruction of new device instances.

* **Updated QAT crypto PMD.**

  Added several performance enhancements:

  * Removed atomics from the internal queue pair structure.
  * Added coalesce writes to HEAD CSR on response processing.
  * Added coalesce writes to TAIL CSR on request processing.

  In addition support was added for the AES CCM algorithm.

* **Updated the AESNI MB PMD.**

  The AESNI MB PMD has been updated with additional support for:

  * The DES CBC algorithm.
  * The DES DOCSIS BPI algorithm.

  This change requires version 0.47 of the IPsec Multi-buffer library. For
  more details see the :doc:`../cryptodevs/aesni_mb` documentation.

* **Updated the OpenSSL PMD.**

  The OpenSSL PMD has been updated with additional support for:

  * The DES CBC algorithm.
  * The AES CCM algorithm.

* **Added NXP DPAA SEC crypto PMD.**

  A new ``dpaa_sec`` hardware based crypto PMD for NXP DPAA devices has been
  added. See the :doc:`../cryptodevs/dpaa_sec` document for more details.

* **Added MRVL crypto PMD.**

  A new crypto PMD has been added, which provides several ciphering and hashing
  algorithms. All cryptography operations use the MUSDK library crypto API.
  See the :doc:`../cryptodevs/mvsam` document for more details.

* **Add new benchmarking mode to dpdk-test-crypto-perf application.**

  Added a new "PMD cyclecount" benchmark mode to the ``dpdk-test-crypto-perf``
  application to display a detailed breakdown of CPU cycles used by hardware
  acceleration.

* **Added the Security Offload Library.**

  Added an experimental library - ``rte_security``. This provide security APIs
  for protocols like IPsec using inline ipsec offload to ethernet devices or
  full protocol offload with lookaside crypto devices.

  See the :doc:`../prog_guide/rte_security` section of the DPDK Programmers
  Guide document for more information.

* **Updated the DPAA2_SEC crypto driver to support rte_security.**

  Updated the ``dpaa2_sec`` crypto PMD to support ``rte_security`` lookaside
  protocol offload for IPsec.

* **Updated the IXGBE ethernet driver to support rte_security.**

  Updated ixgbe ethernet PMD to support ``rte_security`` inline IPsec offload.

* **Updated i40e driver to support GTP-C/GTP-U.**

  Updated i40e PMD to support GTP-C/GTP-U with GTP-C/GTP-U supporting
  profiles which can be programmed by dynamic device personalization (DDP)
  process.

* **Added the i40e ethernet driver to support queue region feature.**

  This feature enable queue regions configuration for RSS in PF,
  so that different traffic classes or different packet
  classification types can be separated into different queues in
  different queue regions.

* **Updated ipsec-secgw application to support rte_security.**

  Updated the ``ipsec-secgw`` sample application to support ``rte_security``
  actions for ipsec inline and full protocol offload using lookaside crypto
  offload.

* **Added IOMMU support to libvhost-user**

  Implemented device IOTLB in the Vhost-user backend, and enabled Virtio's
  IOMMU feature. The feature is disabled by default, and can be enabled by
  setting ``RTE_VHOST_USER_IOMMU_SUPPORT`` flag at vhost device registration
  time.

* **Added the Event Ethernet Adapter Library.**

  Added the Event Ethernet Adapter library. This library provides APIs for
  eventdev applications to configure the ethdev for eventdev packet flow.

* **Updated DPAA2 Event PMD for the Event Ethernet Adapter.**

  Added support for the eventdev ethernet adapter for DPAA2.

* **Added Membership library (rte_member).**

  Added a new data structure library called the Membership Library.

  The Membership Library is an extension and generalization of a traditional
  filter (for example Bloom Filter) structure that has multiple usages in a
  wide variety of workloads and applications. In general, the Membership
  Library is a data structure that provides a "set-summary" and responds to
  set-membership queries whether a certain member belongs to a set(s).

  The library provides APIs for DPDK applications to insert a new member,
  delete an existing member, and query the existence of a member in a given
  set, or a group of sets. For the case of a group of sets the library will
  return not only whether the element has been inserted in one of the sets but
  also which set it belongs to.

  See the :doc:`../prog_guide/member_lib` documentation in the Programmers
  Guide, for more information.

* **Added the Generic Segmentation Offload Library.**

  Added the Generic Segmentation Offload (GSO) library to enable
  applications to split large packets (e.g. MTU is 64KB) into small
  ones (e.g. MTU is 1500B). Supported packet types are:

  * TCP/IPv4 packets.
  * VxLAN packets, which must have an outer IPv4 header, and contain
    an inner TCP/IPv4 packet.
  * GRE packets, which must contain an outer IPv4 header, and inner
    TCP/IPv4 headers.

  The GSO library doesn't check if the input packets have correct
  checksums, and doesn't update checksums for output packets.
  Additionally, the GSO library doesn't process IP fragmented packets.

* **Added the Flow Classification Library.**

  Added an experimental Flow Classification library to provide APIs for DPDK
  applications to classify an input packet by matching it against a set of
  flow rules. It uses the ``librte_table`` API to manage the flow rules.


Resolved Issues
---------------

* **Service core fails to call service callback due to atomic lock**

  In a specific configuration of multi-thread unsafe services and service
  cores, a service core previously did not correctly release the atomic lock
  on the service. This would result in the cores polling the service, but it
  looked like another thread was executing the service callback. The logic for
  atomic locking of the services has been fixed and refactored for readability.


API Changes
-----------

* **Ethdev device name length increased.**

  The size of internal device name has been increased to 64 characters
  to allow for storing longer bus specific names.

* **Removed the Ethdev RTE_ETH_DEV_DETACHABLE flag.**

  Removed the Ethdev ``RTE_ETH_DEV_DETACHABLE`` flag. This flag is not
  required anymore, with the new hotplug implementation. It has been removed
  from the ether library. Its semantics are now expressed at the bus and PMD
  level.

* **Service cores API updated for usability**

  The service cores API has been changed, removing pointers from the API where
  possible, and instead using integer IDs to identify each service. This
  simplifies application code, aids debugging, and provides better
  encapsulation. A summary of the main changes made is as follows:

  * Services identified by ID not by ``rte_service_spec`` pointer
  * Reduced API surface by using ``set`` functions instead of enable/disable
  * Reworked ``rte_service_register`` to provide the service ID to registrar
  * Reworked start and stop APIs into ``rte_service_runstate_set``
  * Added API to set runstate of service implementation to indicate readiness

* **The following changes have been made in the mempool library**

  * Moved ``flags`` datatype from ``int`` to ``unsigned int`` for
    ``rte_mempool``.
  * Removed ``__rte_unused int flag`` param from ``rte_mempool_generic_put``
    and ``rte_mempool_generic_get`` API.
  * Added ``flags`` param in ``rte_mempool_xmem_size`` and
    ``rte_mempool_xmem_usage``.
  * ``rte_mem_phy2mch`` was used in Xen dom0 to obtain the physical address;
    remove this API as Xen dom0 support was removed.

* **Added IOVA aliases related to physical address handling.**

  Some data types, structure members and functions related to physical address
  handling are deprecated and have new aliases with IOVA wording. For example:

  * ``phys_addr_t`` can be often replaced by ``rte_iova_t`` of same size.
  * ``RTE_BAD_PHYS_ADDR`` is often replaced by ``RTE_BAD_IOVA`` of same value.
  * ``rte_memseg.phys_addr`` is aliased with ``rte_memseg.iova_addr``.
  * ``rte_mem_virt2phy()`` can often be replaced by ``rte_mem_virt2iova``.
  * ``rte_malloc_virt2phy`` is aliased with ``rte_malloc_virt2iova``.
  * ``rte_memzone.phys_addr`` is aliased with ``rte_memzone.iova``.
  * ``rte_mempool_objhdr.physaddr`` is aliased with
    ``rte_mempool_objhdr.iova``.
  * ``rte_mempool_memhdr.phys_addr`` is aliased with
    ``rte_mempool_memhdr.iova``.
  * ``rte_mempool_virt2phy()`` can be replaced by ``rte_mempool_virt2iova()``.
  * ``rte_mempool_populate_phys*()`` are aliased with
    ``rte_mempool_populate_iova*()``
  * ``rte_mbuf.buf_physaddr`` is aliased with ``rte_mbuf.buf_iova``.
  * ``rte_mbuf_data_dma_addr*()`` are aliased with ``rte_mbuf_data_iova*()``.
  * ``rte_pktmbuf_mtophys*`` are aliased with ``rte_pktmbuf_iova*()``.

* **PCI bus API moved outside of the EAL**

  The PCI bus previously implemented within the EAL has been moved.
  A first part has been added as an RTE library providing PCI helpers to
  parse device locations or other such utilities.
  A second part consisting of the actual bus driver has been moved to its
  proper subdirectory, without changing its functionalities.

  As such, several PCI-related functions are not exposed by the EAL anymore:

  * ``rte_pci_detach``
  * ``rte_pci_dump``
  * ``rte_pci_ioport_map``
  * ``rte_pci_ioport_read``
  * ``rte_pci_ioport_unmap``
  * ``rte_pci_ioport_write``
  * ``rte_pci_map_device``
  * ``rte_pci_probe``
  * ``rte_pci_probe_one``
  * ``rte_pci_read_config``
  * ``rte_pci_register``
  * ``rte_pci_scan``
  * ``rte_pci_unmap_device``
  * ``rte_pci_unregister``
  * ``rte_pci_write_config``

  These functions are made available either as part of ``librte_pci`` or
  ``librte_bus_pci``.

* **Moved vdev bus APIs outside of the EAL**

  Moved the following APIs from ``librte_eal`` to ``librte_bus_vdev``:

  * ``rte_vdev_init``
  * ``rte_vdev_register``
  * ``rte_vdev_uninit``
  * ``rte_vdev_unregister``

* **Add return value to stats_get dev op API**

  The ``stats_get`` dev op API return value has been changed to be int.
  In this way PMDs can return an error value in case of failure at stats
  getting process time.

* **Modified the rte_cryptodev_allocate_driver function.**

  Modified the ``rte_cryptodev_allocate_driver()`` function in the cryptodev
  library. An extra parameter ``struct cryptodev_driver *crypto_drv`` has been
  added.

* **Removed virtual device bus specific functions from librte_cryptodev.**

  The functions ``rte_cryptodev_vdev_parse_init_params()`` and
  ``rte_cryptodev_vdev_pmd_init()`` have been removed from librte_cryptodev
  and have been replaced by non bus specific functions
  ``rte_cryptodev_pmd_parse_input_args()`` and ``rte_cryptodev_pmd_create()``.

  The ``rte_cryptodev_create_vdev()`` function was removed to avoid the
  dependency on vdev in librte_cryptodev; instead, users can call
  ``rte_vdev_init()`` directly.

* **Removed PCI device bus specific functions from librte_cryptodev.**

  The functions ``rte_cryptodev_pci_generic_probe()`` and
  ``rte_cryptodev_pci_generic_remove()`` have been removed from librte_cryptodev
  and have been replaced by non bus specific functions
  ``rte_cryptodev_pmd_create()`` and ``rte_cryptodev_pmd_destroy()``.

* **Removed deprecated functions to manage log level or type.**

  The functions ``rte_set_log_level()``, ``rte_get_log_level()``,
  ``rte_set_log_type()`` and ``rte_get_log_type()`` have been removed.

  They are respectively replaced by ``rte_log_set_global_level()``,
  ``rte_log_get_global_level()``, ``rte_log_set_level()`` and
  ``rte_log_get_level()``.

* **Removed mbuf flags PKT_RX_VLAN_PKT and PKT_RX_QINQ_PKT.**

  The ``mbuf`` flags ``PKT_RX_VLAN_PKT`` and ``PKT_RX_QINQ_PKT`` have
  been removed since their behavior was not properly described.

* **Added mbuf flags PKT_RX_VLAN and PKT_RX_QINQ.**

  Two ``mbuf`` flags have been added to indicate that the VLAN
  identifier has been saved in the ``mbuf`` structure. For instance:

  - If VLAN is not stripped and TCI is saved: ``PKT_RX_VLAN``
  - If VLAN is stripped and TCI is saved: ``PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED``

* **Modified the vlan_offload_set_t function prototype in the ethdev library.**

  Modified the ``vlan_offload_set_t`` function prototype in the ethdev
  library.  The return value has been changed from ``void`` to ``int`` so the
  caller can determine whether the backing device supports the operation or if
  the operation was successfully performed.


ABI Changes
-----------

* **Extended port_id range.**

  The size of the field ``port_id`` in the ``rte_eth_dev_data`` structure
  has changed, as described in the `New Features` section above.

* **New parameter added to rte_eth_dev.**

  A new parameter ``security_ctx`` has been added to ``rte_eth_dev`` to
  support security operations like IPsec inline.

* **New parameter added to rte_cryptodev.**

  A new parameter ``security_ctx`` has been added to ``rte_cryptodev`` to
  support security operations like lookaside crypto.


Removed Items
-------------

* Xen dom0 in EAL has been removed, as well as the xenvirt PMD and vhost_xen.

* The crypto performance unit tests have been removed,
  replaced by the ``dpdk-test-crypto-perf`` application.


Shared Library Versions
-----------------------

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
   + librte_bitratestats.so.2
   + librte_bus_dpaa.so.1
   + librte_bus_fslmc.so.1
   + librte_bus_pci.so.1
   + librte_bus_vdev.so.1
     librte_cfgfile.so.2
     librte_cmdline.so.2
   + librte_cryptodev.so.4
     librte_distributor.so.1
   + librte_eal.so.6
   + librte_ethdev.so.8
   + librte_eventdev.so.3
   + librte_flow_classify.so.1
     librte_gro.so.1
   + librte_gso.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_latencystats.so.1
     librte_lpm.so.2
     librte_mbuf.so.3
   + librte_mempool.so.3
     librte_meter.so.1
     librte_metrics.so.1
     librte_net.so.1
   + librte_pci.so.1
   + librte_pdump.so.2
     librte_pipeline.so.3
   + librte_pmd_bnxt.so.2
   + librte_pmd_bond.so.2
   + librte_pmd_i40e.so.2
   + librte_pmd_ixgbe.so.2
     librte_pmd_ring.so.2
   + librte_pmd_softnic.so.1
   + librte_pmd_vhost.so.2
     librte_port.so.3
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
   + librte_security.so.1
   + librte_table.so.3
     librte_timer.so.1
     librte_vhost.so.3


Tested Platforms
----------------

* Intel(R) platforms with Intel(R) NICs combinations

   * CPU

     * Intel(R) Atom(TM) CPU C2758 @ 2.40GHz
     * Intel(R) Xeon(R) CPU D-1540 @ 2.00GHz
     * Intel(R) Xeon(R) CPU D-1541 @ 2.10GHz
     * Intel(R) Xeon(R) CPU E5-4667 v3 @ 2.00GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz
     * Intel(R) Xeon(R) CPU E5-2695 v4 @ 2.10GHz
     * Intel(R) Xeon(R) CPU E5-2658 v2 @ 2.40GHz
     * Intel(R) Xeon(R) CPU E5-2658 v3 @ 2.20GHz

   * OS:

     * CentOS 7.2
     * Fedora 25
     * Fedora 26
     * FreeBSD 11
     * Red Hat Enterprise Linux Server release 7.3
     * SUSE Enterprise Linux 12
     * Wind River Linux 8
     * Ubuntu 16.04
     * Ubuntu 16.10

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

       * Firmware version: 6.01 0x80003205
       * Device id (pf/vf): 8086:1572 / 8086:154c
       * Driver version: 2.1.26 (i40e)

     * Intel(R) Ethernet Converged Network Adapter X710-DA2 (2x10G)

       * Firmware version: 6.01 0x80003204
       * Device id (pf/vf): 8086:1572 / 8086:154c
       * Driver version: 2.1.26 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

       * Firmware version: 6.01 0x80003221
       * Device id (pf/vf): 8086:158b
       * Driver version: 2.1.26 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

       * Firmware version: 6.01 0x8000321c
       * Device id (pf/vf): 8086:1583 / 8086:154c
       * Driver version: 2.1.26 (i40e)

     * Intel(R) Corporation I350 Gigabit Network Connection

       * Firmware version: 1.63, 0x80000dda
       * Device id (pf/vf): 8086:1521 / 8086:1520
       * Driver version: 5.3.0-k (igb)

* Intel(R) platforms with Mellanox(R) NICs combinations

   * Platform details:

     * Intel(R) Xeon(R) CPU E5-2697A v4 @ 2.60GHz
     * Intel(R) Xeon(R) CPU E5-2697 v3 @ 2.60GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2650 v4 @ 2.20GHz
     * Intel(R) Xeon(R) CPU E5-2640 @ 2.50GHz
     * Intel(R) Xeon(R) CPU E5-2620 v4 @ 2.10GHz

   * OS:

     * Red Hat Enterprise Linux Server release 7.3 (Maipo)
     * Red Hat Enterprise Linux Server release 7.2 (Maipo)
     * Ubuntu 16.10
     * Ubuntu 16.04
     * Ubuntu 14.04

   * MLNX_OFED: 4.2-1.0.0.0

   * NICs:

     * Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1007
       * Firmware version: 2.42.5000

     * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000

     * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000

     * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000

     * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000

     * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000

     * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT
       (2x50G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000

     * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000

     * Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.21.1000

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.21.1000

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.21.1000

     * Mellanox(R) ConnectX-5 Ex EN 100G MCX516A-CDAT (2x100G)

       * Host interface: PCI Express 4.0 x16
       * Device ID: 15b3:1019
       * Firmware version: 16.21.1000

* ARM platforms with Mellanox(R) NICs combinations

   * Platform details:

     * Qualcomm ARM 1.1 2500MHz

   * OS:

     * Ubuntu 16.04

   * MLNX_OFED: 4.2-1.0.0.0

   * NICs:

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.21.1000

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.21.1000
