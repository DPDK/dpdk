ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* The following fields have been deprecated in rte_eth_stats:
  ibadcrc, ibadlen, imcasts, fdirmatch, fdirmiss,
  tx_pause_xon, rx_pause_xon, tx_pause_xoff, rx_pause_xoff

* The ethdev structures rte_eth_link, rte_eth_dev_info and rte_eth_conf
  must be updated to support 100G link and to have a cleaner link speed API.

* ABI changes is planned for the reta field in struct rte_eth_rss_reta_entry64
  which handles at most 256 queues (8 bits) while newer NICs support larger
  tables (512 queues).
  It should be integrated in release 2.3.

* ABI changes are planned for struct rte_eth_fdir_flow in order to support
  extend flow director's input set. The release 2.2 does not contain these ABI
  changes, but release 2.3 will, and no backwards compatibility is planned.

* ABI changes are planned for rte_eth_ipv4_flow and rte_eth_ipv6_flow to
  include more fields to be matched against. The release 2.2 does not
  contain these ABI changes, but release 2.3 will.

* ABI changes are planned for adding four new flow types. This impacts
  RTE_ETH_FLOW_MAX. The release 2.2 does not contain these ABI changes,
  but release 2.3 will.

* ABI changes are planned for rte_eth_tunnel_filter_conf. Change the fields
  of outer_mac and inner_mac from pointer to struct in order to keep the
  code's readability. The release 2.2 does not contain these ABI changes, but
  release 2.3 will, and no backwards compatibility is planned.

* The scheduler statistics structure will change to allow keeping track of
  RED actions.

* librte_pipeline: The prototype for the pipeline input port, output port
  and table action handlers will be updated:
  the pipeline parameter will be added, the packets mask parameter will be
  either removed (for input port action handler) or made input-only.

* ABI changes are planned in cmdline buffer size to allow the use of long
  commands (such as RETA update in testpmd).  This should impact
  CMDLINE_PARSE_RESULT_BUFSIZE, STR_TOKEN_SIZE and RDLINE_BUF_SIZE.
  It should be integrated in release 2.3.
