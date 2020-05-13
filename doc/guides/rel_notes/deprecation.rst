..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

ABI and API Deprecation
=======================

See the guidelines document for details of the :doc:`ABI policy
<../contributing/abi_policy>`. API and ABI deprecation notices are to be posted
here.

Deprecation Notices
-------------------

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
  Target release for removal of the legacy API will be defined once most
  PMDs have switched to rte_flow.

* ethdev: Update API functions returning ``void`` to return ``int`` with
  negative errno values to indicate various error conditions (e.g.
  invalid port ID, unsupported operation, failed operation):

  - ``rte_eth_dev_stop``
  - ``rte_eth_dev_close``

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

* traffic manager: All traffic manager API's in ``rte_tm.h`` were mistakenly made
  ABI stable in the v19.11 release. The TM maintainer and other contributors have
  agreed to keep the TM APIs as experimental in expectation of additional spec
  improvements. Therefore, all APIs in ``rte_tm.h`` will be marked back as
  experimental in v20.11 DPDK release. For more details, please see `the thread
  <https://mails.dpdk.org/archives/dev/2020-April/164970.html>`_.

* cryptodev: support for using IV with all sizes is added, J0 still can
  be used but only when IV length in following structs ``rte_crypto_auth_xform``,
  ``rte_crypto_aead_xform`` is set to zero. When IV length is greater or equal
  to one it means it represents IV, when is set to zero it means J0 is used
  directly, in this case 16 bytes of J0 need to be passed.

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

* pci: Remove ``RTE_KDRV_NONE`` based device driver probing.
  In order to optimize the DPDK PCI enumeration management, ``RTE_KDRV_NONE``
  based device driver probing will be removed in v20.08.
  The legacy virtio is the only consumer of ``RTE_KDRV_NONE`` based device
  driver probe scheme. The legacy virtio support will be available through
  the existing VFIO/UIO based kernel driver scheme.
  More details at https://patches.dpdk.org/patch/69351/
