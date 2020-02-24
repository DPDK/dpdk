..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

DPDK Release 18.08
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_18_08.html


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

     This section is a comment. Do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =========================================================

* **Added support for Hyper-V netvsc PMD.**

  The new ``netvsc`` poll mode driver provides native support for
  networking on Hyper-V. See the :doc:`../nics/netvsc` NIC driver guide
  for more details on this new driver.

* **Added Flow API support for CXGBE PMD.**

  Flow API support has been added to CXGBE Poll Mode Driver to offload
  flows to Chelsio T5/T6 NICs. Support added for:

  * Wildcard (LE-TCAM) and Exact (HASH) match filters.
  * Match items: physical ingress port, IPv4, IPv6, TCP and UDP.
  * Action items: queue, drop, count, and physical egress port redirect.

* **Added ixgbe preferred Rx/Tx parameters.**

  Rather than applications providing explicit Rx and Tx parameters such as
  queue and burst sizes, they can request that the EAL instead uses preferred
  values provided by the PMD, falling back to defaults within the EAL if the
  PMD does not provide any. The provision of such tuned values now includes
  the ixgbe PMD.

* **Added descriptor status check support for fm10k.**

  The ``rte_eth_rx_descriptor_status`` and ``rte_eth_tx_descriptor_status``
  APIs are now supported by fm10K.

* **Updated the enic driver.**

  * Add low cycle count Tx handler for no-offload Tx.
  * Add low cycle count Rx handler for non-scattered Rx.
  * Minor performance improvements to scattered Rx handler.
  * Add handlers to add/delete VxLAN port number.
  * Add devarg to specify ingress VLAN rewrite mode.

* **Updated mlx5 driver.**

  Updated the mlx5 driver including the following changes:

  * Added port representors support.
  * Added Flow API support for e-switch rules.
    Added support for ACTION_PORT_ID, ACTION_DROP, ACTION_OF_POP_VLAN,
    ACTION_OF_PUSH_VLAN, ACTION_OF_SET_VLAN_VID, ACTION_OF_SET_VLAN_PCP
    and ITEM_PORT_ID.
  * Added support for 32-bit compilation.

* **Added TSO support for the mlx4 driver.**

  Added TSO support for the mlx4 drivers from MLNX_OFED_4.4 and above.

* **SoftNIC PMD rework.**

  The SoftNIC PMD infrastructure has been restructured to use the Packet
  Framework, which makes it more flexible, modular and easier to add new
  functionality in the future.

* **Updated the AESNI MB PMD.**

  The AESNI MB PMD has been updated with additional support for:

  * 3DES for 8, 16 and 24 byte keys.

* **Added a new compression PMD using Intel's QuickAssist (QAT) device family.**

  Added the new ``QAT`` compression driver, for compression and decompression
  operations in software. See the :doc:`../compressdevs/qat_comp` compression
  driver guide for details on this new driver.

* **Updated the ISA-L PMD.**

  Added support for chained mbufs (input and output).


API Changes
-----------

.. This section should contain API changes. Sample format:

   * Add a short 1-2 sentence description of the API change.
     Use fixed width quotes for ``function_names`` or ``struct_names``.
     Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* The path to the runtime config file has changed. The new path is determined
  as follows:

  - If DPDK is running as root, ``/var/run/dpdk/<prefix>/config``
  - If DPDK is not running as root:

    * If ``$XDG_RUNTIME_DIR`` is set, ``${XDG_RUNTIME_DIR}/dpdk/<prefix>/config``
    * Otherwise, ``/tmp/dpdk/<prefix>/config``

* eal: The function ``rte_eal_mbuf_default_mempool_ops`` was deprecated
  and is removed in 18.08. It shall be replaced by
  ``rte_mbuf_best_mempool_ops``.

* mempool: Following functions were deprecated and are removed in 18.08:

  - ``rte_mempool_populate_iova_tab``
  - ``rte_mempool_populate_phys_tab``
  - ``rte_mempool_populate_phys`` (``rte_mempool_populate_iova`` should be used)
  - ``rte_mempool_virt2phy`` (``rte_mempool_virt2iova`` should be used)
  - ``rte_mempool_xmem_create``
  - ``rte_mempool_xmem_size``
  - ``rte_mempool_xmem_usage``

* ethdev: The old offload API is removed:

  - Rx per-port ``rte_eth_conf.rxmode.[bit-fields]``
  - Tx per-queue ``rte_eth_txconf.txq_flags``
  - ``ETH_TXQ_FLAGS_NO*``

  The transition bits are removed:

  - ``rte_eth_conf.rxmode.ignore_offload_bitfield``
  - ``ETH_TXQ_FLAGS_IGNORE``

* cryptodev: The following API changes have been made in 18.08:

  - In struct ``struct rte_cryptodev_info``, field ``rte_pci_device *pci_dev``
    has been replaced with field ``struct rte_device *device``.
  - Value 0 is accepted in ``sym.max_nb_sessions``, meaning that a device
    supports an unlimited number of sessions.
  - Two new fields of type ``uint16_t`` have been added:
    ``min_mbuf_headroom_req`` and ``min_mbuf_tailroom_req``.  These parameters
    specify the recommended headroom and tailroom for mbufs to be processed by
    the PMD.

