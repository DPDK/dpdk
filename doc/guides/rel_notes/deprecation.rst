ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* In 16.11 ABI changes are planned: the ``rte_eth_dev`` structure will be
  extended with new function pointer ``tx_pkt_prep`` allowing verification
  and processing of packet burst to meet HW specific requirements before
  transmit. Also new fields will be added to the ``rte_eth_desc_lim`` structure:
  ``nb_seg_max`` and ``nb_mtu_seg_max`` providing information about number of
  segments limit to be transmitted by device for TSO/non-TSO packets.

* ABI changes are planned for 16.11 in the ``rte_mbuf`` structure: some fields
  may be reordered to facilitate the writing of ``data_off``, ``refcnt``, and
  ``nb_segs`` in one operation, because some platforms have an overhead if the
  store address is not naturally aligned. Other mbuf fields, such as the
  ``port`` field, may be moved or removed as part of this mbuf work.

* The mbuf flags PKT_RX_VLAN_PKT and PKT_RX_QINQ_PKT are deprecated and
  are respectively replaced by PKT_RX_VLAN_STRIPPED and
  PKT_RX_QINQ_STRIPPED, that are better described. The old flags and
  their behavior will be kept in 16.07 and will be removed in 16.11.

* mempool: The functions ``rte_mempool_count`` and ``rte_mempool_free_count``
  will be removed in 17.02.
  They are replaced by ``rte_mempool_avail_count`` and
  ``rte_mempool_in_use_count`` respectively.

* mempool: The functions for single/multi producer/consumer are deprecated
  and will be removed in 17.02.
  It is replaced by ``rte_mempool_generic_get/put`` functions.
