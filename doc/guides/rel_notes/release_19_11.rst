..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 19.11
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_19_11.html


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

* **Added Lock-free Stack for aarch64.**

  The lock-free stack implementation is enabled for aarch64 platforms.

* **Added Hisilicon hns3 PMD.**

  Added the new ``hns3`` net driver for the inbuilt Hisilicon Network
  Subsystem 3(HNS3) network engine found in the Hisilicon Kunpeng 920 SoC.
  See the :doc:`../nics/hns3` guide for more details on this new driver.

* **Updated the Intel e1000 driver.**

  Added support for the ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.

* **Updated the Intel ixgbe driver.**

  Added support for the ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.

* **Updated the Intel i40e driver.**

  Added support for the ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.

* **Updated the Intel fm10k driver.**

  Added support for the ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.

* **Updated the Intel ice driver.**

  Updated the Intel ice driver with new features and improvements, including:

  * Added support for device-specific DDP package loading.
  * Added support for handling Receive Flex Descriptor.
  * Added support for protocol extraction on per Rx queue.
  * Added support for the ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.

* **Added cryptodev asymmetric session-less operation.**

  Added session-less option to cryptodev asymmetric structure. It works the same
  way as symmetric crypto, corresponding xform is used directly by the crypto op.

* **Added Marvell NITROX symmetric crypto PMD.**

  Added a symmetric crypto PMD for Marvell NITROX V security processor.
  See the :doc:`../cryptodevs/nitrox` guide for more details on this new

* **Updated NXP crypto PMDs for PDCP support.**

  PDCP support is added to DPAA_SEC and DPAA2_SEC PMDs using rte_security APIs.
  Support is added for all sequence number sizes for control and user plane.
  Test application is updated for unit testing.

* **Enabled Single Pass GCM acceleration on QAT GEN3.**

  Added support for Single Pass GCM, available on QAT GEN3 only (Intel
  QuickAssist Technology C4xxx). It is automatically chosen instead of the
  classic 2-pass mode when running on QAT GEN3, significantly improving
  the performance of AES GCM operations.

* **Updated the Intel QuickAssist Technology (QAT) compression PMD.**

  Added stateful decompression support in the Intel QuickAssist Technology PMD.
  Please note that stateful compression is not supported.

* **Introduced FIFO for NTB PMD.**

  Introduced FIFO for NTB (Non-transparent Bridge) PMD to support
  packet based processing.

* **Added eBPF JIT support for arm64.**

  Added eBPF JIT support for arm64 architecture to improve the eBPF program
  performance.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* Removed duplicated set of commands for Rx offload configuration from testpmd::

    port config all crc-strip|scatter|rx-cksum|rx-timestamp|
                    hw-vlan|hw-vlan-filter|hw-vlan-strip|hw-vlan-extend on|off

  The testpmd commands set that can be used instead
  in order to enable or disable Rx offloading on all Rx queues of a port is::

    port config <port_id> rx_offload crc_strip|scatter|
                                     ipv4_cksum|udp_cksum|tcp_cksum|timestamp|
                                     vlan_strip|vlan_filter|vlan_extend on|off


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

* ethdev: changed ``rte_eth_dev_infos_get`` return value from ``void`` to
  ``int`` to provide a way to report various error conditions.

* ethdev: changed ``rte_eth_promiscuous_enable`` and
  ``rte_eth_promiscuous_disable`` return value from ``void`` to ``int`` to
  provide a way to report various error conditions.

* ethdev: changed ``rte_eth_allmulticast_enable`` and
  ``rte_eth_allmulticast_disable`` return value from ``void`` to ``int`` to
  provide a way to report various error conditions.

* ethdev: changed ``rte_eth_dev_xstats_reset`` return value from ``void`` to
  ``int`` to provide a way to report various error conditions.

* ethdev: changed ``rte_eth_link_get`` and ``rte_eth_link_get_nowait``
  return value from ``void`` to ``int`` to provide a way to report various
  error conditions.

* ethdev: changed ``rte_eth_macaddr_get`` return value from ``void`` to
  ``int`` to provide a way to report various error conditions.

* ethdev: changed ``rte_eth_dev_owner_delete`` return value from ``void`` to
  ``int`` to provide a way to report various error conditions.

* event: The function ``rte_event_eth_tx_adapter_enqueue`` takes an additional
  input as ``flags``. Flag ``RTE_EVENT_ETH_TX_ADAPTER_ENQUEUE_SAME_DEST`` which
  has been introduced in this release is used when used when all the packets
  enqueued in the tx adapter are destined for the same Ethernet port & Tx queue.


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
     librte_cryptodev.so.8
     librte_distributor.so.1
     librte_eal.so.11
     librte_efd.so.1
   + librte_ethdev.so.13
   + librte_eventdev.so.8
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
     librte_rcu.so.1
     librte_reorder.so.1
     librte_ring.so.2
     librte_sched.so.3
     librte_security.so.2
     librte_stack.so.1
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

* **Updated Mellanox mlx5 driver.**

  Updated Mellanox mlx5 driver with new features and improvements, including:

  * Added support for VLAN pop flow offload command.
  * Added support for VLAN push flow offload command.
  * Added support for VLAN set PCP offload command.
  * Added support for VLAN set VID offload command.

