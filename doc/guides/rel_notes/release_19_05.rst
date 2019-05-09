..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 The DPDK contributors

DPDK Release 19.05
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_19_05.html


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
     * Apps, Examples, Tools (if significant)

     This section is a comment. Do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =========================================================

* **Added new armv8 machine targets:**

  * BlueField (Mellanox)
  * OcteonTX2 (Marvell)
  * ThunderX2 (Marvell)

* **Introduced Windows Support.**

  Added Windows support to build Hello World sample application.

* **Added Stack API.**

  Added a new stack API for configuration and use of a bounded stack of
  pointers. The API provides MT-safe push and pop operations that can operate
  on one or more pointers per operation.

  The library supports two stack implementations: standard (lock-based) and lock-free.
  The lock-free implementation is currently limited to x86-64 platforms.

* **Added Lock-Free Stack Mempool Handler.**

  Added a new lock-free stack handler, which uses the newly added stack
  library.

* **Added RCU library.**

  Added RCU library supporting quiescent state based memory reclamation method.
  This library helps identify the quiescent state of the reader threads so
  that the writers can free the memory associated with the lock free data
  structures.

* **Updated KNI module and PMD.**

  Updated the KNI kernel module to set the max_mtu according to the given
  initial MTU size. Without it, the maximum MTU was 1500.

  Updated the KNI PMD driver to set the mbuf_size and MTU based on
  the given mb-pool. This provide the ability to pass jumbo frames
  if the mb-pool contains suitable buffers' size.

* **Added the AF_XDP PMD.**

  Added a Linux-specific PMD driver for AF_XDP, it can create the AF_XDP socket
  and bind it to a specific netdev queue, it allows a DPDK application to send
  and receive raw packets through the socket which would bypass the kernel
  network stack to achieve high performance packet processing.

* **Added a net PMD NFB.**

  Added the new ``nfb`` net driver for Netcope NFB cards. See
  the :doc:`../nics/nfb` NIC guide for more details on this new driver.

* **Added IPN3KE net PMD.**

  Added the new ``ipn3ke`` net driver for Intel® FPGA PAC(Programmable
  Acceleration Card) N3000. See the :doc:`../nics/ipn3ke` NIC guide for more
  details on this new driver.

  Aside from this, ifpga_rawdev is also updated to support Intel® FPGA PAC
  N3000 with SPI interface access, I2C Read/Write and Ethernet PHY configuration.

* **Updated Solarflare network PMD.**

  Updated the sfc_efx driver including the following changes:

  * Added support for Rx descriptor status and related API in a secondary
    process.
  * Added support for Tx descriptor status API in a secondary process.
  * Added support for RSS RETA and hash configuration get API in a secondary
    process.
  * Added support for Rx packet types list in a secondary process.
  * Added Tx prepare to do Tx offloads checks.
  * Added support for VXLAN and GENEVE encapsulated TSO.

* **Updated Mellanox mlx4 driver.**

   New features and improvements were done:

   * Added firmware version reading.
   * Added support for secondary process.
   * Added support of per-process device registers, reserving identical VA space
     is not needed anymore.
   * Added support for multicast address list interface

* **Updated Mellanox mlx5 driver.**

   New features and improvements were done:

   * Added firmware version reading.
   * Added support of new naming scheme of representor.
   * Added support for new PCI device DMA map/unmap API.
   * Added support for multiport InfiniBand device.
   * Added control of excessive memory pinning by kernel.
   * Added support of DMA memory registration by secondary process.
   * Added Direct Rule support in Direct Verbs flow driver.
   * Added support of per-process device registers, reserving identical VA space
     is not needed anymore.
   * Added E-Switch support in Direct Verbs flow driver.

* **Renamed avf to iavf.**

  Renamed Intel Ethernet Adaptive Virtual Function driver ``avf`` to ``iavf``,
  which includes the directory name, lib name, filenames, makefile, docs,
  macros, functions, structs and any other strings in the code.

