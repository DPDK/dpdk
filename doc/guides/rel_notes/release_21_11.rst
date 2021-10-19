.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 21.11
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_21_11.html


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
     * Device abstraction libs and PMDs (ordered alphabetically by vendor name)
       - ethdev (lib, PMDs)
       - cryptodev (lib, PMDs)
       - eventdev (lib, PMDs)
       - etc
     * Other libs
     * Apps, Examples, Tools (if significant)

     This section is a comment. Do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =======================================================

* **Enabled new devargs parser.**

  * Enabled devargs syntax
    ``bus=X,paramX=x/class=Y,paramY=y/driver=Z,paramZ=z``
  * Added bus-level parsing of the devargs syntax.
  * Kept compatibility with the legacy syntax as parsing fallback.

* **Updated EAL hugetlbfs mount handling for Linux.**

  * Modified to allow ``--huge-dir`` option to specify a sub-directory
    within a hugetlbfs mountpoint.

* **Added dmadev library.**

  * Added a DMA device framework for management and provision of
    hardware and software DMA devices.
  * Added generic API which support a number of different DMA
    operations.
  * Added multi-process support.

* **Added new RSS offload types for IPv4/L4 checksum in RSS flow.**

  Added macros ETH_RSS_IPV4_CHKSUM and ETH_RSS_L4_CHKSUM, now IPv4 and
  TCP/UDP/SCTP header checksum field can be used as input set for RSS.

* **Updated af_packet ethdev driver.**

  * Default VLAN strip behavior was changed. VLAN tag won't be stripped
    unless ``DEV_RX_OFFLOAD_VLAN_STRIP`` offload is enabled.

* **Updated Broadcom bnxt PMD.**

  * Added flow offload support for Thor.
  * Implement support for tunnel offload.
  * Updated HWRM API to version 1.10.2.44

* **Updated Intel iavf driver.**

  * Added Intel iavf support on Windows.
  * Added IPv4 and L4 (TCP/UDP/SCTP) checksum hash support in RSS flow.

* **Updated Intel ice driver.**

  * Added 1PPS out support by a devargs.
  * Added IPv4 and L4 (TCP/UDP/SCTP) checksum hash support in RSS flow.
  * Added DEV_RX_OFFLOAD_TIMESTAMP support.
  * Added timesync API support under scalar path.

* **Updated Intel ixgbe driver.**

  * Added Intel ixgbe support on Windows.

* **Updated Marvell cnxk ethdev driver.**

  * Added rte_flow support for dual VLAN insert and strip actions.
  * Added rte_tm support.
  * Added support for Inline IPsec for CN9K event mode and CN10K
    poll mode and event mode.

* **Updated Mellanox mlx5 driver.**

  Updated the Mellanox mlx5 driver with new features and improvements, including:

  * Added implicit mempool registration to avoid data path hiccups (opt-out).

* **Updated Marvell cnxk crypto PMD.**

  * Added AES-CBC SHA1-HMAC support in lookaside protocol (IPsec) for CN10K.
  * Added Transport mode support in lookaside protocol (IPsec) for CN10K.
  * Added UDP encapsulation support in lookaside protocol (IPsec) for CN10K.
  * Added support for lookaside protocol (IPsec) offload for CN9K.
  * Added support for ZUC algorithm with 256-bit key length for CN10K.
  * Added support for CN98xx dual block.
  * Added inner checksum support in lookaside protocol (IPsec) for CN10K.

* **Added support for event crypto adapter on Marvell CN10K and CN9K.**

  * Added event crypto adapter OP_FORWARD mode support.

* **Updated NXP dpaa_sec crypto PMD.**

  * Added DES-CBC, AES-XCBC-MAC, AES-CMAC and non-HMAC algo support.
  * Added PDCP short MAC-I support.

* **Updated NXP dpaa2_sec crypto PMD.**

  * Added PDCP short MAC-I support.

* **Updated the ACC100 bbdev PMD.**

  Added support for more comprehensive CRC options.

* **Updated the turbo_sw bbdev PMD.**

  Added support for more comprehensive CRC options.

* **Added multi-process support for testpmd.**

  Added command-line options to specify total number of processes and
  current process ID. Each process owns subset of Rx and Tx queues.

* **Updated test-crypto-perf application with new cases.**

  * Added support for asymmetric crypto throughput performance measurement.
    Only modex is supported for now.

