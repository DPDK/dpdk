..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

ABI and API Deprecation
=======================

See the guidelines document for details of the :doc:`ABI policy
<../contributing/abi_policy>`. API and ABI deprecation notices are to be posted
here.

Deprecation Notices
-------------------

* make: Support for building DPDK with "make" has been deprecated and
  support will be removed in the 20.11 release. From 20.11 onwards, DPDK
  should be built using meson and ninja. For basic instructions see the
  `Quick-start Guide <https://core.dpdk.org/doc/quick-start/>`_ on the
  website or the `Getting Started Guide
  <https://doc.dpdk.org/guides/linux_gsg/build_dpdk.html>`_ document.

* meson: The minimum supported version of meson for configuring and building
  DPDK will be increased to v0.47.1 (from 0.41) from DPDK 19.05 onwards. For
  those users with a version earlier than 0.47.1, an updated copy of meson
  can be got using the ``pip``, or ``pip3``, tool for downloading python
  packages.

* kvargs: The function ``rte_kvargs_process`` will get a new parameter
  for returning key match count. It will ease handling of no-match case.

* eal: The function ``rte_eal_remote_launch`` will return new error codes
  after read or write error on the pipe, instead of calling ``rte_panic``.

* eal: The ``rte_logs`` struct and global symbol will be made private to
  remove it from the externally visible ABI and allow it to be updated in the
  future.

* rte_atomicNN_xxx: These APIs do not take memory order parameter. This does
  not allow for writing optimized code for all the CPU architectures supported
  in DPDK. DPDK will adopt C11 atomic operations semantics and provide wrappers
  using C11 atomic built-ins. These wrappers must be used for patches that
  need to be merged in 20.08 onwards. This change will not introduce any
  performance degradation.

* rte_smp_*mb: These APIs provide full barrier functionality. However, many
  use cases do not require full barriers. To support such use cases, DPDK will
  adopt C11 barrier semantics and provide wrappers using C11 atomic built-ins.
  These wrappers must be used for patches that need to be merged in 20.08
  onwards. This change will not introduce any performance degradation.

* rte_cio_*mb: Since the IO barriers for ARMv8 platforms are relaxed from DSB
  to DMB, rte_cio_*mb APIs provide the same functionality as rte_io_*mb
  APIs (taking all platforms into consideration). rte_io_*mb APIs should be
  used in the place of rte_cio_*mb APIs. The rte_cio_*mb APIs will be
  deprecated in 20.11 release.

* igb_uio: In the view of reducing the kernel dependency from the main tree,
  as a first step, the Technical Board decided to move ``igb_uio``
  kernel module to the dpdk-kmods repository in the /linux/igb_uio/ directory
  in 20.11.
  Minutes of Technical Board Meeting of `2019-11-06
  <https://mails.dpdk.org/archives/dev/2019-November/151763.html>`_.

* lib: will fix extending some enum/define breaking the ABI. There are multiple
  samples in DPDK that enum/define terminated with a ``.*MAX.*`` value which is
  used by iterators, and arrays holding these values are sized with this
  ``.*MAX.*`` value. So extending this enum/define increases the ``.*MAX.*``
  value which increases the size of the array and depending on how/where the
  array is used this may break the ABI.
  ``RTE_ETH_FLOW_MAX`` is one sample of the mentioned case, adding a new flow
  type will break the ABI because of ``flex_mask[RTE_ETH_FLOW_MAX]`` array
  usage in following public struct hierarchy:
  ``rte_eth_fdir_flex_conf -> rte_fdir_conf -> rte_eth_conf (in the middle)``.
  Need to identify this kind of usages and fix in 20.11, otherwise this blocks
  us extending existing enum/define.
  One solution can be using a fixed size array instead of ``.*MAX.*`` value.

* pci: The PCI resources map API (``pci_map_resource`` and
  ``pci_unmap_resource``) was not abstracting the Unix mmap flags (see the
  workaround for Windows support implemented in the commit
  9d2b24593724 ("pci: keep API compatibility with mmap values")).
  This API will be removed from the public API in 20.11 and moved to the PCI
  bus driver along with the PCI resources lists and associated structures
  (``pci_map``, ``pci_msix_table``, ``mapped_pci_resource`` and
  ``mapped_pci_res_list``).
  With this removal, there won't be a need for the mentioned workaround which
  will be reverted.