* **Updated the enic driver.**

  * Fixed several flow (director) bugs related to MARK, SCTP, VLAN, VXLAN, and
    inner packet matching.
  * Added limited support for RAW.
  * Added limited support for RSS.
  * Added limited support for PASSTHRU.

* **Updated the ixgbe driver.**

  New features for VF:

  * Added promiscuous mode support.

* **Updated the ice driver.**

  * Added support of SSE and AVX2 instructions in Rx and Tx paths.
  * Added package download support.
  * Added Safe Mode support.
  * Supported RSS for UPD/TCP/SCTP+IPV4/IPV6 packets.

* **Updated the i40e driver.**

  New features for PF:

  * Added support for VXLAN-GPE packet.
  * Added support for VXLAN-GPE classification.

* **Updated the ENETC driver.**

  New features:

  * Added physical addressing mode support
  * Added SXGMII interface support
  * Added basic statistics support
  * Added promiscuous and allmulticast mode support
  * Added MTU update support
  * Added jumbo frame support
  * Added queue start/stop
  * Added CRC offload support
  * Added Rx checksum offload validation support

* **Updated the atlantic PMD.**

  Added MACSEC hardware offload experimental API.

* **Updated the Intel QuickAssist Technology (QAT) compression PMD.**

  Simplified and made more robust QAT compressdev PMD's handling of SGLs with
  more than 16 segments.

* **Updated the QuickAssist Technology (QAT) symmetric crypto PMD.**

  Added support for AES-XTS with 128 and 256 bit AES keys.

* **Added Intel QuickAssist Technology PMD for asymmetric crypto.**

  A new QAT Crypto PMD has been added, which provides asymmetric cryptography
  algorithms, in this release modular exponentiation and modular multiplicative
  inverse algorithms were added.

* **Updated AESNI-MB PMD.**

  Added support for out-of-place operations.

* **Updated the IPsec library.**

  The IPsec library has been updated with AES-CTR and 3DES-CBC cipher algorithms
  support. The related ipsec-secgw test scripts have been added.

* **Updated the testpmd application.**

  Improved testpmd application performance on ARM platform. For ``macswap``
  forwarding mode, NEON intrinsics were used to do swap to save CPU cycles.

* **Updated power management library.**

  Added support for Intel Speed Select Technology - Base Frequency (SST-BF).
  ``rte_power_get_capabilities`` now has a bit in it's returned mask
  indicating it's a high frequency core.

* **Updated distributor sample application.**

  Added support for Intel SST-BF feature so that the distributor core is
  pinned to a high frequency core if available.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


API Changes
-----------

.. This section should contain API changes. Sample format:

   * sample: Add a short 1-2 sentence description of the API change
     which was announced in the previous releases and made in this release.
     Start with a scope label like "ethdev:".
     Use fixed width quotes for ``function_names`` or ``struct_names``.
     Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* eal: the type of the ``attr_value`` parameter of the function
  ``rte_service_attr_get()`` has been changed
  from ``uint32_t *`` to ``uint64_t *``.

* meter: replace ``enum rte_meter_color`` in meter library with new
  ``rte_color`` definition added in 19.02. To consolidate mulitple color
  definitions replicated at many places such as: rte_mtr.h, rte_tm.h,
  replacements with rte_color values are done.

* vfio: Functions ``rte_vfio_container_dma_map`` and
  ``rte_vfio_container_dma_unmap`` have been extended with an option to
  request mapping or un-mapping to the default vfio container fd.

* power: ``rte_power_set_env`` and ``rte_power_unset_env`` functions
  have been modified to be thread safe.

