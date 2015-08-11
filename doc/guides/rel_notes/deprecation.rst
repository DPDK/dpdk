ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* Significant ABI changes are planned for struct rte_eth_dev to support up to
  1024 queues per port. This change will be in release 2.2.
  There is no backward compatibility planned from release 2.2.
  All binaries will need to be rebuilt from release 2.2.

* The EAL function rte_eal_pci_close_one is deprecated because renamed to
  rte_eal_pci_detach.

* The Macros RTE_HASH_BUCKET_ENTRIES_MAX and RTE_HASH_KEY_LENGTH_MAX are
  deprecated and will be removed with version 2.2.

* Significant ABI changes are planned for struct rte_mbuf, struct rte_kni_mbuf,
  and several ``PKT_RX_`` flags will be removed, to support unified packet type
  from release 2.1. Those changes may be enabled in the upcoming release 2.1
  with CONFIG_RTE_NEXT_ABI.

* librte_malloc library has been integrated into librte_eal. The 2.1 release
  creates a dummy/empty malloc library to fulfill binaries with dynamic linking
  dependencies on librte_malloc.so. Such dummy library will not be created from
  release 2.2 so binaries will need to be rebuilt.

* The following fields have been deprecated in rte_eth_stats:
  imissed, ibadcrc, ibadlen, imcasts, fdirmatch, fdirmiss,
  tx_pause_xon, rx_pause_xon, tx_pause_xoff, rx_pause_xoff
