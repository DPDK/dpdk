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

* ABI changes are planned for struct rte_intr_handle, struct rte_eth_conf
  and struct eth_dev_ops to support interrupt mode feature from release 2.1.
  Those changes may be enabled in the release 2.1 with CONFIG_RTE_NEXT_ABI.

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

* API for flow director filters has been replaced by rte_eth_dev_filter_ctrl.
  Following old API is deprecated and will be removed with version 2.2 without
  backward compatibility.
  Functions: rte_eth_dev_fdir_*.
  Structures: rte_fdir_*, rte_eth_fdir.
  Enums: rte_l4type, rte_iptype.

* ABI changes are planned for struct rte_eth_fdir_flow_ext in order to support
  flow director filtering in VF. The release 2.1 does not contain these ABI
  changes, but release 2.2 will, and no backwards compatibility is planned.

* ABI change is planned to extend the SCTP flow's key input from release 2.1.
  The change may be enabled in the release 2.1 with CONFIG_RTE_NEXT_ABI.

* ABI changes are planned for struct rte_eth_fdir_filter and
  rte_eth_fdir_masks in order to support new flow director modes,
  MAC VLAN and Cloud, on x550. The MAC VLAN mode means the MAC and
  VLAN are monitored. The Cloud mode is for VxLAN and NVGRE, and
  the tunnel type, TNI/VNI, inner MAC and inner VLAN are monitored.
  The release 2.2 will contain these changes without backwards compatibility.

* ABI changes are planned for struct virtio_net in order to support vhost-user
  multiple queues feature.
  It should be integrated in release 2.2 without backward compatibility.

* The scheduler hierarchy structure (rte_sched_port_hierarchy) will change to
  allow for a larger number of subport entries.
  The number of available traffic_classes and queues may also change.
  The mbuf structure element for sched hierarchy will also change from a single
  32 bit to a 64 bit structure.

* The scheduler statistics structure will change to allow keeping track of
  RED actions.

* librte_cfgfile: In order to allow for longer names and values,
  the value of macros CFG_NAME_LEN and CFG_NAME_VAL will be increased.
  Most likely, the new values will be 64 and 256, respectively.
