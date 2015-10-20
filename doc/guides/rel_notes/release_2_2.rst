DPDK Release 2.2
================

New Features
------------

* **Introduce ARMv7 and ARMv8 architectures**

  * It is now possible to build DPDK for the ARMv7 and ARMv8 platforms.
  * ARMv7 can be tested with virtual PMD drivers.
  * ARMv8 can be tested with virtual and physical PMD drivers.

* **Enabled freeing of ring.**

  New function rte_ring_free() allows the user to free a ring
  if it was created with rte_ring_create().

* **Added keepalive support to EAL and example application.**

* **Added experimental cryptodev API**

  The cryptographic processing of packet is provided as a preview
  with two drivers for:

  * Intel QuickAssist devices
  * Intel AES-NI multi-buffer library

  Due to its experimental state, the API may change without prior notice.

* **Added ethdev API to support IEEE1588.**

  Added functions to read, write and adjust system time in the NIC.

* **Extended Statistics**

  Define extended statistics naming scheme to store metadata in the name
  string name of each statistic, refer to the Extended Statistics section
  of the programmers guide. Implemented the extended stats API for these
  PMDs:

  * igb
  * igbvf
  * i40e
  * i40evf
  * fm10k
  * virtio

* **Added API in ethdev to retrieve RX/TX queue information.**

  *  Add the ability for the upper layer to query RX/TX queue information.
  *  Add into rte_eth_dev_info new fields to represent information about
     RX/TX descriptors min/max/align numbers per queue for the device.

* **Added RSS dynamic configuration to bonding.**

* **Added e1000 Rx interrupt support.**

* **Added igb TSO support for both PF and VF.**

* **RSS enhancement on Intel x550 NIC**

  * Support 512 entries RSS redirection table.
  * Support per VF RSS redirection table.

* **Flow director enhancement on Intel x550 NIC**

  * Add 2 new flow director modes on x550.
  * One is MAC VLAN mode, the other is tunnel mode.

* **Added i40e vector RX/TX.**

* **Added i40e Rx interrupt support.**

* **Added i40e flow control support.**

* **Added DCB support to i40e PF driver.**

* **Added RSS/FD input set granularity on Intel X710/XL710.**

* **Added different GRE key length for input set on Intel X710/XL710.**

* **Added flow director support in i40e VF.**

* **Added fm10k vector RX/TX.**

* **Added fm10k TSO support for both PF and VF.**

* **Added fm10k VMDQ support.**

* **New NIC Boulder Rapid support.**

  Boulder Rapid is a new NIC of Intel's fm10k family.

* **Enhanced support for the Chelsio CXGBE driver.**

  *  Added support for Jumbo Frames.
  *  Optimize forwarding performance for Chelsio T5 40GbE cards.

* **Improved enic Tx packet rate.**

  Reduced frequency of Tx tail pointer updates to the NIC.

* **Added support for Mellanox ConnectX-4 adapters (mlx5).**

  The mlx5 poll-mode driver implements support for Mellanox ConnectX-4 EN
  and Mellanox ConnectX-4 Lx EN families of 10/25/40/50/100 Gb/s adapters.

  Like mlx4, this PMD is only available for Linux and is disabled by default
  due to external dependencies (libibverbs and libmlx5).

* **Added virtual szedata2 driver for COMBO cards.**

  Added virtual PMD for COMBO-100G and COMBO-80G cards.
  PMD is disabled in default configuration.

* **Enhanced support for virtio driver.**

  * Virtio ring layout optimization (fixed avail ring)
  * Vector RX
  * Simple TX

* **Added vhost-user multiple queue support.**

* **Added port hotplug support to vmxnet3.**

* **Added port hotplug support to xenvirt.**


Resolved Issues
---------------

EAL
~~~

* **eal/linux: Fixed epoll timeout.**

  Fixed issue where the ``rte_epoll_wait()`` function didn't return when the
  underlying call to ``epoll_wait()`` timed out.


Drivers
~~~~~~~

* **igb: Fixed IEEE1588 frame identification in I210.**

  Fixed issue where the flag PKT_RX_IEEE1588_PTP was not being set
  in Intel I210 NIC, as EtherType in RX descriptor is in bits 8:10 of
  Packet Type and not in the default bits 0:2.

* **igb: Fixed VF start with PF stopped.**

  VF needs the PF interrupt support initialized even if not started.

* **ixgbe: Fixed issue with X550 DCB.**

  Fixed a DCB issue with x550 where for 8 TCs (Traffic Classes), if a packet
  with user priority 6 or 7 was injected to the NIC, then the NIC would only
  put 3 packets into the queue. There was also a similar issue for 4 TCs.

* **ixgbe: Removed burst size restriction of vector RX.**

  Fixed issue where a burst size less than 32 didn't receive anything.

* **ixgbe: Fixed VF start with PF stopped.**

  VF needs the PF interrupt support initialized even if not started.

* **ixgbe: Fixed TX hang when RS distance exceeds HW limit.**

  Fixed an issue when TX queue can hang when a lot of highly fragmented
  packets have to be sent.
  As part of that fix, tx_rs_thresh for ixgbe PMD is not allowed to be greater
  then to 32 to comply with HW restrictions.

* **i40e: Fixed base driver allocation when not using first numa node.**

  Fixed i40e issue that occurred when a DPDK application didn't initialize
  ports if memory wasn't available on socket 0.