* pci: The ``rte_kernel_driver`` enum defined in rte_dev.h will be made private
  to the PCI subsystem as it is used only by the PCI bus driver and PCI
  drivers.
  The associated field ``kdrv`` in the ethdev ``rte_eth_dev_data`` structure
  will be removed as it gave no useful abstracted information to the
  applications and had no user (neither internal nor external).

* mbuf: Some fields will be converted to dynamic API in DPDK 20.11
  in order to reserve more space for the dynamic fields, as explained in
  `this presentation <https://www.youtube.com/watch?v=Ttl6MlhmzWY>`_.
  The following static fields will be moved as dynamic:

  - ``timestamp``
  - ``userdata`` / ``udata64``
  - ``seqn``

  As a consequence, the layout of the ``struct rte_mbuf`` will be re-arranged,
  avoiding impact on vectorized implementation of the driver datapaths,
  while evaluating performance gains of a better use of the first cache line.

  The deprecated unioned fields ``buf_physaddr`` and ``refcnt_atomic``
  (as explained below) will be removed in DPDK 20.11.

* mbuf: ``refcnt_atomic`` member in structures ``rte_mbuf`` and
  ``rte_mbuf_ext_shared_info`` is of type ``rte_atomic16_t``.
  Due to adoption of C11 atomic builtins, the field ``refcnt_atomic``
  will be replaced with ``refcnt`` of type ``uint16_t`` in DPDK 20.11.

* ethdev: Split the ``struct eth_dev_ops`` struct to hide it as much as possible
  will be done in 20.11.
  Currently the ``struct eth_dev_ops`` struct is accessible by the application
  because some inline functions, like ``rte_eth_tx_descriptor_status()``,
  access the struct directly.
  The struct will be separate in two, the ops used by inline functions will be
  moved next to Rx/Tx burst functions, rest of the ``struct eth_dev_ops`` struct
  will be moved to header file for drivers to hide it from applications.

* ethdev: the legacy filter API, including
  ``rte_eth_dev_filter_supported()``, ``rte_eth_dev_filter_ctrl()`` as well
  as filter types MACVLAN, ETHERTYPE, FLEXIBLE, SYN, NTUPLE, TUNNEL, FDIR,
  HASH and L2_TUNNEL, is superseded by the generic flow API (rte_flow) in
  PMDs that implement the latter.
  The legacy API will be removed in DPDK 20.11.

* ethdev: The flow director API, including ``rte_eth_conf.fdir_conf`` field,
  and the related structures (``rte_fdir_*`` and ``rte_eth_fdir_*``),
  will be removed in DPDK 20.11.

* ethdev: The legacy L2 tunnel filtering API is deprecated as the rest of
  the legacy filtering API.
  The functions ``rte_eth_dev_l2_tunnel_eth_type_conf`` and
  ``rte_eth_dev_l2_tunnel_offload_set`` which were not marked as deprecated,
  will be removed in DPDK 20.11.

* ethdev: Update API functions returning ``void`` to return ``int`` with
  negative errno values to indicate various error conditions (e.g.
  invalid port ID, unsupported operation, failed operation):

  - ``rte_eth_dev_stop``
  - ``rte_eth_dev_close``

* ethdev: The temporary flag RTE_ETH_DEV_CLOSE_REMOVE will be removed in 20.11.
  As a consequence, the new behaviour introduced in 18.11 will be effective
  for all drivers: generic port resources are freed on close operation.
  Private resources are expected to be released in the ``dev_close`` callback.
  More details in http://inbox.dpdk.org/dev/5248162.j6AOsuQRmx@thomas/

* ethdev: New offload flags ``DEV_RX_OFFLOAD_FLOW_MARK`` will be added in 19.11.
  This will allow application to enable or disable PMDs from updating
  ``rte_mbuf::hash::fdir``.
  This scheme will allow PMDs to avoid writes to ``rte_mbuf`` fields on Rx and
  thereby improve Rx performance if application wishes do so.
  In 19.11 PMDs will still update the field even when the offload is not
  enabled.

