..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2016 Intel Corporation.

Vhost Library
=============

The vhost library implements a user space virtio net server allowing the user
to manipulate the virtio ring directly. In another words, it allows the user
to fetch/put packets from/to the VM virtio net device. To achieve this, a
vhost library should be able to:

* Access the guest memory:

  For QEMU, this is done by using the ``-object memory-backend-file,share=on,...``
  option. Which means QEMU will create a file to serve as the guest RAM.
  The ``share=on`` option allows another process to map that file, which
  means it can access the guest RAM.

* Know all the necessary information about the vring:

  Information such as where the available ring is stored. Vhost defines some
  messages (passed through a Unix domain socket file) to tell the backend all
  the information it needs to know how to manipulate the vring.


Vhost API Overview
------------------

The following is an overview of some key Vhost API functions:

* ``rte_vhost_driver_register(path, flags)``

  This function registers a vhost driver into the system. ``path`` specifies
  the Unix domain socket file path.

  Currently supported flags are:

  - ``RTE_VHOST_USER_CLIENT``

    DPDK vhost-user will act as the client when this flag is given. See below
    for an explanation.

  - ``RTE_VHOST_USER_NO_RECONNECT``

    When DPDK vhost-user acts as the client it will keep trying to reconnect
    to the server (QEMU) until it succeeds. This is useful in two cases:

    * When QEMU is not started yet.
    * When QEMU restarts (for example due to a guest OS reboot).

    This reconnect option is enabled by default. However, it can be turned off
    by setting this flag.

  - ``RTE_VHOST_USER_IOMMU_SUPPORT``

    IOMMU support will be enabled when this flag is set. It is disabled by
    default.

    Enabling this flag makes possible to use guest vIOMMU to protect vhost
    from accessing memory the virtio device isn't allowed to, when the feature
    is negotiated and an IOMMU device is declared.

  - ``RTE_VHOST_USER_POSTCOPY_SUPPORT``

    Postcopy live-migration support will be enabled when this flag is set.
    It is disabled by default.

    Enabling this flag should only be done when the calling application does
    not pre-fault the guest shared memory, otherwise migration would fail.

  - ``RTE_VHOST_USER_LINEARBUF_SUPPORT``

    Enabling this flag forces vhost dequeue function to only provide linear
    pktmbuf (no multi-segmented pktmbuf).

    The vhost library by default provides a single pktmbuf for given a
    packet, but if for some reason the data doesn't fit into a single
    pktmbuf (e.g., TSO is enabled), the library will allocate additional
    pktmbufs from the same mempool and chain them together to create a
    multi-segmented pktmbuf.

    However, the vhost application needs to support multi-segmented format.
    If the vhost application does not support that format and requires large
    buffers to be dequeue, this flag should be enabled to force only linear
    buffers (see RTE_VHOST_USER_EXTBUF_SUPPORT) or drop the packet.

    It is disabled by default.

  - ``RTE_VHOST_USER_EXTBUF_SUPPORT``

    Enabling this flag allows vhost dequeue function to allocate and attach
    an external buffer to a pktmbuf if the pkmbuf doesn't provide enough
    space to store all data.

    This is useful when the vhost application wants to support large packets
    but doesn't want to increase the default mempool object size nor to
    support multi-segmented mbufs (non-linear). In this case, a fresh buffer
    is allocated using rte_malloc() which gets attached to a pktmbuf using
    rte_pktmbuf_attach_extbuf().

    See RTE_VHOST_USER_LINEARBUF_SUPPORT as well to disable multi-segmented
    mbufs for application that doesn't support chained mbufs.

    It is disabled by default.

  - ``RTE_VHOST_USER_ASYNC_COPY``

    Asynchronous data path will be enabled when this flag is set. Async data
    path allows applications to register async copy devices (typically
    hardware DMA channels) to the vhost queues. Vhost leverages the copy
    device registered to free CPU from memory copy operations. A set of
    async data path APIs are defined for DPDK applications to make use of
    the async capability. Only packets enqueued/dequeued by async APIs are
    processed through the async data path.

    Currently this feature is only implemented on split ring enqueue data
    path.

    It is disabled by default.

  - ``RTE_VHOST_USER_NET_COMPLIANT_OL_FLAGS``

    Since v16.04, the vhost library forwards checksum and gso requests for
    packets received from a virtio driver by filling Tx offload metadata in
    the mbuf. This behavior is inconsistent with other drivers but it is left
    untouched for existing applications that might rely on it.

    This flag disables the legacy behavior and instead ask vhost to simply
    populate Rx offload metadata in the mbuf.

    It is disabled by default.

* ``rte_vhost_driver_set_features(path, features)``

  This function sets the feature bits the vhost-user driver supports. The
  vhost-user driver could be vhost-user net, yet it could be something else,
  say, vhost-user SCSI.