* cryptodev: The following functions were deprecated and are removed in 18.08:

  - ``rte_cryptodev_queue_pair_start``
  - ``rte_cryptodev_queue_pair_stop``
  - ``rte_cryptodev_queue_pair_attach_sym_session``
  - ``rte_cryptodev_queue_pair_detach_sym_session``

* cryptodev: The following functions were deprecated and are replaced by other
  functions in 18.08:

  - ``rte_cryptodev_get_header_session_size`` is replaced with
    ``rte_cryptodev_sym_get_header_session_size``
  - ``rte_cryptodev_get_private_session_size`` is replaced with
    ``rte_cryptodev_sym_get_private_session_size``

* cryptodev: Feature flag ``RTE_CRYPTODEV_FF_MBUF_SCATTER_GATHER`` is
  replaced with the following more explicit flags:

  - ``RTE_CRYPTODEV_FF_IN_PLACE_SGL``
  - ``RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT``
  - ``RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT``
  - ``RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT``
  - ``RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT``

* cryptodev: Renamed cryptodev experimental APIs:

  Used user_data instead of private_data in following APIs to avoid confusion
  with the existing session parameter ``sess_private_data[]`` and related APIs.

  - ``rte_cryptodev_sym_session_set_private_data()`` changed to
    ``rte_cryptodev_sym_session_set_user_data()``
  - ``rte_cryptodev_sym_session_get_private_data()`` changed to
    ``rte_cryptodev_sym_session_get_user_data()``

* compressdev: Feature flag ``RTE_COMP_FF_MBUF_SCATTER_GATHER`` is
  replaced with the following more explicit flags:

  - ``RTE_COMP_FF_OOP_SGL_IN_SGL_OUT``
  - ``RTE_COMP_FF_OOP_SGL_IN_LB_OUT``
  - ``RTE_COMP_FF_OOP_LB_IN_SGL_OUT``


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
     librte_bus_dpaa.so.1
     librte_bus_fslmc.so.1
     librte_bus_pci.so.1
     librte_bus_vdev.so.1
   + librte_bus_vmbus.so.1
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_common_octeontx.so.1
     librte_compressdev.so.1
   + librte_cryptodev.so.5
     librte_distributor.so.1
   + librte_eal.so.8
   + librte_ethdev.so.10
   + librte_eventdev.so.5
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
   + librte_mempool.so.5
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

* Intel(R) platforms with Intel(R) NICs combinations

   * CPU

     * Intel(R) Atom(TM) CPU C3858 @ 2.00GHz
     * Intel(R) Xeon(R) CPU D-1541 @ 2.10GHz
     * Intel(R) Xeon(R) CPU E5-4667 v3 @ 2.00GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2699 v4 @ 2.20GHz
     * Intel(R) Xeon(R) CPU E5-2695 v4 @ 2.10GHz
     * Intel(R) Xeon(R) CPU E5-2658 v2 @ 2.40GHz
     * Intel(R) Xeon(R) CPU E5-2658 v3 @ 2.20GHz
     * Intel(R) Xeon(R) Platinum 8180 CPU @ 2.50GHz

   * OS:

     * CentOS 7.4
     * Fedora 25
     * Fedora 27
     * Fedora 28
     * FreeBSD 11.1
     * Red Hat Enterprise Linux Server release 7.5
     * SUSE Enterprise Linux 12
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

     * Intel Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

       * Firmware version: 3.33 0x80000fd5 0.0.0
       * Device id (pf/vf): 8086:37d0 / 8086:37cd
       * Driver version: 2.4.3 (i40e)

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

     * Red Hat Enterprise Linux Server release 7.5 (Maipo)
     * Red Hat Enterprise Linux Server release 7.4 (Maipo)
     * Red Hat Enterprise Linux Server release 7.3 (Maipo)
     * Red Hat Enterprise Linux Server release 7.2 (Maipo)
     * Ubuntu 18.04
     * Ubuntu 17.10
     * Ubuntu 16.04
     * SUSE Linux Enterprise Server 15

   * MLNX_OFED: 4.3-2.0.2.0
   * MLNX_OFED: 4.4-2.0.1.0

   * NICs:

     * Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1007
       * Firmware version: 2.42.5000

     * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.21.1000 and above

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.21.1000 and above

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.21.1000 and above

     * Mellanox(R) ConnectX-5 Ex EN 100G MCX516A-CDAT (2x100G)

       * Host interface: PCI Express 4.0 x16
       * Device ID: 15b3:1019
       * Firmware version: 16.21.1000 and above

* ARM platforms with Mellanox(R) NICs combinations

   * CPU:

     * Qualcomm ARM 1.1 2500MHz

   * OS:

     * Red Hat Enterprise Linux Server release 7.5 (Maipo)

   * NICs:

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.23.1000

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.23.1000

* Mellanox BlueField SmartNIC

   * Mellanox(R) BlueField SmartNIC MT416842 (2x25G)
       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:a2d2
       * Firmware version: 18.99.3950

   * SoC ARM cores running OS:
     * CentOS Linux release 7.4.1708 (AltArch)
     * Mellanox MLNX_OFED 4.2-1.4.21.0

  * DPDK application running on ARM cores inside SmartNIC
  * BlueField representors support planned for next release.