* ethdev: ``rx_descriptor_done`` dev_ops and ``rte_eth_rx_descriptor_done``
  will be deprecated in 20.11 and will be removed in 21.11.
  Existing ``rte_eth_rx_descriptor_status`` and ``rte_eth_tx_descriptor_status``
  APIs can be used as replacement.

* ethdev: The port mirroring API can be replaced with a more fine grain flow API.
  The structs ``rte_eth_mirror_conf``, ``rte_eth_vlan_mirror`` and the functions
  ``rte_eth_mirror_rule_set``, ``rte_eth_mirror_rule_reset`` will be marked
  as deprecated in DPDK 20.11, along with the associated macros ``ETH_MIRROR_*``.
  This API will be fully removed in DPDK 21.11.

* ethdev: Some internal APIs for driver usage are exported in the .map file.
  Now DPDK has ``__rte_internal`` marker so we can mark internal APIs and move
  them to the INTERNAL block in .map. Although these APIs are internal it will
  break the ABI checks, that is why change is planned for 20.11.
  The list of internal APIs are mainly ones listed in ``rte_ethdev_driver.h``.

* traffic manager: All traffic manager API's in ``rte_tm.h`` were mistakenly made
  ABI stable in the v19.11 release. The TM maintainer and other contributors have
  agreed to keep the TM APIs as experimental in expectation of additional spec
  improvements. Therefore, all APIs in ``rte_tm.h`` will be marked back as
  experimental in v20.11 DPDK release. For more details, please see `the thread
  <https://mails.dpdk.org/archives/dev/2020-April/164970.html>`_.

* pmd_dpaa: The API ``rte_pmd_dpaa_set_tx_loopback`` will have extended
  ``port_id`` definition from ``uint8_t`` to ``uint16_t``.

* cryptodev: support for using IV with all sizes is added, J0 still can
  be used but only when IV length in following structs ``rte_crypto_auth_xform``,
  ``rte_crypto_aead_xform`` is set to zero. When IV length is greater or equal
  to one it means it represents IV, when is set to zero it means J0 is used
  directly, in this case 16 bytes of J0 need to be passed.

* eventdev: Following structures will be modified to support DLB PMD
  and future extensions:

  - ``rte_event_dev_info``
  - ``rte_event_dev_config``
  - ``rte_event_port_conf``

  Patches containing justification, documentation, and proposed modifications
  can be found at:

  - https://patches.dpdk.org/patch/71457/
  - https://patches.dpdk.org/patch/71456/

* rawdev: The rawdev APIs which take a device-specific structure as
  parameter directly, or indirectly via a "private" pointer inside another
  structure, will be modified to take an additional parameter of the
  structure size. The affected APIs will include ``rte_rawdev_info_get``,
  ``rte_rawdev_configure``, ``rte_rawdev_queue_conf_get`` and
  ``rte_rawdev_queue_setup``.

* sched: To allow more traffic classes, flexible mapping of pipe queues to
  traffic classes, and subport level configuration of pipes and queues
  changes will be made to macros, data structures and API functions defined
  in "rte_sched.h". These changes are aligned to improvements suggested in the
  RFC https://mails.dpdk.org/archives/dev/2018-November/120035.html.

* metrics: The function ``rte_metrics_init`` will have a non-void return
  in order to notify errors instead of calling ``rte_exit``.

* power: ``rte_power_set_env`` function will no longer return 0 on attempt
  to set new power environment if power environment was already initialized.
  In this case the function will return -1 unless the environment is unset first
  (using ``rte_power_unset_env``). Other function usage scenarios will not change.

* python: Since the beginning of 2020, Python 2 has officially reached
  end-of-support: https://www.python.org/doc/sunset-python-2/.
  Python 2 support will be completely removed in 20.11.
  In 20.08, explicit deprecation warnings will be displayed when running
  scripts with Python 2.

* dpdk-setup.sh: This old script relies on deprecated stuff, and especially
  ``make``. Given environments are too much variables for such a simple script,
  it will be removed in DPDK 20.11.
  Some useful parts may be converted into specific scripts.
