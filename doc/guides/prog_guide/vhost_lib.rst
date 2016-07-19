..  BSD LICENSE
    Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
  messages to tell the backend all the information it needs to know how to
  manipulate the vring.

Currently, there are two ways to pass these messages and as a result there are
two Vhost implementations in DPDK: *vhost-cuse* (where the character devices
are in user space) and *vhost-user*.

Vhost-cuse creates a user space character device and hook to a function ioctl,
so that all ioctl commands that are sent from the frontend (QEMU) will be
captured and handled.

Vhost-user creates a Unix domain socket file through which messages are
passed.

.. Note::

   Since DPDK v2.2, the majority of the development effort has gone into
   enhancing vhost-user, such as multiple queue, live migration, and
   reconnect. Thus, it is strongly advised to use vhost-user instead of
   vhost-cuse.


Vhost API Overview
------------------

The following is an overview of the Vhost API functions:

* ``rte_vhost_driver_register(path, flags)``

  This function registers a vhost driver into the system. For vhost-cuse, a
  ``/dev/path`` character device file will be created. For vhost-user server
  mode, a Unix domain socket file ``path`` will be created.

  Currently two flags are supported (these are valid for vhost-user only):

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

* ``rte_vhost_driver_session_start()``

  This function starts the vhost session loop to handle vhost messages. It
  starts an infinite loop, therefore it should be called in a dedicated
  thread.

* ``rte_vhost_driver_callback_register(virtio_net_device_ops)``

  This function registers a set of callbacks, to let DPDK applications take
  the appropriate action when some events happen. The following events are
  currently supported:

  * ``new_device(int vid)``

    This callback is invoked when a virtio net device becomes ready. ``vid``
    is the virtio net device ID.

  * ``destroy_device(int vid)``

    This callback is invoked when a virtio net device shuts down (or when the
    vhost connection is broken).

  * ``vring_state_changed(int vid, uint16_t queue_id, int enable)``

    This callback is invoked when a specific queue's state is changed, for
    example to enabled or disabled.

* ``rte_vhost_enqueue_burst(vid, queue_id, pkts, count)``

  Transmits (enqueues) ``count`` packets from host to guest.

* ``rte_vhost_dequeue_burst(vid, queue_id, mbuf_pool, pkts, count)``

  Receives (dequeues) ``count`` packets from guest, and stored them at ``pkts``.

* ``rte_vhost_feature_disable/rte_vhost_feature_enable(feature_mask)``

  This function disables/enables some features. For example, it can be used to
  disable mergeable buffers and TSO features, which both are enabled by
  default.


Vhost Implementations
---------------------

Vhost-cuse implementation
~~~~~~~~~~~~~~~~~~~~~~~~~

When vSwitch registers the vhost driver, it will register a cuse device driver
into the system and creates a character device file. This cuse driver will
receive vhost open/release/IOCTL messages from the QEMU simulator.

When the open call is received, the vhost driver will create a vhost device
for the virtio device in the guest.

When the ``VHOST_SET_MEM_TABLE`` ioctl is received, vhost searches the memory
region to find the starting user space virtual address that maps the memory of
the guest virtual machine. Through this virtual address and the QEMU pid,
vhost can find the file QEMU uses to map the guest memory. Vhost maps this
file into its address space, in this way vhost can fully access the guest
physical memory, which means vhost could access the shared virtio ring and the
guest physical address specified in the entry of the ring.

The guest virtual machine tells the vhost whether the virtio device is ready
for processing or is de-activated through the ``VHOST_NET_SET_BACKEND``
message. The registered callback from vSwitch will be called.

When the release call is made, vhost will destroy the device.

Vhost-user implementation
~~~~~~~~~~~~~~~~~~~~~~~~~

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

There is no ``VHOST_NET_SET_BACKEND`` message as in vhost-cuse to signal
whether the virtio device is ready or stopped. Instead,
``VHOST_SET_VRING_KICK`` is used as the signal to put the vhost device into
the data plane, and ``VHOST_GET_VRING_BASE`` is used as the signal to remove
the vhost device from the data plane.

When the socket connection is closed, vhost will destroy the device.

Vhost supported vSwitch reference
---------------------------------

For more vhost details and how to support vhost in vSwitch, please refer to
the vhost example in the DPDK Sample Applications Guide.
