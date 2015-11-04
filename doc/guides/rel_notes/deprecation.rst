ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* The following fields have been deprecated in rte_eth_stats:
  ibadcrc, ibadlen, imcasts, fdirmatch, fdirmiss,
  tx_pause_xon, rx_pause_xon, tx_pause_xoff, rx_pause_xoff

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

* librte_table: New functions for table entry bulk add/delete will be added
  to the table operations structure.

* librte_table hash: Key mask parameter will be added to the hash table
  parameter structure for 8-byte key and 16-byte key extendible bucket and
  LRU tables.

* librte_pipeline: The prototype for the pipeline input port, output port
  and table action handlers will be updated:
  the pipeline parameter will be added, the packets mask parameter will be
  either removed (for input port action handler) or made input-only.
