..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* meson: The minimum supported version of meson for configuring and building
  DPDK will be increased to v0.47.1 (from 0.41) from DPDK 19.05 onwards. For
  those users with a version earlier than 0.47.1, an updated copy of meson
  can be got using the ``pip``, or ``pip3``, tool for downloading python
  packages.

* kvargs: The function ``rte_kvargs_process`` will get a new parameter
  for returning key match count. It will ease handling of no-match case.

* eal: The function ``rte_eal_remote_launch`` will return new error codes
  after read or write error on the pipe, instead of calling ``rte_panic``.

* eal: The ``lcore_config`` struct and global symbol will be made private to
  remove it from the externally visible ABI and allow it to be updated in the
  future.

* eal: both declaring and identifying devices will be streamlined in v18.11.
  New functions will appear to query a specific port from buses, classes of
  device and device drivers. Device declaration will be made coherent with the
  new scheme of device identification.
  As such, ``rte_devargs`` device representation will change.

  - The enum ``rte_devtype`` was used to identify a bus and will disappear.
  - Functions previously deprecated will change or disappear:

    + ``rte_eal_devargs_type_count``

* eal: The ``rte_cpu_check_supported`` function has been deprecated since
  v17.08 and will be removed.

* eal: The ``rte_malloc_virt2phy`` function has been deprecated and replaced
  by ``rte_malloc_virt2iova`` since v17.11 and will be removed.

* vfio: removal of ``rte_vfio_dma_map`` and ``rte_vfio_dma_unmap`` APIs which
  have been replaced with ``rte_dev_dma_map`` and ``rte_dev_dma_unmap``
  functions.  The due date for the removal targets DPDK 20.02.

* pci: Several exposed functions are misnamed.
  The following functions are deprecated starting from v17.11 and are replaced:

  - ``eal_parse_pci_BDF`` replaced by ``rte_pci_addr_parse``
  - ``eal_parse_pci_DomBDF`` replaced by ``rte_pci_addr_parse``
  - ``rte_eal_compare_pci_addr`` replaced by ``rte_pci_addr_cmp``

* dpaa2: removal of ``rte_dpaa2_memsegs`` structure which has been replaced
  by a pa-va search library. This structure was earlier being used for holding
  memory segments used by dpaa2 driver for faster pa->va translation. This
  structure would be made internal (or removed if all dependencies are cleared)
  in future releases.

* net: The Ethernet address and header definitions will change
  attributes. The Ethernet address struct will no longer be marked as
  packed since the packed attribute is meaningless on a byte
  array. The Ethernet header will be marked as aligned on a 2-byte
  boundary and will no longer have the packed attribute. This allows
  for efficient access on CPU architectures where unaligned access is
  expensive. These changes should not impact normal usage because drivers
  naturally align the Ethernet header on receive and all known
  encapsulations preserve the alignment of the header.

* ethdev: The function ``rte_eth_dev_count`` will be removed in DPDK 20.02.
  It is replaced by the function ``rte_eth_dev_count_avail``.
  If the intent is to iterate over ports, ``RTE_ETH_FOREACH_*`` macros
  are better port iterators.

* ethdev: the legacy filter API, including
  ``rte_eth_dev_filter_supported()``, ``rte_eth_dev_filter_ctrl()`` as well
  as filter types MACVLAN, ETHERTYPE, FLEXIBLE, SYN, NTUPLE, TUNNEL, FDIR,
  HASH and L2_TUNNEL, is superseded by the generic flow API (rte_flow) in
  PMDs that implement the latter.
  Target release for removal of the legacy API will be defined once most
  PMDs have switched to rte_flow.

* ethdev: Update API functions returning ``void`` to return ``int`` with
  negative errno values to indicate various error conditions (e.g.
  invalid port ID, unsupported operation, failed operation):

  - ``rte_eth_dev_info_get``
  - ``rte_eth_promiscuous_enable`` and ``rte_eth_promiscuous_disable``
  - ``rte_eth_allmulticast_enable`` and ``rte_eth_allmulticast_disable``
  - ``rte_eth_link_get`` and ``rte_eth_link_get_nowait``
  - ``rte_eth_dev_stop``
  - ``rte_eth_dev_close``
  - ``rte_eth_xstats_reset``
  - ``rte_eth_macaddr_get``
  - ``rte_eth_dev_owner_delete``

* ethdev: New offload flags ``DEV_RX_OFFLOAD_RSS_HASH`` and
  ``DEV_RX_OFFLOAD_FLOW_MARK`` will be added in 19.11.
  This will allow application to enable or disable PMDs from updating
  ``rte_mbuf::hash::rss`` and ``rte_mbuf::hash::fdir`` respectively.
  This scheme will allow PMDs to avoid writes to ``rte_mbuf`` fields on Rx and
  thereby improve Rx performance if application wishes do so.
  In 19.11 PMDs will still update the fields even when the offloads are not
  enabled.

* ethdev: New function ``rte_eth_dev_set_supported_ptypes`` will be added in
  19.11.
  This will allow application to request PMD to set specific ptypes defined
  through ``rte_eth_dev_set_supported_ptypes`` in ``rte_mbuf::packet_type``.
  If application doesn't want any ptype information it can call
  ``rte_eth_dev_set_supported_ptypes(ethdev_id, RTE_PTYPE_UNKNOWN)`` and PMD
  will set ``rte_mbuf::packet_type`` to ``0``.
  If application doesn't call ``rte_eth_dev_set_supported_ptypes`` PMD can
  return ``rte_mbuf::packet_type`` with ``rte_eth_dev_get_supported_ptypes``.
  If application is interested only in L2/L3 layer, it can inform the PMD
  to update ``rte_mbuf::packet_type`` with L2/L3 ptype by calling
  ``rte_eth_dev_set_supported_ptypes(ethdev_id, RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK)``.
  This scheme will allow PMDs to avoid lookup to internal ptype table on Rx and
  thereby improve Rx performance if application wishes do so.

* cryptodev: support for using IV with all sizes is added, J0 still can
  be used but only when IV length in following structs ``rte_crypto_auth_xform``,
  ``rte_crypto_aead_xform`` is set to zero. When IV length is greater or equal
  to one it means it represents IV, when is set to zero it means J0 is used
  directly, in this case 16 bytes of J0 need to be passed.

* sched: To allow more traffic classes, flexible mapping of pipe queues to
  traffic classes, and subport level configuration of pipes and queues
  changes will be made to macros, data structures and API functions defined
  in "rte_sched.h". These changes are aligned to improvements suggested in the
  RFC https://mails.dpdk.org/archives/dev/2018-November/120035.html.

* metrics: The function ``rte_metrics_init`` will have a non-void return
  in order to notify errors instead of calling ``rte_exit``.

* power: ``rte_power_set_env`` function will no longer return 0 on attempt
  to set new power environment if power environment was already initialized.
  In this case the function will return -1 unless the environment is unset first
  (using ``rte_power_unset_env``). Other function usage scenarios will not change.
