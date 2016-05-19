ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* The log history is deprecated.
  It is voided in 16.07 and will be removed in release 16.11.

* The ethdev library file will be renamed from libethdev.* to librte_ethdev.*
  in release 16.11 in order to have a more consistent namespace.

* In 16.11 ABI changes are planned: the ``rte_eth_dev`` structure will be
  extended with new function pointer ``tx_pkt_prep`` allowing verification
  and processing of packet burst to meet HW specific requirements before
  transmit. Also new fields will be added to the ``rte_eth_desc_lim`` structure:
  ``nb_seg_max`` and ``nb_mtu_seg_max`` providing information about number of
  segments limit to be transmitted by device for TSO/non-TSO packets.

* The ethdev hotplug API is going to be moved to EAL with a notification
  mechanism added to crypto and ethdev libraries so that hotplug is now
  available to both of them. This API will be stripped of the device arguments
  so that it only cares about hotplugging.

* Structures embodying pci and vdev devices are going to be reworked to
  integrate new common rte_device / rte_driver objects (see
  http://dpdk.org/ml/archives/dev/2016-January/031390.html).
  ethdev and crypto libraries will then only handle those objects so that they
  do not need to care about the kind of devices that are being used, making it
  easier to add new buses later.

* ABI changes are planned for 16.11 in the ``rte_mbuf`` structure: some fields
  may be reordered to facilitate the writing of ``data_off``, ``refcnt``, and
  ``nb_segs`` in one operation, because some platforms have an overhead if the
  store address is not naturally aligned. Other mbuf fields, such as the
  ``port`` field, may be moved or removed as part of this mbuf work.

* The mbuf flags PKT_RX_VLAN_PKT and PKT_RX_QINQ_PKT are deprecated and
  are respectively replaced by PKT_RX_VLAN_STRIPPED and
  PKT_RX_QINQ_STRIPPED, that are better described. The old flags and
  their behavior will be kept in 16.07 and will be removed in 16.11.

* The APIs rte_mempool_count and rte_mempool_free_count are being deprecated
  on the basis that they are confusing to use - free_count actually returns
  the number of allocated entries, not the number of free entries as expected.
  They are being replaced by rte_mempool_avail_count and
  rte_mempool_in_use_count respectively.

* The mempool functions for single/multi producer/consumer are deprecated and
  will be removed in 16.11.
  It is replaced by rte_mempool_generic_get/put functions.

* The ``rte_ivshmem`` feature (including library and EAL code) will be removed
  in 16.11 because it has some design issues which are not planned to be fixed.

* The vhost-cuse will be removed in 16.11. Since v2.1, a large majority of
  development effort has gone to vhost-user, such as multiple-queue, live
  migration, reconnect etc. Therefore, vhost-user should be used instead.

* Driver names are quite inconsistent among each others and they will be
  renamed to something more consistent (net and crypto prefixes) in 16.11.
  Some of these driver names are used publicly, to create virtual devices,
  so a deprecation notice is necessary.

* API will change for ``rte_port_source_params`` and ``rte_port_sink_params``
  structures. The member ``file_name`` data type will be changed from
  ``char *`` to ``const char *``. This change targets release 16.11.
