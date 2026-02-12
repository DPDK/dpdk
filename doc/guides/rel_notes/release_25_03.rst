.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2024 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 25.03
==================

New Features
------------

* **Added Staged-Ordered-Ring (SORING) API to the ring library.**

  Added new API to the ring library to provide a software abstraction
  for ordered queues with multiple processing stages.
  It is based on the conventional DPDK ``rte_ring`` and re-uses many of its concepts,
  including substantial part of its code.
  It can be viewed as an extension of ``rte_ring`` functionality.

* **Hardened more allocation functions.**

  Added allocation attributes to functions that allocate data:

  * ``rte_acl_create()``
  * ``rte_comp_op_pool_create()``
  * ``rte_event_ring_create()``
  * ``rte_fbk_hash_create()``
  * ``rte_fib_create()``
  * ``rte_fib6_create()``
  * ``rte_hash_create()``
  * ``rte_lpm_create()``
  * ``rte_lpm6_create()``
  * ``rte_member_create()``
  * ``rte_mempool_create()``
  * ``rte_mempool_create_empty()``
  * ``rte_reorder_create()``
  * ``rte_rib_create()``
  * ``rte_rib6_create()``
  * ``rte_ring_create()``
  * ``rte_sched_port_config()``
  * ``rte_stats_bitrate_create()``
  * ``rte_tel_data_alloc()``

  This can catch some obvious bugs at compile time (with GCC 11.0 or later).
  For example, calling ``free`` on a pointer that was allocated with one
  of those functions (and vice versa), freeing the same pointer twice
  in the same routine or freeing an object that was not created by allocation.

* **Updated af_packet net driver.**

  * Added ability to configure receive packet fanout mode.
  * Added statistics for failed buffer allocation and missed packets.

* **Updated Amazon ENA (Elastic Network Adapter) net driver.**

  * Added support for mutable RSS table size based on device capabilities.

* **Updated AMD axgbe driver.**

  * Added support for the TCP Segmentation Offload (TSO).

* **Updated Intel e1000 driver.**

  * Added support for the Intel i225-series NICs (previously handled by net/igc).
  * Updated base code to the latest version.

* **Updated Intel ipdf driver.**

  * Added support for AVX2 instructions in single queue Rx and Tx path.
    (The single queue model processes all packets in order within one Rx queue,
    while the split queue model separates packet data and metadata into different queues
    for parallel processing and improved performance.)

* **Updated Marvell cnxk net driver.**

  * Added flow rules support for CN20K SoC.
  * Added inline IPsec support for CN20K SoC.

* **Updated Napatech ntnic driver.**

  * Added support for the NT400D13 adapter.

* **Updated NVIDIA mlx5 driver.**

  * Added support for NVIDIA ConnectX-8 adapters.
  * Optimized large scale port probing.
    This feature enhances the efficiency of probing VF/SFs on a large scale
    by significantly reducing the probing time.

* **Updated Wangxun ngbe driver.**

  * Added support for virtual function (VF).

* **Updated ZTE zxdh network driver.**

  * Added support for multiple queues.
  * Added support for SR-IOV VF.
  * Scatter and gather for Tx and Rx.
  * Link state and auto-negotiation.
  * MAC address filtering.
  * Multicast and promiscuous mode.
  * VLAN filtering and offload.
  * Receive Side Scaling (RSS).
  * Hardware and extended statistics.
  * Jumbo frames.
  * Checksum offload.
  * LRO and TSO.
  * Ingress metering.

* **Added Yunsilicon xsc net driver [EXPERIMENTAL].**

  Added network driver for the Yunsilicon metaScale serials NICs.

* **Updated vhost/virtio for RSA crypto.**

  * Added RSA crypto operations to the vhost library.
  * Added RSA crypto operations to the virtio crypto driver.

* **Updated IPsec_MB crypto driver.**

  * Added support for the SM4 GCM algorithm.

* **Added ZTE Storage Data Accelerator (ZSDA) driver.**

  Added a compress driver for ZSDA devices
  to support the deflate compression and decompression algorithm.

  See the :doc:`../compressdevs/zsda` guide for more details on the new driver.

* **Added atomic tests to the eventdev test application.**

  Added two atomic tests: ``atomic_queue`` and ``atomic_atq``.
  These work in the same way as the corresponding ordered tests
  but use atomic queues exclusively.
  Atomicity is verified using spinlocks.


Removed Items
-------------

* **Dropped support for Intel\ |reg| C++ Compiler (icc) (replaced by "icx" support).**

  Support for the older Intel\ |reg| C++ Compiler "icc" has been dropped.
  The newer Intel\ |reg| oneAPI DPC++/C++ Compiler, "icx", can be used to compile DPDK instead.