* ``rte_vhost_driver_callback_register(path, vhost_device_ops)``

  This function registers a set of callbacks, to let DPDK applications take
  the appropriate action when some events happen. The following events are
  currently supported:

  * ``new_device(int vid)``

    This callback is invoked when a virtio device becomes ready. ``vid``
    is the vhost device ID.

  * ``destroy_device(int vid)``

    This callback is invoked when a virtio device is paused or shut down.

  * ``vring_state_changed(int vid, uint16_t queue_id, int enable)``

    This callback is invoked when a specific queue's state is changed, for
    example to enabled or disabled.

  * ``features_changed(int vid, uint64_t features)``

    This callback is invoked when the features is changed. For example,
    ``VHOST_F_LOG_ALL`` will be set/cleared at the start/end of live
    migration, respectively.

  * ``new_connection(int vid)``

    This callback is invoked on new vhost-user socket connection. If DPDK
    acts as the server the device should not be deleted before
    ``destroy_connection`` callback is received.

  * ``destroy_connection(int vid)``

    This callback is invoked when vhost-user socket connection is closed.
    It indicates that device with id ``vid`` is no longer in use and can be
    safely deleted.

* ``rte_vhost_driver_disable/enable_features(path, features))``

  This function disables/enables some features. For example, it can be used to
  disable mergeable buffers and TSO features, which both are enabled by
  default.

* ``rte_vhost_driver_start(path)``

  This function triggers the vhost-user negotiation. It should be invoked at
  the end of initializing a vhost-user driver.

* ``rte_vhost_enqueue_burst(vid, queue_id, pkts, count)``

  Transmits (enqueues) ``count`` packets from host to guest.

* ``rte_vhost_dequeue_burst(vid, queue_id, mbuf_pool, pkts, count)``

  Receives (dequeues) ``count`` packets from guest, and stored them at ``pkts``.

* ``rte_vhost_crypto_create(vid, cryptodev_id, sess_mempool, socket_id)``

  As an extension of new_device(), this function adds virtio-crypto workload
  acceleration capability to the device. All crypto workload is processed by
  DPDK cryptodev with the device ID of ``cryptodev_id``.

* ``rte_vhost_crypto_free(vid)``

  Frees the memory and vhost-user message handlers created in
  rte_vhost_crypto_create().

* ``rte_vhost_crypto_fetch_requests(vid, queue_id, ops, nb_ops)``

  Receives (dequeues) ``nb_ops`` virtio-crypto requests from guest, parses
  them to DPDK Crypto Operations, and fills the ``ops`` with parsing results.

* ``rte_vhost_crypto_finalize_requests(queue_id, ops, nb_ops)``

  After the ``ops`` are dequeued from Cryptodev, finalizes the jobs and
  notifies the guest(s).

* ``rte_vhost_crypto_set_zero_copy(vid, option)``

  Enable or disable zero copy feature of the vhost crypto backend.

* ``rte_vhost_async_channel_register(vid, queue_id, config, ops)``

  Register an async copy device channel for a vhost queue after vring
  is enabled. Following device ``config`` must be specified together
  with the registration:

  * ``features``

    This field is used to specify async copy device features.

    ``RTE_VHOST_ASYNC_INORDER`` represents the async copy device can
    guarantee the order of copy completion is the same as the order
    of copy submission.

    Currently, only ``RTE_VHOST_ASYNC_INORDER`` capable device is
    supported by vhost.

  Applications must provide following ``ops`` callbacks for vhost lib to
  work with the async copy devices:

  * ``transfer_data(vid, queue_id, descs, opaque_data, count)``

    vhost invokes this function to submit copy data to the async devices.
    For non-async_inorder capable devices, ``opaque_data`` could be used
    for identifying the completed packets.

  * ``check_completed_copies(vid, queue_id, opaque_data, max_packets)``

    vhost invokes this function to get the copy data completed by async
    devices.

* ``rte_vhost_async_channel_register_thread_unsafe(vid, queue_id, config, ops)``

  Register an async copy device channel for a vhost queue without
  performing any locking.

  This function is only safe to call in vhost callback functions
  (i.e., struct rte_vhost_device_ops).

* ``rte_vhost_async_channel_unregister(vid, queue_id)``

  Unregister the async copy device channel from a vhost queue.
  Unregistration will fail, if the vhost queue has in-flight
  packets that are not completed.

  Unregister async copy devices in vring_state_changed() may
  fail, as this API tries to acquire the spinlock of vhost
  queue. The recommended way is to unregister async copy
  devices for all vhost queues in destroy_device(), when a
  virtio device is paused or shut down.

* ``rte_vhost_async_channel_unregister_thread_unsafe(vid, queue_id)``

  Unregister the async copy device channel for a vhost queue without
  performing any locking.

  This function is only safe to call in vhost callback functions
  (i.e., struct rte_vhost_device_ops).

