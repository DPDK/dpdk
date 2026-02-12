.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2025 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 25.07
==================

New Features
------------

* **Enhanced EAL -l corelist argument.**

  Added support to the EAL ``-l`` argument for the full range of core-to-cpu mapping
  options supported by the ``--lcores`` EAL argument.
  Previously, ``-l`` only supported a subset of the options,
  allowing only a list of CPUs to be specified,
  which would be mapped 1:1 with internal lcore IDs.
  Now, it is possible to use the shortened ``-l`` form
  to map lcore IDs to threads running on specific CPUs.

  See the :doc:`../linux_gsg/eal_args.include` guide for examples.

* **Added PMU library.**

  Added a Performance Monitoring Unit (PMU) library which allows Linux applications
  to perform self monitoring activities without depending on external utilities like perf.

* **Updated Amazon ENA (Elastic Network Adapter) net driver.**

  * Added support for enabling fragment bypass mode for egress packets.
    This mode bypasses the PPS limit enforced by EC2 for fragmented egress packets on every ENI.
  * Fixed the device initialization routine to correctly handle failure during the registration
    or enabling of interrupts when operating in control path interrupt mode.
  * Fixed an issue where the device might be incorrectly reported as unresponsive when using
    polling-based admin queue functionality with a poll interval of less than 500 milliseconds.

* **Added Mucse rnp net driver.**

  Added a new network PMD which supports Mucse 10 Gigabit Ethernet NICs.
  See the :doc:`../nics/rnp` for more details.

* **Added RSS type for RoCE v2.**

  Added ``RTE_ETH_RSS_IB_BTH`` flag so that the RoCE InfiniBand Base Transport Header
  can be used as input for RSS.

* **Added burst mode query function to Intel drivers.**

  Added support for Rx and Tx burst mode query to the following drivers:

    * e1000 (igb)
    * ixgbe
    * iavf

* **Added Tx packet pacing support to Intel ice net driver.**

  Intel\ |reg| Ethernet E830 network adapters support delayed/timed packet Tx based on timestamp.
  Support for this feature was added to the ice ethdev driver.

* **Updated NVIDIA mlx5 driver.**

  Added matching on IPv6 frag extension header with async flow template API.

* **Updated Solarflare network driver.**

  Added support for AMD Solarflare X45xx adapters.

* **Updated virtio driver.**

  Added support for Rx and Tx burst mode query.

* **Added ZTE Storage Data Accelerator (ZSDA) crypto driver.**

  Added a crypto driver for ZSDA devices
  to support some encrypt, decrypt and hash algorithms.

  See the :doc:`../cryptodevs/zsda` guide for more details on the new driver.

* **Updated Marvell cnxk crypto driver.**

  * Added support for CN20K SoC in cnxk CPT driver.
  * Added support for session-less asymmetric operations.

* **Updated UADK crypto & compress driver.**

  * Updated to init2 interface which requires v2.9.1 of the UADK library.
  * Updated to asynchronous mode for better performance.

* **Added eventdev vector adapter.**

  Added the event vector adapter library.
  This library extends the event-based model by introducing API
  that allow applications to offload creation of event vectors,
  thereby reducing the scheduling latency.

  Added vector adapter producer mode in eventdev test to measure performance.

  See the :doc:`../prog_guide/eventdev/event_vector_adapter` guide
  for more details on the new library.

* **Added event vector adapter support in CN20K event device driver.**

  Added support for the event vector adapter in the CN20K event device driver.
  This allows the CN20K to offload ``rte_event_vector`` creation and aggregation
  of objects originating from the CPU.

* **Added feature arc support in graph library.**

  Feature arc helps ``rte_graph`` based applications
  to manage multiple network protocols/features with runtime configurability,
  in-built node-reusability and optimized control/data plane synchronization.

  See section ``Graph feature arc`` in :doc:`../prog_guide/graph_lib` for more details.

  Added ``ip4 output`` feature arc processing in ``ip4_rewrite`` node.

* **Added support for string and boolean types to the argparse library.**

  The argparse library now supports parsing of string and boolean types.
  String values are simply saved as-is,
  while the boolean support allows for values "true", "false", "1" or "0".

* **Added graph nodes specific shared mbuf dynamic field.**

  Instead each node registers its own mbuf dynamic field for its specific purpose,
  a global/shared structure was added which can be used/overloaded by any node
  (including out-of-tree nodes).
  This minimizes footprint of node specific mbuf dynamic field.


Removed Items
-------------

* eal: Removed the ``rte_function_versioning.h`` header from the exported headers.

