ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* igb_uio: iomem mapping and sysfs files created for iomem and ioport in
  igb_uio will be removed, because we are able to detect these from what Linux
  has exposed, like the way we have done with uio-pci-generic. This change
  targets release 17.02.

* ABI/API changes are planned for 17.02: ``rte_device``, ``rte_driver`` will be
  impacted because of introduction of a new ``rte_bus`` hierarchy. This would
  also impact the way devices are identified by EAL. A bus-device-driver model
  will be introduced providing a hierarchical view of devices.

* ``eth_driver`` is planned to be removed in 17.02. This currently serves as
  a placeholder for PMDs to register themselves. Changes for ``rte_bus`` will
  provide a way to handle device initialization currently being done in
  ``eth_driver``.

* In 17.02 ABI changes are planned: the ``rte_eth_dev`` structure will be
  extended with new function pointer ``tx_pkt_prepare`` allowing verification
  and processing of packet burst to meet HW specific requirements before
  transmit. Also new fields will be added to the ``rte_eth_desc_lim`` structure:
  ``nb_seg_max`` and ``nb_mtu_seg_max`` providing information about number of
  segments limit to be transmitted by device for TSO/non-TSO packets.

* In 17.02 ABI change is planned: the ``rte_eth_dev_info`` structure
  will be extended with a new member ``fw_version`` in order to store
  the NIC firmware version.

* ethdev: an API change is planned for 17.02 for the function
  ``_rte_eth_dev_callback_process``. In 17.02 the function will return an ``int``
  instead of ``void`` and a fourth parameter ``void *ret_param`` will be added.

* ethdev: for 17.02 it is planned to deprecate the following five functions
  and move them in ixgbe:

  ``rte_eth_dev_set_vf_rxmode``

  ``rte_eth_dev_set_vf_rx``

  ``rte_eth_dev_set_vf_tx``

  ``rte_eth_dev_set_vf_vlan_filter``

  ``rte_eth_set_vf_rate_limit``

* ABI changes are planned for 17.02 in the ``rte_mbuf`` structure: some fields
  may be reordered to facilitate the writing of ``data_off``, ``refcnt``, and
  ``nb_segs`` in one operation, because some platforms have an overhead if the
  store address is not naturally aligned. Other mbuf fields, such as the
  ``port`` field, may be moved or removed as part of this mbuf work. A
  ``timestamp`` will also be added.

* The mbuf flags PKT_RX_VLAN_PKT and PKT_RX_QINQ_PKT are deprecated and
  are respectively replaced by PKT_RX_VLAN_STRIPPED and
  PKT_RX_QINQ_STRIPPED, that are better described. The old flags and
  their behavior will be kept until 16.11 and will be removed in 17.02.

* mempool: The functions ``rte_mempool_count`` and ``rte_mempool_free_count``
  will be removed in 17.02.
  They are replaced by ``rte_mempool_avail_count`` and
  ``rte_mempool_in_use_count`` respectively.

* mempool: The functions for single/multi producer/consumer are deprecated
  and will be removed in 17.02.
  It is replaced by ``rte_mempool_generic_get/put`` functions.