* **Added lookaside protocol (IPsec) tests in dpdk-test.**

  * Added known vector tests (AES-GCM 128, 192, 256).
  * Added tests to verify error reporting with ICV corruption.
  * Added tests to verify IV generation.
  * Added tests to verify UDP encapsulation.
  * Added tests to verify UDP encapsulation ports.
  * Added tests to validate packets soft expiry.
  * Added tests to validate packets hard expiry.
  * Added tests to verify tunnel header verification in IPsec inbound.
  * Added tests to verify inner checksum.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* eal: Removed the deprecated function ``rte_get_master_lcore()``
  and the iterator macro ``RTE_LCORE_FOREACH_SLAVE``.

* eal: The old api arguments that were deprecated for
  blacklist/whitelist are removed. Users must use the new
  block/allow list arguments.

* mbuf: Removed offload flag ``PKT_RX_EIP_CKSUM_BAD``.
  ``PKT_RX_OUTER_IP_CKSUM_BAD`` should be used as a replacement.

* ethdev: Removed the port mirroring API. A more fine-grain flow API
  action ``RTE_FLOW_ACTION_TYPE_SAMPLE`` should be used instead.
  The structures ``rte_eth_mirror_conf`` and ``rte_eth_vlan_mirror`` and
  the functions ``rte_eth_mirror_rule_set`` and
  ``rte_eth_mirror_rule_reset`` along with the associated macros
  ``ETH_MIRROR_*`` are removed.

* i40e: Removed i40evf driver.
  iavf already became the default VF driver for i40e devices,
  so there is no need to maintain i40evf.


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
   =======================================================

* kvargs: The experimental function ``rte_kvargs_strcmp()`` has been
  removed. Its usages have been replaced by a new function
  ``rte_kvargs_get_with_value()``.

* mempool: Added ``RTE_MEMPOOL_F_NON_IO`` flag to give a hint to DPDK components
  that objects from this pool will not be used for device IO (e.g. DMA).

* mempool: The mempool flags ``MEMPOOL_F_*`` will be deprecated in the future.
  Newly added flags with ``RTE_MEMPOOL_F_`` prefix should be used instead.

* net: Renamed ``s_addr`` and ``d_addr`` fields of ``rte_ether_hdr`` structure
  to ``src_addr`` and ``dst_addr``, respectively.

* cryptodev: The API rte_cryptodev_pmd_is_valid_dev is modified to
  rte_cryptodev_is_valid_dev as it can be used by the application as
  well as PMD to check whether the device is valid or not.

* cryptodev: The rte_cryptodev_pmd.* files are renamed as cryptodev_pmd.*
  as it is for drivers only and should be private to DPDK, and not
  installed for app use.

* cryptodev: A ``reserved`` byte from structure ``rte_crypto_op`` was
  renamed to ``aux_flags`` to indicate warnings and other information from
  the crypto/security operation. This field will be used to communicate
  events such as soft expiry with IPsec in lookaside mode.


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
   =======================================================

* security: ``rte_security_set_pkt_metadata`` and ``rte_security_get_userdata``
  routines used by inline outbound and inline inbound security processing were
  made inline and enhanced to do simple 64-bit set/get for PMDs that do not
  have much processing in PMD specific callbacks but just 64-bit set/get.
  This avoids a per packet function pointer jump overhead for such PMDs.

* security: A new option ``iv_gen_disable`` was added in structure
  ``rte_security_ipsec_sa_options`` to disable IV generation inside PMD,
  so that application can provide its own IV and test known test vectors.

* security: A new option ``tunnel_hdr_verify`` was added in structure
  ``rte_security_ipsec_sa_options`` to indicate whether outer header
  verification need to be done as part of inbound IPsec processing.

* security: A new option ``udp_ports_verify`` was added in structure
  ``rte_security_ipsec_sa_options`` to indicate whether UDP ports
  verification need to be done as part of inbound IPsec processing.

* security: A new structure ``rte_security_ipsec_lifetime`` was added to
  replace ``esn_soft_limit`` in IPsec configuration structure
  ``rte_security_ipsec_xform`` to allow applications to configure SA soft
  and hard expiry limits. Limits can be either in number of packets or bytes.

* security: The new options ``ip_csum_enable`` and ``l4_csum_enable`` were added
  in structure ``rte_security_ipsec_sa_options`` to indicate whether inner
  packet IPv4 header checksum and L4 checksum need to be offloaded to
  security device.

* bbdev: Added capability related to more comprehensive CRC options,
  shifting values of the ``enum rte_bbdev_op_ldpcdec_flag_bitmasks``.


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue
     in the present tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================


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
   =======================================================
