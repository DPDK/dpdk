..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 The DPDK contributors

DPDK Release 19.05
==================

New Features
------------

* **Added new armv8 machine targets.**

  Added new armv8 machine targets:

  * BlueField (Mellanox)
  * OcteonTX2 (Marvell)
  * ThunderX2 (Marvell)

* **Added Windows Support.**

  Added Windows support to build Hello World sample application.

* **Added Stack Library.**

  Added a new stack library and APIs for configuration and use of a bounded
  stack of pointers. The API provides multi-thread safe push and pop
  operations that can operate on one or more pointers per operation.

  The library supports two stack implementations: standard (lock-based) and
  lock-free.  The lock-free implementation is currently limited to x86-64
  platforms.

* **Added Lock-Free Stack Mempool Handler.**

  Added a new lock-free stack handler, which uses the newly added stack
  library.

* **Added RCU library.**

  Added RCU library supporting a quiescent state based memory reclamation method.
  This library helps identify the quiescent state of the reader threads so
  that the writers can free the memory associated with the lock free data
  structures.

* **Updated KNI module and PMD.**

  Updated the KNI kernel module to set the ``max_mtu`` according to the given
  initial MTU size. Without it, the maximum MTU was 1500.

  Updated the KNI PMD to set the ``mbuf_size`` and MTU based on
  the given mb-pool. This provide the ability to pass jumbo frames
  if the mb-pool contains a suitable buffer size.

* **Added the AF_XDP PMD.**

  Added a Linux-specific PMD for AF_XDP. This PMD can create an AF_XDP socket
  and bind it to a specific netdev queue. It allows a DPDK application to send
  and receive raw packets through the socket which would bypass the kernel
  network stack to achieve high performance packet processing.

* **Added a net PMD NFB.**

  Added the new ``nfb`` net driver for Netcope NFB cards. See
  the :doc:`../nics/nfb` NIC guide for more details on this new driver.

* **Added IPN3KE net PMD.**

  Added the new ``ipn3ke`` net driver for the Intel® FPGA PAC (Programmable
  Acceleration Card) N3000. See the :doc:`../nics/ipn3ke` NIC guide for more
  details on this new driver.

  In addition ``ifpga_rawdev`` was also updated to support Intel® FPGA PAC
  N3000 with SPI interface access, I2C Read/Write, and Ethernet PHY configuration.

* **Updated Solarflare network PMD.**

  Updated the Solarflare ``sfc_efx`` driver with changes including:

  * Added support for Rx descriptor status and related API in a secondary
    process.
  * Added support for Tx descriptor status API in a secondary process.
  * Added support for RSS RETA and hash configuration reading API in a
    secondary process.
  * Added support for Rx packet types list in a secondary process.
  * Added Tx prepare to do Tx offloads checks.
  * Added support for VXLAN and GENEVE encapsulated TSO.

* **Updated Mellanox mlx4 driver.**

   Updated Mellanox mlx4 driver with new features and improvements, including:

   * Added firmware version reading.
   * Added support for secondary processes.
   * Added support of per-process device registers. Reserving identical VA space
     is not needed anymore.
   * Added support for multicast address list interfaces.

* **Updated Mellanox mlx5 driver.**

   Updated Mellanox mlx5 driver with new features and improvements, including:

   * Added firmware version reading.
   * Added support for new naming scheme of representor.
   * Added support for new PCI device DMA map/unmap API.
   * Added support for multiport InfiniBand device.
   * Added control of excessive memory pinning by kernel.
   * Added support of DMA memory registration by secondary process.
   * Added support of per-process device registers. Reserving identical VA space
     is not required anymore.
   * Added support for jump action for both E-Switch and NIC.
   * Added Support for multiple rte_flow groups in NIC steering.
   * Flow engine re-designed to support large scale deployments. this includes:
      * Support millions of offloaded flow rules.
      * Fast flow insertion and deletion up to 1M flow update per second.

* **Renamed avf to iavf.**

  Renamed Intel Ethernet Adaptive Virtual Function driver ``avf`` to ``iavf``,
  which includes the directory name, lib name, filenames, makefile, docs,
  macros, functions, structs and any other strings in the code.

* **Updated the enic driver.**

   Updated enic driver with new features and improvements, including:

  * Fixed several flow (director) bugs related to MARK, SCTP, VLAN, VXLAN, and
    inner packet matching.
  * Added limited support for RAW.
  * Added limited support for RSS.
  * Added limited support for PASSTHRU.

* **Updated the ixgbe driver.**

  Updated the ixgbe driver to add promiscuous mode support for the VF.

* **Updated the ice driver.**

  Updated ice driver with new features and improvements, including:

  * Added support of SSE and AVX2 instructions in Rx and Tx paths.
  * Added package download support.
  * Added Safe Mode support.
  * Supported RSS for UPD/TCP/SCTP+IPV4/IPV6 packets.

* **Updated the i40e driver.**

  New features for PF in the i40e driver:

  * Added support for VXLAN-GPE packet.
  * Added support for VXLAN-GPE classification.

* **Updated the ENETC driver.**

  Updated ENETC driver with new features and improvements, including:

  * Added physical addressing mode support.
  * Added SXGMII interface support.
  * Added basic statistics support.
  * Added promiscuous and allmulticast mode support.
  * Added MTU update support.
  * Added jumbo frame support.
  * Added queue start/stop.
  * Added CRC offload support.
  * Added Rx checksum offload validation support.