* **i40e: Fixed maximum of 64 queues per port.**

  Fixed the issue in i40e of cannot supporting more than 64 queues per port,
  though hardware actually supports that. The real number of queues may vary,
  as long as the total number of queues used in PF, VFs, VMDq and FD does not
  exceeds the hardware maximum.

* **i40e: Fixed statistics of packets.**

  Added discarding packets on VSI to the stats and rectify the old statistics.

* **i40e: Fixed issue of not freeing memzone.**

  Fixed the issue of not freeing memzone in the call to free the memory for
  adminq DMA.

* **vhost: Fixed Qemu shutdown.**

  Fixed issue with libvirt ``virsh destroy`` not killing the VM.

* **virtio: Fixed crash after changing link state.**

  Fixed io permission in the interrupt handler.

* **virtio: Fixed crash when releasing queue.**

  Fixed issue when releasing null control queue.


Libraries
~~~~~~~~~

* **hash: Fixed memory allocation of Cuckoo Hash key table.**

  Fixed issue where an incorrect Cuckoo Hash key table size could be
  calculated limiting the size to 4GB.

* **hash: Fixed incorrect lookup if key is all zero.**

  Fixed issue in hash library that occurred if an all zero
  key was not added in the table and the key was looked up,
  resulting in an incorrect hit.

* **hash: Fixed thread scaling by reducing contention.**

  Fixed issue in hash library where, using multiple cores with
  hardware transactional memory support, thread scaling did not work,
  due to the global ring that is shared by all cores.


Examples
~~~~~~~~


Other
~~~~~


Known Issues
------------

* Some drivers do not fill the packet type when receiving.
  As the l3fwd example application requires this info, the i40e vector
  driver must be disabled to benefit of the packet type with i40e.


API Changes
-----------

* The deprecated flow director API is removed.
  It was replaced by rte_eth_dev_filter_ctrl().

* The dcb_queue is renamed to dcb_tc in following dcb configuration
  structures: rte_eth_dcb_rx_conf, rte_eth_dcb_tx_conf,
  rte_eth_vmdq_dcb_conf, rte_eth_vmdq_dcb_tx_conf.

* The rte_eth_rx_queue_count() function now returns "int" instead of "uint32_t"
  to allow the use of negative values as error codes on return.

* The function rte_eal_pci_close_one() is removed.
  It was replaced by rte_eal_pci_detach().

* The deprecated ACL API ipv4vlan is removed.

* The deprecated hash function rte_jhash2() is removed.
  It was replaced by rte_jhash_32b().

* The deprecated KNI functions are removed:
  rte_kni_create(), rte_kni_get_port_id() and rte_kni_info_get().

* The deprecated ring PMD functions are removed:
  rte_eth_ring_pair_create() and rte_eth_ring_pair_attach().

* The devargs union field virtual is renamed to virt for C++ compatibility.

ABI Changes
-----------

* The EAL and ethdev structures rte_intr_handle and rte_eth_conf were changed
  to support Rx interrupt. It was already done in 2.1 for CONFIG_RTE_NEXT_ABI.

* The ethdev flow director entries for SCTP were changed.
  It was already done in 2.1 for CONFIG_RTE_NEXT_ABI.

* The ethdev flow director structure rte_eth_fdir_flow_ext structure is changed.
  New fields are added to support flow director filtering in VF.

* The size of the ethdev structure rte_eth_hash_filter_info is changed
  by adding a new element rte_eth_input_set_conf in an union.

* The new fields rx_desc_lim and tx_desc_lim are added into rte_eth_dev_info
  structure.

* For debug builds, the functions rte_eth_rx_burst(), rte_eth_tx_burst()
  rte_eth_rx_descriptor_done() and rte_eth_rx_queue_count() will
  no longer be separate functions in the DPDK libraries. Instead, they will
  only be present in the rte_ethdev.h header file.

* The maximum number of queues per port CONFIG_RTE_MAX_QUEUES_PER_PORT is
  increased to 1024.

* The mbuf structure was changed to support unified packet type.
  It was already done in 2.1 for CONFIG_RTE_NEXT_ABI.

* The dummy malloc library is removed. The content was moved into EAL in 2.1.

* The LPM structure is changed. The deprecated field mem_location is removed.

* librte_table LPM: A new parameter to hold the table name will be added to
  the LPM table parameter structure.

* librte_port: Macros to access the packet meta-data stored within the packet
  buffer has been adjusted to cover the packet mbuf structure.

* librte_cfgfile: Allow longer names and values by increasing the constants
  CFG_NAME_LEN and CFG_VALUE_LEN to 64 and 256 respectively.


Shared Library Versions
-----------------------

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

   + libethdev.so.2
   + librte_acl.so.2
   + librte_cfgfile.so.2
     librte_cmdline.so.1
     librte_distributor.so.1
   + librte_eal.so.2
   + librte_hash.so.2
     librte_ip_frag.so.1
     librte_ivshmem.so.1
     librte_jobstats.so.1
   + librte_kni.so.2
     librte_kvargs.so.1
   + librte_lpm.so.2
   + librte_mbuf.so.2
     librte_mempool.so.1
     librte_meter.so.1
   + librte_pipeline.so.2
     librte_pmd_bond.so.1
   + librte_pmd_ring.so.2
   + librte_port.so.2
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
   + librte_table.so.2
     librte_timer.so.1
     librte_vhost.so.1
