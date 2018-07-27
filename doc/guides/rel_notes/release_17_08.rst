..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2017 The DPDK contributors

DPDK Release 17.08
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_17_08.html


New Features
------------

.. This section should contain new features added in this release. Sample
   format:

   * **Add a title in the past tense with a full stop.**

     Add a short 1-2 sentence description in the past tense. The description
     should be enough to allow someone scanning the release notes to
     understand the new feature.

     If the feature adds a lot of sub-features you can use a bullet list like
     this:

     * Added feature foo to do something.
     * Enhanced feature bar to do something else.

     Refer to the previous release notes for examples.

     This section is a comment. do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =========================================================

* **Increase minimum x86 ISA version to SSE4.2.**

  Starting with version 17.08, DPDK requires SSE4.2 to run on x86.
  Previous versions required SSE3.

* **Added Service Core functionality.**

  The service core functionality added to EAL allows DPDK to run services such
  as software PMDs on lcores without the application manually running them. The
  service core infrastructure allows flexibility of running multiple services
  on the same service lcore, and provides the application with powerful APIs to
  configure the mapping from service lcores to services.

* **Added Generic Receive Offload API.**

  Added Generic Receive Offload (GRO) API support to reassemble TCP/IPv4
  packets. The GRO API assumes all input packets have the correct
  checksums. GRO API doesn't update checksums for merged packets. If
  input packets are IP fragmented, the GRO API assumes they are complete
  packets (i.e. with L4 headers).

* **Added Fail-Safe PMD**

  Added the new Fail-Safe PMD. This virtual device allows applications to
  support seamless hotplug of devices.
  See the :doc:`/nics/fail_safe` guide for more details about this driver.

* **Added support for generic flow API (rte_flow) on igb NICs.**

  This API provides a generic means of configuring hardware to match specific
  ingress or egress traffic, altering its behavior and querying related counters
  according to any number of user-defined rules.

  Added generic flow API support for Ethernet, IPv4, UDP, TCP and RAW pattern
  items with QUEUE actions. There are four types of filter support for this
  feature on igb.

* **Added support for generic flow API (rte_flow) on enic.**

  Added flow API support for outer Ethernet, VLAN, IPv4, IPv6, UDP, TCP, SCTP,
  VxLAN and inner Ethernet, VLAN, IPv4, IPv6, UDP and TCP pattern items with
  QUEUE, MARK, FLAG and VOID actions for ingress traffic.

* **Added support for Chelsio T6 family of adapters**

  The CXGBE PMD was updated to run Chelsio T6 family of adapters.

* **Added latency and performance improvements for cxgbe**

  the Tx and Rx path in cxgbe were reworked to improve performance. In
  addition the latency was reduced for slow traffic.

* **Updated the bnxt PMD.**

  Updated the bnxt PMD. The major enhancements include:

  * Support MTU modification.
  * Add support for LRO.
  * Add support for VLAN filter and strip functionality.
  * Additional enhancements to add support for more dev_ops.
  * Added PMD specific APIs mainly to control VF from PF.
  * Update HWRM version to 1.7.7

* **Added support for Rx interrupts on mlx4 driver.**

  Rx queues can be now be armed with an interrupt which will trigger on the
  next packet arrival.

* **Updated mlx5 driver.**

  Updated the mlx5 driver including the following changes:

  * Added vectorized Rx/Tx burst for x86.
  * Added support for isolated mode from flow API.
  * Reworked the flow drop action to implement in hardware classifier.
  * Improved Rx interrupts management.

* **Updated szedata2 PMD.**

  Added support for firmware with multiple Ethernet ports per physical port.

* **Updated dpaa2 PMD.**

  Updated dpaa2 PMD. Major enhancements include:

  * Added support for MAC Filter configuration.
  * Added support for Segmented Buffers.
  * Added support for VLAN filter and strip functionality.
  * Additional enhancements to add support for more dev_ops.
  * Optimized the packet receive path

