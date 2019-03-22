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

* eal: both declaring and identifying devices will be streamlined in v18.11.
  New functions will appear to query a specific port from buses, classes of
  device and device drivers. Device declaration will be made coherent with the
  new scheme of device identification.
  As such, ``rte_devargs`` device representation will change.

  - The enum ``rte_devtype`` was used to identify a bus and will disappear.
  - Functions previously deprecated will change or disappear:

    + ``rte_eal_devargs_type_count``

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

* ethdev: the legacy filter API, including
  ``rte_eth_dev_filter_supported()``, ``rte_eth_dev_filter_ctrl()`` as well
  as filter types MACVLAN, ETHERTYPE, FLEXIBLE, SYN, NTUPLE, TUNNEL, FDIR,
  HASH and L2_TUNNEL, is superseded by the generic flow API (rte_flow) in
  PMDs that implement the latter.
  Target release for removal of the legacy API will be defined once most
  PMDs have switched to rte_flow.

* kni: remove KNI ethtool support. To clarify, this is not to remove the KNI,
  but only to remove ethtool support of it that is disabled by default and
  can be enabled via ``CONFIG_RTE_KNI_KMOD_ETHTOOL`` config option.
  Existing KNI ethtool implementation is only supported by ``igb`` & ``ixgbe``
  drivers, by using a copy of kernel drivers in DPDK. This model cannot be
  extended to all drivers in DPDK and it is too much effort to maintain
  kernel modules in DPDK. As a result users won't be able to use ``ethtool``
  via ``igb`` & ``ixgbe`` anymore.

* meter: New ``rte_color`` definition will be added in 19.02 and that will
  replace ``enum rte_meter_color`` in meter library in 19.05. This will help
  to consolidate color definition, which is currently replicated in many places,
  such as: rte_meter.h, rte_mtr.h, rte_tm.h.

* cryptodev: New member in ``rte_cryptodev_config`` to allow applications to
  disable features supported by the crypto device. Only the following features
  would be allowed to be disabled this way,

  - ``RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO``
  - ``RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO``
  - ``RTE_CRYPTODEV_FF_SECURITY``

  Disabling unused features would facilitate efficient usage of HW/SW offload.

  - Member ``uint64_t ff_disable`` in ``rte_cryptodev_config``

  The field would be added in v19.08.

* cryptodev: the ``uint8_t *data`` member of ``key`` structure in the xforms
  structure (``rte_crypto_cipher_xform``, ``rte_crypto_auth_xform``, and
  ``rte_crypto_aead_xform``) will be changed to ``const uint8_t *data``.