API Changes
-----------

* eal: The ``__rte_packed`` macro for packing data is replaced with
  ``__rte_packed_begin`` / ``__rte_packed_end``.

* eal: The ``__rte_weak`` macro is deprecated and will be removed in a future release.

* net: Changed the API for CRC calculation to be thread-safe.
  An opaque context argument was introduced to the net CRC API
  containing the algorithm type and length.
  This argument is added to ``rte_net_crc_calc``, ``rte_net_crc_set_alg``
  and freed with ``rte_net_crc_free``.
  These functions are versioned to retain binary compatibility until the next LTS release.

* build: The Intel networking drivers:
  cpfl, e1000, fm10k, i40e, iavf, ice, idpf, ipn3ke and ixgbe,
  have been moved from ``drivers/net`` to a new ``drivers/net/intel`` directory.
  The resulting build output, including the driver filenames, is the same,
  but to enable/disable these drivers via Meson option requires the use of the new paths.
  For example, ``-Denable_drivers=/net/i40e`` becomes ``-Denable_drivers=/net/intel/i40e``.

* build: The Intel IGC networking driver was merged with the e1000 driver
  and is no longer provided as a separate driver.
  The resulting build output will no longer have the ``librte_net_igc.*`` driver files,
  but the ``librte_net_e1000.*`` driver files will provide support
  for all of the devices and features of the old driver.
  In addition, to enable/disable the driver via Meson option,
  the path has changed from ``-Denable_drivers=net/igc``
  to ``-Denable_drivers=net/intel/e1000``.

* build: The driver ``common/idpf`` has been merged into the ``net/intel/idpf`` driver.
  Similarly, the ``common/iavf`` driver has been merged into the ``net/intel/iavf`` driver.
  These changes should have no impact to end applications, but,
  when specifying the ``idpf`` or ``cpfl`` net drivers to Meson via ``-Denable_drivers`` option,
  there is no longer any need to also specify the ``common/idpf`` driver.
  In the same way, when specifying the ``iavf`` or ``ice`` net drivers,
  there is no need to also specify the ``common/iavf`` driver.
  Note, however, ``net/intel/cpfl`` driver now depends upon the ``net/intel/idpf`` driver.


ABI Changes
-----------