* **Reorganized the symmetric crypto operation structure.**

  The crypto operation (``rte_crypto_sym_op``) has been reorganized as follows:

  * Removed the ``rte_crypto_sym_op_sess_type`` field.
  * Replaced the pointer and physical address of IV with offset from the start
    of the crypto operation.
  * Moved length and offset of cipher IV to ``rte_crypto_cipher_xform``.
  * Removed "Additional Authentication Data" (AAD) length.
  * Removed digest length.
  * Removed AAD pointer and physical address from ``auth`` structure.
  * Added ``aead`` structure, containing parameters for AEAD algorithms.

* **Reorganized the crypto operation structure.**

  The crypto operation (``rte_crypto_op``) has been reorganized as follows:

  * Added the ``rte_crypto_op_sess_type`` field.
  * The enumerations ``rte_crypto_op_status`` and ``rte_crypto_op_type``
    have been modified to be ``uint8_t`` values.
  * Removed the field ``opaque_data``.
  * Pointer to ``rte_crypto_sym_op`` has been replaced with a zero length array.

* **Reorganized the crypto symmetric session structure.**

  The crypto symmetric session structure (``rte_cryptodev_sym_session``) has
  been reorganized as follows:

  * The ``dev_id`` field has been removed.
  * The ``driver_id`` field has been removed.
  * The mempool pointer ``mp`` has been removed.
  * Replaced ``private`` marker with array of pointers to private data sessions
    ``sess_private_data``.

* **Updated cryptodev library.**

  * Added AEAD algorithm specific functions and structures, so it is not
    necessary to use a combination of cipher and authentication
    structures anymore.
  * Added helper functions for crypto device driver identification.
  * Added support for multi-device sessions, so a single session can be
    used in multiple drivers.
  * Added functions to initialize and free individual driver private data
    with the same session.

* **Updated dpaa2_sec crypto PMD.**

  Added support for AES-GCM and AES-CTR.

* **Updated the AESNI MB PMD.**

  The AESNI MB PMD has been updated with additional support for:

  * 12-byte IV on AES Counter Mode, apart from the previous 16-byte IV.

* **Updated the AES-NI GCM PMD.**

  The AES-NI GCM PMD was migrated from the ISA-L library to the Multi Buffer
  library, as the latter library has Scatter Gather List support
  now. The migration entailed adding additional support for 192-bit keys.

* **Updated the Cryptodev Scheduler PMD.**

  Added a multicore based distribution mode, which distributes the enqueued
  crypto operations among several slaves, running on different logical cores.

* **Added NXP DPAA2 Eventdev PMD.**

  Added the new dpaa2 eventdev driver for NXP DPAA2 devices. See the
  "Event Device Drivers" document for more details on this new driver.

* **Added dpdk-test-eventdev test application.**

  The dpdk-test-eventdev tool is a Data Plane Development Kit (DPDK) application
  that allows exercising various eventdev use cases.
  This application has a generic framework to add new eventdev based test cases
  to verify functionality and measure the performance parameters of DPDK
  eventdev devices.


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue in the present
     tense. Add information on any known workarounds.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **Starting with version 17.08, libnuma is required to build DPDK.**


API Changes
-----------

.. This section should contain API changes. Sample format:

   * Add a short 1-2 sentence description of the API change. Use fixed width
     quotes for ``rte_function_names`` or ``rte_struct_names``. Use the past
     tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **Modified the _rte_eth_dev_callback_process function in the ethdev library.**

  The function ``_rte_eth_dev_callback_process()`` has been modified. The
  return value has been changed from void to int and an extra parameter ``void
  *ret_param`` has been added.