* timer: Functions have been introduced that allow multiple instances of the
  timer lists to be created, and they are now allocated in shared memory. New
  functions allow particular timer lists to be selected when timers are being
  started, stopped, and managed.


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * sample: Add a short 1-2 sentence description of the ABI change
     which was announced in the previous releases and made in this release.
     Start with a scope label like "ethdev:".
     Use fixed width quotes for ``function_names`` or ``struct_names``.
     Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* ethdev: Additional fields in rte_eth_dev_info.

  The ``rte_eth_dev_info`` structure has had two extra fields
  added: ``min_mtu`` and ``max_mtu``. Each of these are of type ``uint16_t``.
  The values of these fields can be set specifically by the PMD drivers as
  supported values can vary from device to device.

* cryptodev: in 18.08 new structure ``rte_crypto_asym_op`` was introduced and
  included into ``rte_crypto_op``. As ``rte_crypto_asym_op`` structure was
  defined as cache-line aligned that caused unintended changes in
  ``rte_crypto_op`` structure layout and alignment. Remove cache-line
  alignment for ``rte_crypto_asym_op`` to restore expected ``rte_crypto_op``
  layout and alignment.

* timer: ``rte_timer_subsystem_init`` now returns success or failure to reflect
  whether it was able to allocate memory.


Shared Library Versions
-----------------------

.. Update any library version updated in this release
   and prepend with a ``+`` sign, like this:

     libfoo.so.1
   + libupdated.so.2
     libbar.so.1

   This section is a comment. Do not overwrite or remove it.
   =========================================================

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
   + librte_cryptodev.so.7
     librte_distributor.so.1
   + librte_eal.so.10
     librte_efd.so.1
   + librte_ethdev.so.12
     librte_eventdev.so.6
     librte_flow_classify.so.1
     librte_gro.so.1
     librte_gso.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_ipsec.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_latencystats.so.1
     librte_lpm.so.2
     librte_mbuf.so.5
     librte_member.so.1
     librte_mempool.so.5
     librte_meter.so.3
     librte_metrics.so.1
     librte_net.so.1
     librte_pci.so.1
     librte_pdump.so.3
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
   + librte_rcu.so.1
     librte_reorder.so.1
     librte_ring.so.2
     librte_sched.so.2
     librte_security.so.2
   + librte_stack.so.1
     librte_table.so.3
     librte_timer.so.1
     librte_vhost.so.4


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue
     in the present tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **On x86 platforms, AVX512 support is disabled with binutils 2.31**

  Because a defect in binutils 2.31 AVX512 support is disabled.
  DPDK defect: https://bugs.dpdk.org/show_bug.cgi?id=249
  GCC defect: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90028

* **No software AES-XTS implementation.**

  There are currently no cryptodev software PMDs available which implement
  support for the AES-XTS algorithm, so this feature can only be used
  if compatible hardware and an associated PMD is available.


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
    * Ubuntu 19.04
    * Ubuntu 18.10
    * Ubuntu 18.04
    * Ubuntu 16.04
    * SUSE Linux Enterprise Server 15

  * MLNX_OFED: 4.5-1.0.1.0
  * MLNX_OFED: 4.6-1.0.1.1

  * NICs:

    * Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.25.1020 and above
      * Firmware version: 12.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.25.1020 and above

    * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.25.1020 and above

    * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.25.1020 and above

    * Mellanox(R) ConnectX(R)-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.25.1020 and above

* Arm platforms with Mellanox(R) NICs combinations

  * CPU:

    * Qualcomm Arm 1.1 2500MHz

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
    * Firmware version: 18.25.1010

  * SoC Arm cores running OS:

    * CentOS Linux release 7.4.1708 (AltArch)
    * MLNX_OFED 4.6-1.0.0.0

  * DPDK application running on Arm cores inside SmartNIC

* IBM Power 9 platforms with Mellanox(R) NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202) 2300MHz

  * OS:

    * Ubuntu 18.04.1 LTS (Bionic Beaver)

  * NICs:

    * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.24.1000

  * OFED:

    * MLNX_OFED_LINUX-4.6-1.0.1.0
