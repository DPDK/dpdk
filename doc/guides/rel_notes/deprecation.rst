ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* The EAL function pci_config_space_set is deprecated in release 16.04
  and will be removed from 16.07.
  Macros CONFIG_RTE_PCI_CONFIG, CONFIG_RTE_PCI_EXTENDED_TAG and
  CONFIG_RTE_PCI_MAX_READ_REQUEST_SIZE will be removed.
  The /sys entries extended_tag and max_read_request_size created by igb_uio
  will be removed.

* The following fields have been deprecated in rte_eth_stats:
  ibadcrc, ibadlen, imcasts, fdirmatch, fdirmiss,
  tx_pause_xon, rx_pause_xon, tx_pause_xoff, rx_pause_xoff

* ABI changes are planned for adding four new flow types. This impacts
  RTE_ETH_FLOW_MAX. The release 2.2 does not contain these ABI changes,
  but release 2.3 will. [postponed]