* **Moved bypass functions from the rte_ethdev library to ixgbe PMD**

  * The following rte_ethdev library functions were removed:

    * ``rte_eth_dev_bypass_event_show()``
    * ``rte_eth_dev_bypass_event_store()``
    * ``rte_eth_dev_bypass_init()``
    * ``rte_eth_dev_bypass_state_set()``
    * ``rte_eth_dev_bypass_state_show()``
    * ``rte_eth_dev_bypass_ver_show()``
    * ``rte_eth_dev_bypass_wd_reset()``
    * ``rte_eth_dev_bypass_wd_timeout_show()``
    * ``rte_eth_dev_wd_timeout_store()``

  * The following ixgbe PMD functions were added:

    * ``rte_pmd_ixgbe_bypass_event_show()``
    * ``rte_pmd_ixgbe_bypass_event_store()``
    * ``rte_pmd_ixgbe_bypass_init()``
    * ``rte_pmd_ixgbe_bypass_state_set()``
    * ``rte_pmd_ixgbe_bypass_state_show()``
    * ``rte_pmd_ixgbe_bypass_ver_show()``
    * ``rte_pmd_ixgbe_bypass_wd_reset()``
    * ``rte_pmd_ixgbe_bypass_wd_timeout_show()``
    * ``rte_pmd_ixgbe_bypass_wd_timeout_store()``

* **Reworked rte_cryptodev library.**

  The rte_cryptodev library has been reworked and updated. The following changes
  have been made to it:

  * The crypto device type enumeration has been removed from cryptodev library.
  * The function ``rte_crypto_count_devtype()`` has been removed, and replaced
    by the new function ``rte_crypto_count_by_driver()``.
  * Moved crypto device driver names definitions to the particular PMDs.
    These names are not public anymore.
  * The ``rte_cryptodev_configure()`` function does not create the session
    mempool for the device anymore.
  * The ``rte_cryptodev_queue_pair_attach_sym_session()`` and
    ``rte_cryptodev_queue_pair_dettach_sym_session()`` functions require
    the new parameter ``device id``.
  * Parameters of ``rte_cryptodev_sym_session_create()`` were modified to
    accept ``mempool``, instead of ``device id`` and ``rte_crypto_sym_xform``.
  * Removed ``device id`` parameter from ``rte_cryptodev_sym_session_free()``.
  * Added a new field ``session_pool`` to ``rte_cryptodev_queue_pair_setup()``.
  * Removed ``aad_size`` parameter from
    ``rte_cryptodev_sym_capability_check_auth()``.
  * Added ``iv_size`` parameter to
    ``rte_cryptodev_sym_capability_check_auth()``.
  * Removed ``RTE_CRYPTO_OP_STATUS_ENQUEUED`` from enum
    ``rte_crypto_op_status``.


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * Add a short 1-2 sentence description of the ABI change that was announced
     in the previous releases and made in this release. Use fixed width quotes
     for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* Changed type of ``domain`` field in ``rte_pci_addr`` to ``uint32_t``
  to follow the PCI standard.

* Added new ``rte_bus`` experimental APIs available as operators within the
  ``rte_bus`` structure.

* Made ``rte_devargs`` structure internal device representation generic to
  prepare for a bus-agnostic EAL.

* **Reorganized the crypto operation structures.**

  Some fields have been modified in the ``rte_crypto_op`` and
  ``rte_crypto_sym_op`` structures, as described in the `New Features`_
  section.

* **Reorganized the crypto symmetric session structure.**

  Some fields have been modified in the ``rte_cryptodev_sym_session``
  structure, as described in the `New Features`_ section.

* **Reorganized the rte_crypto_sym_cipher_xform structure.**

  * Added cipher IV length and offset parameters.
  * Changed field size of key length from ``size_t`` to ``uint16_t``.

* **Reorganized the rte_crypto_sym_auth_xform structure.**

  * Added authentication IV length and offset parameters.
  * Changed field size of AAD length from ``uint32_t`` to ``uint16_t``.
  * Changed field size of digest length from ``uint32_t`` to ``uint16_t``.
  * Removed AAD length.
  * Changed field size of key length from ``size_t`` to ``uint16_t``.

