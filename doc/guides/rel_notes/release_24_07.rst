.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2024 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 24.07
==================

New Features
------------

* **Introduced pointer compression library.**

  Library provides functions to compress and decompress arrays of pointers
  which can improve application performance under certain conditions.
  Performance test was added to help users evaluate performance on their setup.

* **Added API to retrieve memory locations of objects in a mempool.**

  Added mempool API ``rte_mempool_get_mem_range`` and
  ``rte_mempool_get_obj_alignment`` to retrieve information about the memory
  range and the alignment of objects stored in a mempool.

* **Updated AF_XDP driver.**

  * Enabled multi-interface (UDS) support with AF_XDP Device Plugin.

    The argument ``use_cni`` was limiting a pod to a single netdev/interface.
    The new ``dp_path`` parameter removed this limitation
    and maintains backward compatibility for applications using the ``use_cni``
    vdev argument with the AF_XDP Device Plugin.

  * Integrated AF_XDP Device Plugin eBPF map pinning support.

    The argument ``use_map_pinning`` was added to allow Kubernetes Pods
    to use AF_XDP with DPDK, and run with limited privileges,
    without having to do a full handshake over a Unix Domain Socket
    with the Device Plugin.

* **Updated Amazon ena (Elastic Network Adapter) driver.**

  * Reworked the driver logger usage in order to improve Tx performance.
  * Reworked the device uninitialization flow to ensure complete resource cleanup
    and lay the groundwork for hot-unplug support.

* **Updated Intel ice driver.**

  * Added support for E830 device family.
  * Added support for configuring the Forward Error Correction (FEC) mode,
    querying FEC capabilities and current FEC mode from a device.

* **Updated Intel i40e driver.**

  * Added support for configuring the Forward Error Correction (FEC) mode,
    querying FEC capabilities and current FEC mode from a device.

* **Updated Intel ixgbe driver.**

  * Updated base code with E610 device family support.

* **Added Napatech ntnic net driver [EXPERIMENTAL].**

  * Added the PMD for Napatech smartNIC:

    - Ability to initialize the NIC (NT200A02)
    - Supporting only one FPGA firmware (9563.55.39)
    - Ability to bring up the 100G link
    - Supporting QSFP/QSFP+/QSFP28 NIM
    - Does not support datapath

* **Updated Marvell cnxk net driver.**

  * Added support disabling custom meta aura
    and separately use custom SA action support.
  * Added MTU update for port representor.
  * Added multi-segment support for port representor.

* **Updated NVIDIA mlx5 driver.**

  * Added match with Tx queue.
  * Added match with external Tx queue.
  * Added match with E-Switch manager.
  * Added async flow item and actions validation.
  * Added global and per-port out of buffer counter for hairpin queues.
  * Added hardware queue object context dump for Rx/Tx debugging.

* **Updated TAP driver.**

  * Updated to support up to 8 queues when used by secondary process.
  * Fixed support for RSS flow action to work with current Linux kernels
    and BPF tooling.
    Will only be enabled if clang, libbpf 1.0 and bpftool are available.

* **Updated Wangxun ngbe driver.**

  * Added SSE/NEON vector datapath.

* **Updated Wangxun txgbe driver.**

  * Added SSE/NEON vector datapath.

* **Added AMD Pensando ionic crypto driver.**

  Added a new crypto driver for AMD Pensando hardware accelerators.

* **Updated NVIDIA mlx5 crypto driver.**

  * Added AES-GCM IPsec operation optimization.

* **Updated IPsec_MB crypto driver.**

  * Made Kasumi and ChaCha-Poly PMDs to share the job code path
    with AESNI_MB PMD.

* **Added UADK compress driver.**

  Added a new compress driver for the UADK library. See the
  :doc:`../compressdevs/uadk` guide for more details on this new driver.

* **Updated Marvell CNXK DMA driver.**

  * Updated DMA driver internal pool to use higher chunk size,
    effectively reducing the number of mempool allocs needed,
    thereby increasing DMA performance.

* **Added Marvell Odyssey ODM DMA driver.**

  * Added Marvell Odyssey ODM DMA device PMD.

* **Updated the DSW event device.**

  * Added support for ``RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE``,
    allowing applications to take on new tasks without having completed
    (released) the previous event batch. This in turn facilities DSW
    use alongside high-latency look-aside hardware accelerators.

* **Updated the hash library.**

  * Added defer queue reclamation via RCU.
  * Added SVE support for bulk lookup.


Removed Items
-------------

* **Disabled the BPF library and net/af_xdp for 32-bit x86.**

  BPF is not supported and the librte-bpf test fails on 32-bit x86 kernels.
  So disable the library and the pmd.

* **Removed hisilicon DMA support for HIP09 platform.**

  The DMA for HIP09 is no longer available,
  so the support is removed from hisilicon driver for HIP09 platform.


API Changes
-----------

* mbuf: ``RTE_MARKER`` fields ``cacheline0`` and ``cacheline1``
  have been removed from ``struct rte_mbuf``.

* hash: The ``rte_hash_sig_compare_function`` internal enum is not exposed
  in the public API anymore.


ABI Changes
-----------

* No ABI change that would break compatibility with 23.11.