* ``rte_vhost_submit_enqueue_burst(vid, queue_id, pkts, count, comp_pkts, comp_count)``

  Submit an enqueue request to transmit ``count`` packets from host to guest
  by async data path. Successfully enqueued packets can be transfer completed
  or being occupied by DMA engines; transfer completed packets are returned in
  ``comp_pkts``, but others are not guaranteed to finish, when this API
  call returns.

  Applications must not free the packets submitted for enqueue until the
  packets are completed.

* ``rte_vhost_poll_enqueue_completed(vid, queue_id, pkts, count)``

  Poll enqueue completion status from async data path. Completed packets
  are returned to applications through ``pkts``.

* ``rte_vhost_async_get_inflight(vid, queue_id)``

  This function returns the amount of in-flight packets for the vhost
  queue using async acceleration.

* ``rte_vhost_clear_queue_thread_unsafe(vid, queue_id, **pkts, count)``

  Clear inflight packets which are submitted to DMA engine in vhost async data
  path. Completed packets are returned to applications through ``pkts``.

Vhost-user Implementations
--------------------------

Vhost-user uses Unix domain sockets for passing messages. This means the DPDK
vhost-user implementation has two options:

* DPDK vhost-user acts as the server.

  DPDK will create a Unix domain socket server file and listen for
  connections from the frontend.

  Note, this is the default mode, and the only mode before DPDK v16.07.


* DPDK vhost-user acts as the client.

  Unlike the server mode, this mode doesn't create the socket file;
  it just tries to connect to the server (which responses to create the
  file instead).

  When the DPDK vhost-user application restarts, DPDK vhost-user will try to
  connect to the server again. This is how the "reconnect" feature works.

  .. Note::
     * The "reconnect" feature requires **QEMU v2.7** (or above).

     * The vhost supported features must be exactly the same before and
       after the restart. For example, if TSO is disabled and then enabled,
       nothing will work and undefined issues might happen.

No matter which mode is used, once a connection is established, DPDK
vhost-user will start receiving and processing vhost messages from QEMU.

For messages with a file descriptor, the file descriptor can be used directly
in the vhost process as it is already installed by the Unix domain socket.

The supported vhost messages are:

* ``VHOST_SET_MEM_TABLE``
* ``VHOST_SET_VRING_KICK``
* ``VHOST_SET_VRING_CALL``
* ``VHOST_SET_LOG_FD``
* ``VHOST_SET_VRING_ERR``

For ``VHOST_SET_MEM_TABLE`` message, QEMU will send information for each
memory region and its file descriptor in the ancillary data of the message.
The file descriptor is used to map that region.

``VHOST_SET_VRING_KICK`` is used as the signal to put the vhost device into
the data plane, and ``VHOST_GET_VRING_BASE`` is used as the signal to remove
the vhost device from the data plane.

When the socket connection is closed, vhost will destroy the device.

Guest memory requirement
------------------------

* Memory pre-allocation

  For non-async data path guest memory pre-allocation is not a
  must but can help save memory. To do this we can add option
  ``-mem-prealloc`` when starting QEMU, or we can lock all memory at vhost
  side which will force memory to be allocated when it calls mmap
  (option --mlockall in ovs-dpdk is an example in hand).


  For async data path, we force the VM memory to be pre-allocated at vhost
  lib when mapping the guest memory; and also we need to lock the memory to
  prevent pages being swapped out to disk.

* Memory sharing

  Make sure ``share=on`` QEMU option is given. The vhost-user will not work with
  a QEMU instance without shared memory mapping.

Vhost supported vSwitch reference
---------------------------------

For more vhost details and how to support vhost in vSwitch, please refer to
the vhost example in the DPDK Sample Applications Guide.

Vhost data path acceleration (vDPA)
-----------------------------------

vDPA supports selective datapath in vhost-user lib by enabling virtio ring
compatible devices to serve virtio driver directly for datapath acceleration.

``rte_vhost_driver_attach_vdpa_device`` is used to configure the vhost device
with accelerated backend.

Also vhost device capabilities are made configurable to adopt various devices.
Such capabilities include supported features, protocol features, queue number.

Finally, a set of device ops is defined for device specific operations:

* ``get_queue_num``

  Called to get supported queue number of the device.

* ``get_features``

  Called to get supported features of the device.

* ``get_protocol_features``

  Called to get supported protocol features of the device.

* ``dev_conf``

  Called to configure the actual device when the virtio device becomes ready.

* ``dev_close``

  Called to close the actual device when the virtio device is stopped.

* ``set_vring_state``

  Called to change the state of the vring in the actual device when vring state
  changes.

* ``set_features``

  Called to set the negotiated features to device.

* ``migration_done``

  Called to allow the device to response to RARP sending.

* ``get_vfio_group_fd``

   Called to get the VFIO group fd of the device.

* ``get_vfio_device_fd``

  Called to get the VFIO device fd of the device.

* ``get_notify_area``

  Called to get the notify area info of the queue.