* crypto/qat: Removed ZUC-256 algorithms from Intel QuickAssist Technology PMD.

  Due to changes in the specification related to IV size and initialization sequence,
  support for ZUC-256 cipher and integrity algorithms
  was removed from Gen 3 and Gen 5 PMD.


API Changes
-----------

* memory: Added secure functions to force zeroing some memory,
  bypassing compiler optimizations:
  ``rte_memzero_explicit()`` and ``rte_free_sensitive()``.

* graph: Added ``graph`` field to the ``rte_node.dispatch`` structure.

* argparse: The ``rte_argparse_arg`` structure used for defining arguments has been updated.
  See ABI changes in the next section for details.


ABI Changes
-----------

* No ABI change that would break compatibility with 24.11.

* argparse: The experimental argparse library has had the following updates:

  * The main parsing function, ``rte_argparse_parse()``,
    now returns the number of arguments parsed on success, rather than zero.
    It still returns a negative value on error.

  * When parsing a list of arguments,
    ``rte_argparse_parse()`` stops processing arguments when a ``--`` argument is encountered.
    This behaviour mirrors the behaviour of the ``getopt()`` function,
    as well as the behaviour of ``rte_eal_init()`` function.

  * The ``rte_argparse_arg`` structure used for defining arguments has been updated
    to separate out into separate fields the options for:

    #. Whether the argument is required or optional.
    #. What the type of the argument is (in case of saving the parameters automatically).
    #. Any other flags - of which there is only one, ``RTE_ARGPARSE_FLAG_SUPPORT_MULTI``,
       at this time.

  * With the splitting of the flags into separate enums for categories,
    the names of the flags have been changed to better reflect their purpose.
    The flags/enum values are:

    * For the ``value_required`` field:

      * ``RTE_ARGPARSE_VALUE_NONE``
      * ``RTE_ARGPARSE_VALUE_REQUIRED``
      * ``RTE_ARGPARSE_VALUE_OPTIONAL``

    * For the ``value_type`` field:

      * ``RTE_ARGPARSE_VALUE_TYPE_NONE``
        (No argument value type is specified, callback is to be used for processing.)
      * ``RTE_ARGPARSE_VALUE_TYPE_INT``
      * ``RTE_ARGPARSE_VALUE_TYPE_U8``
      * ``RTE_ARGPARSE_VALUE_TYPE_U16``
      * ``RTE_ARGPARSE_VALUE_TYPE_U32``
      * ``RTE_ARGPARSE_VALUE_TYPE_U64``
      * ``RTE_ARGPARSE_VALUE_TYPE_STR``
      * ``RTE_ARGPARSE_VALUE_TYPE_BOOL``

    * Other flags:

      * ``RTE_ARGPARSE_FLAG_SUPPORT_MULTI``
        (Allows the argument to be specified multiple times.)


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| x7835RE
    * Intel\ |reg| Xeon\ |reg| 6767P
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU Max 9480  CPU @ 1.9GHz
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

    * Fedora 42
    * FreeBSD 14.2
    * Microsoft Azure Linux 3.0
    * OpenAnolis OS 8.10
    * openEuler 24.03 (LTS-SP1)
    * Red Hat Enterprise Linux Server release 9.6
    * Ubuntu 24.04.2 LTS

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.80 0x80020601 1.3805.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version(out-tree): 2.2.8 (ice)
      * Driver version(in-tree): 6.8.0-60-generic (Ubuntu24.04.2 LTS) /
        5.14.0-570.12.1.el9_6.x86_64 (RHEL9.6) (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.23.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.80 0x80020543 1.3805.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version(out-tree): 2.2.8 (ice)
      * Driver version(in-tree): 6.6.12.1-1.azl3+ice+ (Microsoft Azure Linux 3.0) (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.23.0

    * Intel\ |reg| Ethernet Controller E810-XXV for SFP (2x25G)

      * Firmware version: 4.80 0x80020544 1.3805.0
      * Device id (pf/vf): 8086:159b / 8086:1889
      * Driver version: 2.2.8 (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0

    * Intel\ |reg| Ethernet Controller E830-CC for SFP

      * Firmware version: 1.00 0x800161b9 1.3832.0
      * Device id (pf/vf): 8086:12d3 / 8086:1889
      * Driver version: 2.2.8 (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.23.0

    * Intel\ |reg| Ethernet Controller E830-CC for QSFP

      * Firmware version: 1.00 0x800161b7 1.3832.0
      * Device id (pf/vf): 8086:12d2 / 8086:1889
      * Driver version: 2.2.8 (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.23.0

    * Intel\ |reg| Ethernet Connection E825-C for QSFP

      * Firmware version: 3.94 0x8000766d 1.3825.0
      * Device id (pf/vf): 8086:579d / 8086:1889
      * Driver version: 2.2.8 (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.23.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x000161bf
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version(out-tree): 6.1.5 (ixgbe)
      * Driver version(in-tree): 6.8.0-60-generic (Ubuntu24.04.2 LTS) (ixgbe)

    * Intel\ |reg| Ethernet Network Adapter E610-XT2

      * Firmware version: 1.00 0x800066ae 0.0.0
      * Device id (pf/vf): 8086:57b0 / 8086:57ad
      * Driver version(out-tree): 6.1.5 (ixgbe)

    * Intel\ |reg| Ethernet Network Adapter E610-XT4

      * Firmware version: 1.00 0x80004ef2 0.0.0
      * Device id (pf/vf): 8086:57b0 / 8086:57ad
      * Driver version(out-tree): 6.1.5 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 9.54 0x8000fb3f 1.3800.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version(out-tree): 2.28.7 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

      * Firmware version: 6.50 0x800043a1 1.3597.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version(out-tree): 2.28.7 (i40e)
      * Driver version(in-tree): 5.14.0-570.12.1.el9_6.x86_64 (RHEL9.6) (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version:  9.54 0x8000fb47 1.3800.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version(out-tree): 2.28.7 (i40e)
      * Driver version(in-tree): 6.8.0-63-generic (Ubuntu24.04.2 LTS) /
        5.14.0-570.12.1.el9_6.x86_64 (RHEL9.6) (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 9.54 0x8000fb7f 1.3800.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version(out-tree): 2.28.7 (i40e)

    * Intel\ |reg| Ethernet Controller I225-LM

      * Firmware version: 1.3, 0x800000c9
      * Device id (pf): 8086:15f2
      * Driver version(in-tree): 6.8.0-60-generic (Ubuntu24.04.2 LTS) (igc)

    * Intel\ |reg| Ethernet Controller I225-IT

      * Firmware version: 1094:878d
      * Device id (pf): 8086:0d9f
      * Driver version(in-tree): 6.12-intel (Ubuntu 24.04.2 LTS) (igc)

    * Intel\ |reg| Ethernet Controller I226-LM

      * Firmware version: 2.14, 0x8000028c
      * Device id (pf): 8086:125b
      * Driver version(in-tree): 6.8.0-60-generic (Ubuntu24.04.2 LTS) (igc)

    * Intel\ |reg| Corporation Ethernet Server Adapter I350-T4

      * Firmware version: 1.63, 0x8000116b
      * Device id (pf/vf): 8086:1521 /8086:1520
      * Driver version: 6.12-intel (Ubuntu 24.04.2 LTS) (igb)

    * Intel\ |reg| Infrastructure Processing Unit (Intel\ |reg| IPU) E2100

      * Firmware version: ci-ts.release.2.0.0.11126
      * Device id (idpf/cpfl): 8086:1452/8086:1453
      * Driver version: 0.0.772 (idpf)

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

    * Red Hat Enterprise Linux release 9.2 (Plow)
    * Red Hat Enterprise Linux release 9.1 (Plow)
    * Red Hat Enterprise Linux release 8.6 (Ootpa)
    * Ubuntu 24.04
    * Ubuntu 22.04
    * Ubuntu 20.04
    * SUSE Enterprise Linux 15 SP4

  * DOCA:

    * DOCA 3.0.0-058000 and above.

  * upstream kernel:

    * Linux 6.15.0 and above

  * rdma-core:

    * rdma-core-57.0 and above

  * NICs

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.45.1020 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.45.1020 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.45.1020 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-8 SuperNIC 400G  MT4131 - 900-9X81Q-00CN-STA (2x400G)

      * Host interface: PCI Express 6.0 x16
      * Device ID: 15b3:1023
      * Firmware version: 40.45.1020 and above

* NVIDIA\ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.45.1020 and above

  * NVIDIA\ |reg| BlueField\ |reg|-3 P-Series DPU MT41692 - 900-9D3B6-00CV-AAB (2x200G)

    * Host interface: PCI Express 5.0 x16
    * Device ID: 15b3:a2dc
    * Firmware version: 32.45.1020 and above

  * Embedded software:

    * Ubuntu 22.04
    * MLNX_OFED 25.04-0.6.1.1
    * bf-bundle-3.0.0-135_25.04_ubuntu-22.04
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
      * Firmware version: 22.45.1020 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.45.1020 and above

   * DOCA:

      * DOCA 3.0.0-058000 and above