* eventdev/dma: Reorganize the experimental fastpath structure ``rte_event_dma_adapter_op``
  to optimize the memory layout and improve performance.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel Atom\ |reg| P5342 processor
    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| D-1747NTE CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| D-2796NT CPU @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6140M CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8380 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8468H
    * Intel\ |reg| Xeon\ |reg| Platinum 8490H

  * OS:

    * CBL Mariner 2.0
    * Fedora 40
    * FreeBSD 14.0
    * OpenAnolis OS 8.8
    * openEuler 22.03 (LTS-SP3)
    * Red Hat Enterprise Linux Server release 9.0
    * Red Hat Enterprise Linux Server release 9.4
    * Ubuntu 22.04.3
    * Ubuntu 24.04

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.50 0x8001d8b5 1.3597.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version(out-tree): 1.14.11 (ice)
      * Driver version(in-tree): 6.8.0-31-generic (Ubuntu24.04) /
        5.14.0-427.13.1.el9_4.x86_64+rt (RHEL9.4) (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.50 0x8001d8b6 1.3597.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version(out-tree): 1.14.11 (ice)
      * Driver version(in-tree): 5.15.55.1-1.cm2-5464b22cac7+ (CBL Mariner 2.0) (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Controller E810-XXV for SFP (2x25G)

      * Firmware version: 4.50 0x8001d8c2 1.3597.0
      * Device id (pf/vf): 8086:159b / 8086:1889
      * Driver version: 1.14.11 (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0

    * Intel\ |reg| Ethernet Connection E823-C for QSFP

      * Firmware version: 3.39 0x8001db5f 1.3597.0
      * Device id (pf/vf): 8086:188b / 8086:1889
      * Driver version: 1.14.11 (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Connection E823-L for QSFP

      * Firmware version: 3.39 0x8001da47 1.3534.0
      * Device id (pf/vf): 8086:124c / 8086:1889
      * Driver version: 1.14.11 (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Connection E822-L for backplane

      * Firmware version: 3.39 0x8001d9b6 1.3353.0
      * Device id (pf/vf): 8086:1897 / 8086:1889
      * Driver version: 1.14.11 (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x000161bf
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version(out-tree): 5.20.9 (ixgbe)
      * Driver version(in-tree): 6.8.0-31-generic (Ubuntu24.04) /
        5.14.0-427.13.1.el9_4.x86_64 (RHEL9.4)(ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 9.50 0x8000f145 1.3597.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version(out-tree): 2.25.9 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

      * Firmware version: 6.50 0x80004216 1.3597.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version(out-tree): 2.25.9 (i40e)
      * Driver version(in-tree): 5.14.0-427.13.1.el9_4.x86_64 (RHEL9.4)(i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 9.50 0x8000f167 1.3597.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version(out-tree): 2.25.9 (i40e)
      * Driver version(in-tree): 6.8.0-31-generic (Ubuntu24.04) /
        5.14.0-427.13.1.el9_4.x86_64 (RHEL9.4)(i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 9.50 0x8000f181 1.3597.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version(out-tree): 2.25.9 (i40e)

    * Intel\ |reg| Ethernet Controller I225-LM

      * Firmware version: 1.3, 0x800000c9
      * Device id (pf): 8086:15f2
      * Driver version(in-tree): 6.8.0-31-generic (Ubuntu24.04)(igc)

    * Intel\ |reg| Ethernet Controller I226-LM

      * Firmware version: 2.14, 0x8000028c
      * Device id (pf): 8086:125b
      * Driver version(in-tree): 6.8.0-31-generic (Ubuntu24.04)(igc)

* Intel\ |reg| platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697A v4 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697 v3 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2670 0 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2640 @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 0 @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2620 v4 @ 2.10GHz

  * OS:

    * Red Hat Enterprise Linux release 9.1 (Plow)
    * Red Hat Enterprise Linux release 8.6 (Ootpa)
    * Red Hat Enterprise Linux release 8.4 (Ootpa)
    * Red Hat Enterprise Linux Server release 7.9 (Maipo)
    * Red Hat Enterprise Linux Server release 7.6 (Maipo)
    * Ubuntu 22.04
    * Ubuntu 20.04
    * SUSE Enterprise Linux 15 SP2

  * OFED:

    * MLNX_OFED 24.04-0.6.6.0 and above

  * upstream kernel:

    * Linux 6.10.0 and above

  * rdma-core:

    * rdma-core-52.0 and above

  * NICs

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.41.1000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.41.1000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.41.1000 and above

* NVIDIA\ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.41.1000 and above

  * NVIDIA\ |reg| BlueField\ |reg|-3 P-Series DPU MT41692 - 900-9D3B6-00CV-AAB (2x200G)

    * Host interface: PCI Express 5.0 x16
    * Device ID: 15b3:a2dc
    * Firmware version: 32.41.1000 and above

  * Embedded software:

    * Ubuntu 22.04
    * MLNX_OFED 24.04-0.6.6.0 and above
    * bf-bundle-2.7.0-33_24.04_ubuntu-22.04
    * DPDK application running on ARM cores

* IBM Power 9 platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202)

  * OS:

    * Ubuntu 20.04

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.41.1000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.41.1000 and above

  * OFED:

    * MLNX_OFED 24.04-0.6.6.0
