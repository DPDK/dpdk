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

* The Macros RTE_HASH_BUCKET_ENTRIES_MAX and RTE_HASH_KEY_LENGTH_MAX are
  deprecated and will be removed with version 2.2.

* The function rte_jhash2 is deprecated and should be removed.

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

* ABI changes are planned for struct rte_eth_fdir_filter and
  rte_eth_fdir_masks in order to support new flow director modes,
  MAC VLAN and Cloud, on x550. The MAC VLAN mode means the MAC and
  VLAN are monitored. The Cloud mode is for VxLAN and NVGRE, and
  the tunnel type, TNI/VNI, inner MAC and inner VLAN are monitored.
  The release 2.2 will contain these changes without backwards compatibility.

* librte_kni: Functions based on port id are deprecated for a long time and
  should be removed (rte_kni_create, rte_kni_get_port_id and rte_kni_info_get).

* librte_pmd_ring: The deprecated functions rte_eth_ring_pair_create and
  rte_eth_ring_pair_attach should be removed.

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

* librte_port: Macros to access the packet meta-data stored within the
  packet buffer will be adjusted to cover the packet mbuf structure as well,
  as currently they are able to access any packet buffer location except the
  packet mbuf structure.

* librte_table LPM: A new parameter to hold the table name will be added to
  the LPM table parameter structure.

* librte_table: New functions for table entry bulk add/delete will be added
  to the table operations structure.

* librte_table hash: Key mask parameter will be added to the hash table
  parameter structure for 8-byte key and 16-byte key extendible bucket and
  LRU tables.

* librte_pipeline: The prototype for the pipeline input port, output port
  and table action handlers will be updated:
  the pipeline parameter will be added, the packets mask parameter will be
  either removed (for input port action handler) or made input-only.