* No ABI change that would break compatibility with 24.11.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| x7835RE
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6140M CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8358 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8380 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8468H
    * Intel\ |reg| Xeon\ |reg| Platinum 8490H

  * OS:

    * Microsoft Azure Linux 3.0
    * Fedora 41
    * FreeBSD 14.2
    * OpenAnolis OS 8.9
    * openEuler 24.03 (LTS)
    * Red Hat Enterprise Linux Server release 9.0
    * Red Hat Enterprise Linux Server release 9.4
    * Red Hat Enterprise Linux Server release 9.5
    * Ubuntu 22.04.3 LTS
    * Ubuntu 22.04.4 LTS
    * Ubuntu 22.04.5 LTS
    * Ubuntu 24.04.1 LTS

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.70 0x8001f79e 1.3755.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version(out-tree): 1.16.3 (ice)
      * Driver version(in-tree): 6.8.0-48-generic (Ubuntu24.04.1) /
        5.14.0-503.11.1.el9_5.x86_64 (RHEL9.5) (ice)
      * OS Default DDP: 1.3.39.1
      * COMMS DDP: 1.3.53.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.70 0x8001f7b8 1.3755.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version(out-tree): 1.16.3 (ice)
      * Driver version(in-tree): 6.6.12.1-1.azl3+ice+ (Microsoft Azure Linux 3.0) (ice)
      * OS Default DDP: 1.3.39.1
      * COMMS DDP: 1.3.53.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Controller E810-XXV for SFP (2x25G)

      * Firmware version: 4.70 0x8001f7ba 1.3755.0
      * Device id (pf/vf): 8086:159b / 8086:1889
      * Driver version: 1.16.3 (ice)
      * OS Default DDP: 1.3.39.1
      * COMMS DDP: 1.3.53.0

    * Intel\ |reg| Ethernet Network Adapter E830-XXVDA2 for OCP

      * Firmware version: 1.00 0x800107e3 1.3766.0
      * Device id (pf/vf): 8086:12d3 / 8086:1889
      * Driver version: 1.16.3 (ice)
      * OS Default DDP: 1.3.39.1

    * Intel\ |reg| Ethernet Network Adapter E830-CQDA2

      * Firmware version: 1.00 0x800107ca 1.3766.0
      * Device id (pf/vf): 8086:12d2 / 8086:1889
      * Driver version: 1.16.3 (ice)
      * OS Default DDP: 1.3.39.1
      * COMMS DDP: 1.3.53.0
      * Wireless Edge DDP: 1.3.19.0

    * Intel\ |reg| Ethernet Connection E825-C for QSFP

      * Firmware version: 3.81 0x8000674f 1.0.0
      * Device id (pf/vf): 8086:579d / 8086:1889
      * Driver version: 2.1.0 (ice)
      * OS Default DDP: 1.3.39.2
      * COMMS DDP: 1.3.54.0
      * Wireless Edge DDP: 1.3.22.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x000161bf
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version(out-tree): 6.0.6 (ixgbe)
      * Driver version(in-tree): 6.8.0-48-generic (Ubuntu24.04.1) (ixgbe)

    * Intel\ |reg| Ethernet Network Adapter E610-XT2

      * Firmware version: 1.00 0x800066ae 0.0.0
      * Device id (pf/vf): 8086:57b0 / 8086:57ad
      * Driver version(out-tree): 6.0.6 (ixgbe)

    * Intel\ |reg| Ethernet Network Adapter E610-XT4

      * Firmware version: 1.00 0x80004ef2 0.0.0
      * Device id (pf/vf): 8086:57b0 / 8086:57ad
      * Driver version(out-tree): 6.0.6 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 9.53 0x8000f8f5 1.3755.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version(out-tree): 2.27.8 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

      * Firmware version: 6.50 0x80004216 1.3597.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version(out-tree): 2.27.8 (i40e)
      * Driver version(in-tree): 5.14.0-427.13.1.el9_4.x86_64 (RHEL9.4)(i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version:  9.53 0x8000f912 1.3755.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version(out-tree): 2.27.8 (i40e)
      * Driver version(in-tree): 6.8.0-45-generic (Ubuntu24.04.1) /
        5.14.0-427.13.1.el9_4.x86_64 (RHEL9.4)(i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 9.53 0x8000f8f5 1.3755.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version(out-tree): 2.27.8 (i40e)

    * Intel\ |reg| Ethernet Controller I225-LM

      * Firmware version: 1.3, 0x800000c9
      * Device id (pf): 8086:15f2
      * Driver version(in-tree): 6.8.0-48-generic (Ubuntu24.04.1)(igc)

    * Intel\ |reg| Ethernet Controller (2) I225-IT

      * Firmware version: 1.82, 0x8000026C
      * Device id (pf): 8086:0d9f
      * Driver version(in-tree): 6.6.25-lts-240422t024020z (Ubuntu22.04.4 LTS)(igc)

    * Intel\ |reg| Ethernet Controller I226-LM

      * Firmware version: 2.14, 0x8000028c
      * Device id (pf): 8086:125b
      * Driver version(in-tree): 6.8.0-45-generic (Ubuntu24.04.1)(igc)

    * Intel\ |reg| Corporation Ethernet Server Adapter I350-T4

      * Firmware version: 1.63, 0x80001001
      * Device id (pf/vf): 8086:1521 /8086:1520
      * Driver version: 6.6.25-lts-240422t024020z(igb)

    * Intel\ |reg| Infrastructure Processing Unit (Intel\ |reg| IPU) E2100

      * Firmware version: CI-9231
      * Device id (idpf/cpfl): 8086:1452/8086:1453
      * Driver version: 0.0.754 (idpf)

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
    * Ubuntu 22.04
    * Ubuntu 20.04
    * SUSE Enterprise Linux 15 SP2

  * DOCA:

    * DOCA 2.10.0-0.5.3 and above

  * upstream kernel:

    * Linux 6.12.0 and above

  * rdma-core:

    * rdma-core-54.0 and above

  * NICs

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.44.1036 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.44.1036 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.44.1036 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-8 SuperNIC 400G  MT4131 - 900-9X81Q-00CN-STA (2x400G)

      * Host interface: PCI Express 6.0 x16
      * Device ID: 15b3:1023
      * Firmware version: 40.44.1036 and above

* NVIDIA\ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.44.1036 and above

  * NVIDIA\ |reg| BlueField\ |reg|-3 P-Series DPU MT41692 - 900-9D3B6-00CV-AAB (2x200G)

    * Host interface: PCI Express 5.0 x16
    * Device ID: 15b3:a2dc
    * Firmware version: 32.44.1036 and above

  * Embedded software:

    * Ubuntu 22.04
    * MLNX_OFED 25.01-0.6.0.0
    * bf-bundle-2.10.0-147_25.01_ubuntu-22.04
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
      * Firmware version: 22.44.1036 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.44.1036 and above

   * DOCA:

      * DOCA 2.10.0-0.5.3 and above
