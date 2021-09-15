..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

DPDK Release 19.02
==================

New Features
------------

* **Added support for freeing hugepages exactly as originally allocated.**

  Some applications using memory event callbacks (especially for managing
  RDMA memory regions) require that memory be freed back to the system
  exactly as it was originally allocated. These applications typically
  also require that a malloc allocation not span across two separate
  hugepage allocations.  A new ``--match-allocations`` EAL init flag has
  been added to fulfill both of these requirements.

* **Added API to register external memory in DPDK.**

  A new ``rte_extmem_register``/``rte_extmem_unregister`` API was added to allow
  chunks of external memory to be registered with DPDK without adding them to
  the malloc heap.

* **Added support for using virtio-user without hugepages.**

  The ``--no-huge`` mode was augmented to use memfd-backed memory (on systems
  that support memfd), to allow using virtio-user-based NICs without
  hugepages.

* **Release of the ENA PMD v2.0.0.**

  Version 2.0.0 of the ENA PMD was added with the following additions:

  * Added Low Latency Queue v2 (LLQv2). This feature reduces the latency
    of the packets by pushing the header directly through the PCI to the
    device. This allows the NIC to start handle packets right after the doorbell
    without waiting for DMA.
  * Added independent configuration of HW Tx and Rx ring depths.
  * Added support for up to 8k Rx descriptors per ring.
  * Added additional doorbell check on Tx, to handle Tx more efficiently for big
    bursts of packets.
  * Added per queue statistics.
  * Added extended statistics using xstats DPDK API.
  * The reset routine was aligned with the DPDK API, so now it can be
    handled as in other PMDs.
  * Fixed out of order (OOO) completion.
  * Fixed memory leaks due to port stops and starts in the middle of
    traffic.
  * Updated documentation and features list of the PMD.

* **Updated mlx5 driver.**

  Updated the mlx5 driver including the following changes:

  * Fixed ``imissed`` counter to be reported through ``rte_eth_stats`` instead
    of ``rte_eth_xstats``.
  * Added packet header modification through Direct Verbs flow driver.
  * Added ConnectX-6 PCI device ID to be proved by ``mlx5`` driver.
  * Added flow counter support to Direct Verbs flow driver though DevX.
  * Renamed build options for the glue layer to
    ``CONFIG_RTE_IBVERBS_LINK_DLOPEN`` for make and ``ibverbs_link`` for meson.
  * Added static linkage of ``mlx`` dependency.
  * Improved stability of E-Switch flow driver.
  * Added new make build configuration to set the cacheline size for BlueField
    correctly - ``arm64-bluefield-linux-gcc``.

* **Updated the enic driver.**

  * Added support for the ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.
  * Added a handler to get the firmware version string.
  * Added support for multicast filtering.

* **Added dynamic queues allocation support for i40e VF.**

  Previously, the available VF queues were reserved by PF at initialization
  stage. Now both DPDK PF and Kernel PF (>=2.1.14) will support dynamic queue
  allocation. At runtime, when VF requests for more queue exceed the initial
  reserved amount, the PF can allocate up to 16 queues as the request after a
  VF reset.

* **Added ICE net PMD.**

  Added the new ``ice`` net driver for Intel(R) Ethernet Network Adapters E810.
  See the :doc:`../nics/ice` NIC guide for more details on this new driver.

* **Added support for SW-assisted VDPA live migration.**

  This SW-assisted VDPA live migration facility helps VDPA devices without
  logging capability to perform live migration, a mediated SW relay can help
  devices to track dirty pages caused by DMA. the IFC driver has enabled this
  SW-assisted live migration mode.

* **Added security checks to the cryptodev symmetric session operations.**

  Added a set of security checks to the access cryptodev symmetric session.
  The checks include the session's user data read/write check and the
  session private data referencing status check while freeing a session.

* **Updated the AESNI-MB PMD.**

  * Added support for intel-ipsec-mb version 0.52.
  * Added AES-GMAC algorithm support.
  * Added Plain SHA1, SHA224, SHA256, SHA384, and SHA512 algorithms support.