* **Updated the atlantic PMD.**

  Added MACSEC hardware offload experimental API.

* **Updated the Intel QuickAssist Technology (QAT) compression PMD.**

  Updated the Intel QuickAssist Technology (QAT) compression PMD to simplify,
  and make more robust, the handling of Scatter Gather Lists (SGLs) with more
  than 16 segments.

* **Updated the QuickAssist Technology (QAT) symmetric crypto PMD.**

  Added support for AES-XTS with 128 and 256 bit AES keys.

* **Added Intel QuickAssist Technology PMD for asymmetric crypto.**

  Added a new QAT Crypto PMD which provides asymmetric cryptography
  algorithms. Modular exponentiation and modular multiplicative
  inverse algorithms were added in this release.

* **Updated AESNI-MB PMD.**

  Added support for out-of-place operations.

* **Updated the IPsec library.**

  The IPsec library has been updated with AES-CTR and 3DES-CBC cipher algorithms
  support. The related ``ipsec-secgw`` test scripts have been added.

* **Updated the testpmd application.**

  Improved the ``testpmd`` application performance on ARM platform. For ``macswap``
  forwarding mode, NEON intrinsics are now used to do swap to save CPU cycles.

* **Updated power management library.**

  Added support for Intel Speed Select Technology - Base Frequency (SST-BF).
  The ``rte_power_get_capabilities`` struct now has a bit in it's returned mask
  indicating if it is a high frequency core.

* **Updated distributor sample application.**

  Added support for the Intel SST-BF feature so that the distributor core is
  pinned to a high frequency core if available.


API Changes
-----------

* eal: the type of the ``attr_value`` parameter of the function
  ``rte_service_attr_get()`` has been changed
  from ``uint32_t *`` to ``uint64_t *``.

* meter: replace ``enum rte_meter_color`` in the meter library with new
  ``rte_color`` definition added in 19.02. Replacements with ``rte_color``
  values has been performed in many places such as ``rte_mtr.h`` and
  ``rte_tm.h`` to consolidate multiple color definitions.

* vfio: Functions ``rte_vfio_container_dma_map`` and
  ``rte_vfio_container_dma_unmap`` have been extended with an option to
  request mapping or un-mapping to the default vfio container fd.

* power: ``rte_power_set_env`` and ``rte_power_unset_env`` functions
  have been modified to be thread safe.

* timer: Functions have been introduced that allow multiple instances of the
  timer lists to be created. In addition they are now allocated in shared
  memory. New functions allow particular timer lists to be selected when
  timers are being started, stopped, and managed.


ABI Changes
-----------

* ethdev: Additional fields in rte_eth_dev_info.

  The ``rte_eth_dev_info`` structure has had two extra fields
  added: ``min_mtu`` and ``max_mtu``. Each of these are of type ``uint16_t``.
  The values of these fields can be set specifically by the PMDs as
  supported values can vary from device to device.

* cryptodev: in 18.08 a new structure ``rte_crypto_asym_op`` was introduced and
  included into ``rte_crypto_op``. As the ``rte_crypto_asym_op`` structure was
  defined as cache-line aligned that caused unintended changes in
  ``rte_crypto_op`` structure layout and alignment. Remove cache-line
  alignment for ``rte_crypto_asym_op`` to restore expected ``rte_crypto_op``
  layout and alignment.

* timer: ``rte_timer_subsystem_init`` now returns success or failure to reflect
  whether it was able to allocate memory.


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

* **On x86 platforms, AVX512 support is disabled with binutils 2.31.**

  Due to a defect in binutils 2.31 AVX512 support is disabled.
  DPDK defect: https://bugs.dpdk.org/show_bug.cgi?id=249
  GCC defect: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90028

* **No software AES-XTS implementation.**

  There are currently no cryptodev software PMDs available which implement
  support for the AES-XTS algorithm, so this feature can only be used
  if compatible hardware and an associated PMD is available.


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
    * Fedora 29
    * FreeBSD 12.0
    * Red Hat Enterprise Linux Server release 7.4
    * Red Hat Enterprise Linux Server release 7.5
    * Red Hat Enterprise Linux Server release 7.6
    * SUSE12SP3
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

    * Intel Corporation Ethernet Controller 10G X550T

      * Firmware version: 0x80000482
      * Device id (pf): 8086:1563
      * Driver version: 5.1.0-k(ixgbe)

    * Intel(R) Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 6.80 0x80003cc1
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version: 2.7.29 (i40e)

    * Intel(R) Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

      * Firmware version: 3.33 0x80000fd5 0.0.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version: 2.7.29 (i40e)

    * Intel(R) Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 6.80 0x80003d05
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version: 2.7.29 (i40e)

    * Intel(R) Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version: 6.80 0x80003cfb
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version: 2.7.29 (i40e)

    * Intel(R) Corporation I350 Gigabit Network Connection

      * Firmware version: 1.63, 0x80000dda
      * Device id (pf/vf): 8086:1521 / 8086:1520
      * Driver version: 5.4.0-k (igb)

    * Intel Corporation I210 Gigabit Network Connection

      * Firmware version: 3.25, 0x800006eb, 1.1824.0
      * Device id (pf): 8086:1533
      * Driver version: 5.4.0-k(igb)

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
