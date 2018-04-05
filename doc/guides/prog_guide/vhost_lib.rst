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

  - ``RTE_VHOST_USER_DEQUEUE_ZERO_COPY``

    Dequeue zero copy will be enabled when this flag is set. It is disabled by
    default.

    There are some truths (including limitations) you might want to know while
    setting this flag:

    * zero copy is not good for small packets (typically for packet size below
      512).

    * zero copy is really good for VM2VM case. For iperf between two VMs, the
      boost could be above 70% (when TSO is enableld).

    * for VM2NIC case, the ``nb_tx_desc`` has to be small enough: <= 64 if virtio
      indirect feature is not enabled and <= 128 if it is enabled.

      This is because when dequeue zero copy is enabled, guest Tx used vring will
      be updated only when corresponding mbuf is freed. Thus, the nb_tx_desc
      has to be small enough so that the PMD driver will run out of available
      Tx descriptors and free mbufs timely. Otherwise, guest Tx vring would be
      starved.

    * Guest memory should be backended with huge pages to achieve better
      performance. Using 1G page size is the best.

      When dequeue zero copy is enabled, the guest phys address and host phys
      address mapping has to be established. Using non-huge pages means far
      more page segments. To make it simple, DPDK vhost does a linear search
      of those segments, thus the fewer the segments, the quicker we will get
      the mapping. NOTE: we may speed it by using tree searching in future.

  - ``RTE_VHOST_USER_IOMMU_SUPPORT``

    IOMMU support will be enabled when this flag is set. It is disabled by
    default.

    Enabling this flag makes possible to use guest vIOMMU to protect vhost
    from accessing memory the virtio device isn't allowed to, when the feature
    is negotiated and an IOMMU device is declared.

    However, this feature enables vhost-user's reply-ack protocol feature,
    which implementation is buggy in Qemu v2.7.0-v2.9.0 when doing multiqueue.
    Enabling this flag with these Qemu version results in Qemu being blocked
    when multiple queue pairs are declared.

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
       nothing will work and issues undefined might happen.

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

  For non-zerocopy, guest memory pre-allocation is not a must. This can help
  save of memory. If users really want the guest memory to be pre-allocated
  (e.g., for performance reason), we can add option ``-mem-prealloc`` when
  starting QEMU. Or, we can lock all memory at vhost side which will force
  memory to be allocated when mmap at vhost side; option --mlockall in
  ovs-dpdk is an example in hand.

  For zerocopy, we force the VM memory to be pre-allocated at vhost lib when
  mapping the guest memory; and also we need to lock the memory to prevent
  pages being swapped out to disk.

* Memory sharing

  Make sure ``share=on`` QEMU option is given. vhost-user will not work with
  a QEMU version without shared memory mapping.

Vhost supported vSwitch reference
---------------------------------

For more vhost details and how to support vhost in vSwitch, please refer to
the vhost example in the DPDK Sample Applications Guide.