* **Added IPsec Library.**

  Added an experimental library ``librte_ipsec`` to provide ESP tunnel and
  transport support for IPv4 and IPv6 packets.

  The library provides support for AES-CBC ciphering and AES-CBC with HMAC-SHA1
  algorithm-chaining, and AES-GCM and NULL algorithms only at present. It is
  planned to add more algorithms in future releases.

  See :doc:`../prog_guide/ipsec_lib` for more information.

* **Updated the ipsec-secgw sample application.**

  The ``ipsec-secgw`` sample application has been updated to use the new
  ``librte_ipsec`` library, which has also been added in this release.
  The original functionality of ipsec-secgw is retained, a new command line
  parameter ``-l`` has  been added to ipsec-secgw to use the IPsec library,
  instead of the existing IPsec code in the application.

  The IPsec library does not support all the functionality of the existing
  ipsec-secgw application. It is planned to add the outstanding functionality
  in future releases.

  See :doc:`../sample_app_ug/ipsec_secgw` for more information.

* **Enabled checksum support in the ISA-L compressdev driver.**

  Added support for both adler and crc32 checksums in the ISA-L PMD.
  This aids data integrity across both compression and decompression.

* **Added a compression performance test tool.**

  Added a new performance test tool to test the compressdev PMD. The tool tests
  compression ratio and compression throughput.

* **Added intel_pstate support to Power Management library.**

  Previously, using the power management library required the
  disabling of the intel_pstate kernel driver, and the enabling of the
  acpi_cpufreq kernel driver. This is no longer the case, as the use of
  the intel_pstate kernel driver is now supported, and automatically
  detected by the library.


API Changes
-----------

* eal: Function ``rte_bsf64`` in ``rte_bitmap.h`` has been renamed to
  ``rte_bsf64_safe`` and moved to ``rte_common.h``. A new ``rte_bsf64``
  function has been added in ``rte_common.h`` that follows the convention set
  by the existing ``rte_bsf32`` function.

* eal: Segment fd API on Linux now sets error code to ``ENOTSUP`` in more cases
  where segment the fd API is not expected to be supported:

  - On attempt to get a segment fd for an externally allocated memory segment
  - In cases where memfd support would have been required to provide segment
    fds (such as in-memory or no-huge mode)

* eal: Functions ``rte_malloc_dump_stats()``, ``rte_malloc_dump_heaps()`` and
  ``rte_malloc_get_socket_stats()`` are no longer safe to call concurrently with
  ``rte_malloc_heap_create()`` or ``rte_malloc_heap_destroy()`` function calls.

* mbuf: ``RTE_MBUF_INDIRECT()``, which was deprecated in 18.05, was replaced
  with ``RTE_MBUF_CLONED()`` and removed in 19.02.

* sched: As result of the new format of the mbuf sched field, the
  functions ``rte_sched_port_pkt_write()`` and
  ``rte_sched_port_pkt_read_tree_path()`` got an additional parameter of
  type ``struct rte_sched_port``.

* pdump: The ``rte_pdump_set_socket_dir()``, the parameter ``path`` of
  ``rte_pdump_init()`` and enum ``rte_pdump_socktype`` were deprecated
  since 18.05 and are removed in this release.

* cryptodev: The parameter ``session_pool`` in the function
  ``rte_cryptodev_queue_pair_setup()`` is removed.

* cryptodev: a new function ``rte_cryptodev_sym_session_pool_create()`` has been
  introduced. This function is now mandatory when creating symmetric session
  header mempool. Please note all crypto applications are required to use this
  function from now on. Failed to do so will cause a
  ``rte_cryptodev_sym_session_create()`` function call return error.


ABI Changes
-----------

* mbuf: The format of the sched field of ``rte_mbuf`` has been changed
  to include the following fields: ``queue ID``, ``traffic class``, ``color``.

* cryptodev: as shown in the 18.11 deprecation notice, the structure
  ``rte_cryptodev_qp_conf`` has added two parameters for symmetric session
  mempool and symmetric session private data mempool.

* cryptodev: as shown in the 18.11 deprecation notice, the structure
  ``rte_cryptodev_sym_session`` has been updated to contain more information
  to ensure safely accessing the session and session private data.

* security: A new field ``uint64_t opaque_data`` has been added to
  ``rte_security_session`` structure. That would allow upper layer to easily
  associate/de-associate some user defined data with the security session.