* Replaced ``dev_type`` enumeration with ``uint8_t`` ``driver_id`` in
  ``rte_cryptodev_info`` and  ``rte_cryptodev`` structures.

* Removed ``session_mp`` from ``rte_cryptodev_config``.


Shared Library Versions
-----------------------

.. Update any library version updated in this release and prepend with a ``+``
   sign, like this:

     librte_acl.so.2
   + librte_cfgfile.so.2
     librte_cmdline.so.2

   This section is a comment. do not overwrite or remove it.
   =========================================================


The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
     librte_bitratestats.so.1
     librte_cfgfile.so.2
     librte_cmdline.so.2
   + librte_cryptodev.so.3
     librte_distributor.so.1
   + librte_eal.so.5
   + librte_ethdev.so.7
   + librte_eventdev.so.2
   + librte_gro.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_latencystats.so.1
     librte_lpm.so.2
     librte_mbuf.so.3
     librte_mempool.so.2
     librte_meter.so.1
     librte_metrics.so.1
     librte_net.so.1
     librte_pdump.so.1
     librte_pipeline.so.3
     librte_pmd_bond.so.1
     librte_pmd_ring.so.2
     librte_port.so.3
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
     librte_table.so.2
     librte_timer.so.1
     librte_vhost.so.3


Tested Platforms
----------------

.. This section should contain a list of platforms that were tested with this
   release.

   The format is:

   * <vendor> platform with <vendor> <type of devices> combinations

     * List of CPU
     * List of OS
     * List of devices
     * Other relevant details...

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* Intel(R) platforms with Mellanox(R) NICs combinations

   * Platform details:

     * Intel(R) Xeon(R) CPU E5-2697A v4 @ 2.60GHz
     * Intel(R) Xeon(R) CPU E5-2697 v3 @ 2.60GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2640 @ 2.50GHz

   * OS:

     * Red Hat Enterprise Linux Server release 7.3 (Maipo)
     * Red Hat Enterprise Linux Server release 7.2 (Maipo)
     * Ubuntu 16.10
     * Ubuntu 16.04
     * Ubuntu 14.04

   * MLNX_OFED: 4.1-1.0.2.0

   * NICs:

     * Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1007
       * Firmware version: 2.40.5030

     * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT
       (2x50G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.18.2000

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.18.2000

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.19.1200

     * Mellanox(R) ConnectX-5 Ex EN 100G MCX516A-CDAT (2x100G)

       * Host interface: PCI Express 4.0 x16
       * Device ID: 15b3:1019
       * Firmware version: 16.19.1200

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
       * Driver version: 4.0.1-k (ixgbe)

     * Intel(R) Corporation Ethernet Connection X552/X557-AT 10GBASE-T

       * Firmware version: 0x800001cf
       * Device id (pf/vf): 8086:15ad / 8086:15a8
       * Driver version: 4.2.5 (ixgbe)

     * Intel(R) Ethernet Converged Network Adapter X710-DA4 (4x10G)

       * Firmware version: 6.01 0x80003205
       * Device id (pf/vf): 8086:1572 / 8086:154c
       * Driver version: 2.0.19 (i40e)

     * Intel(R) Ethernet Converged Network Adapter X710-DA2 (2x10G)

       * Firmware version: 6.01 0x80003204
       * Device id (pf/vf): 8086:1572 / 8086:154c
       * Driver version: 2.0.19 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

       * Firmware version: 6.01 0x80003221
       * Device id (pf/vf): 8086:158b
       * Driver version: 2.0.19 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

       * Firmware version: 6.01 0x8000321c
       * Device id (pf/vf): 8086:1583 / 8086:154c
       * Driver version: 2.0.19 (i40e)

     * Intel(R) Corporation I350 Gigabit Network Connection

       * Firmware version: 1.48, 0x800006e7
       * Device id (pf/vf): 8086:1521 / 8086:1520
       * Driver version: 5.2.13-k (igb)