Shared Library Versions
-----------------------

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
     librte_bbdev.so.1
     librte_bitratestats.so.2
     librte_bpf.so.1
     librte_bus_dpaa.so.2
     librte_bus_fslmc.so.2
     librte_bus_ifpga.so.2
     librte_bus_pci.so.2
     librte_bus_vdev.so.2
     librte_bus_vmbus.so.2
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_compressdev.so.1
   + librte_cryptodev.so.6
     librte_distributor.so.1
     librte_eal.so.9
     librte_efd.so.1
     librte_ethdev.so.11
     librte_eventdev.so.6
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
   + librte_mbuf.so.5
     librte_member.so.1
     librte_mempool.so.5
     librte_meter.so.2
     librte_metrics.so.1
     librte_net.so.1
     librte_pci.so.1
   + librte_pdump.so.3
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
   + librte_sched.so.2
   + librte_security.so.2
     librte_table.so.3
     librte_timer.so.1
     librte_vhost.so.4


Known Issues
------------

* ``AVX-512`` support has been disabled for ``GCC`` builds when ``binutils 2.30``
  is detected [1] because of a crash [2]. This can affect ``native`` machine type
  build targets on the platforms that support ``AVX512F`` like ``Intel Skylake``
  processors, and can cause a possible performance drop. The immediate workaround
  is to use ``clang`` compiler on these platforms.
  Initial workaround in DPDK v18.11 was to disable ``AVX-512`` support for ``GCC``
  completely, but based on information on defect submitted to GCC community [3],
  issue has been identified as ``binutils 2.30`` issue. Since currently only GCC
  generates ``AVX-512`` instructions, the scope is limited to ``GCC`` and
  ``binutils 2.30``

  - [1]: Commit ("mk: fix scope of disabling AVX512F support")
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
     * Intel(R) Xeon(R) Gold 6139 CPU @ 2.30GHz

   * OS:

     * CentOS 7.4
     * CentOS 7.5
     * Fedora 25
     * Fedora 28
     * FreeBSD 11.2
     * FreeBSD 12.0
     * Red Hat Enterprise Linux Server release 7.4
     * Red Hat Enterprise Linux Server release 7.5
     * Open SUSE 15
     * Wind River Linux 8
     * Ubuntu 14.04
     * Ubuntu 16.04
     * Ubuntu 16.10
     * Ubuntu 18.04
     * Ubuntu 18.10

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

       * Firmware version: 6.80 0x80003cc1
       * Device id (pf/vf): 8086:1572 / 8086:154c
       * Driver version: 2.7.26 (i40e)

     * Intel(R) Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

       * Firmware version: 3.33 0x80000fd5 0.0.0
       * Device id (pf/vf): 8086:37d0 / 8086:37cd
       * Driver version: 2.7.26 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

       * Firmware version: 6.80 0x80003d05
       * Device id (pf/vf): 8086:158b / 8086:154c
       * Driver version: 2.7.26 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

       * Firmware version: 6.80 0x80003cfb
       * Device id (pf/vf): 8086:1583 / 8086:154c
       * Driver version: 2.7.26 (i40e)

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
   * MLNX_OFED: 4.5-1.0.1.0

   * NICs:

     * Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1007
       * Firmware version: 2.42.5000

     * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.24.1000 and above
       * Firmware version: 12.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.24.1000 and above

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.24.1000 and above

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.24.1000 and above

     * Mellanox(R) ConnectX(R)-5 Ex EN 100G MCX516A-CDAT (2x100G)

       * Host interface: PCI Express 4.0 x16
       * Device ID: 15b3:1019
       * Firmware version: 16.24.1000 and above

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
       * Firmware version: 18.24.0328

   * SoC ARM cores running OS:

     * CentOS Linux release 7.4.1708 (AltArch)
     * MLNX_OFED 4.4-2.5.9.0

  * DPDK application running on ARM cores inside SmartNIC

* Power 9 platforms with Mellanox(R) NICs combinations

   * CPU:

     * POWER9 2.2 (pvr 004e 1202) 2300MHz

   * OS:

     * Ubuntu 18.04.1 LTS (Bionic Beaver)

   * NICs:

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.23.1020

   * OFED:

      * MLNX_OFED_LINUX-4.5-1.0.1.0
