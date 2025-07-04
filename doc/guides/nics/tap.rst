..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

TAP Poll Mode Driver
====================

The TAP Poll Mode Driver (PMD) is a virtual device for injecting packets
to be processed by the Linux kernel.
This PMD is useful when writing DPDK application
for offloading network functionality (such as tunneling) from the kernel.

From the kernel point of view, the TAP device looks like a regular network interface.
The network device can be managed by standard tools such as ``ip`` and ``ethtool`` commands.
It is also possible to use existing packet tools such as  ``wireshark`` or ``tcpdump``.

From the DPDK application, the TAP device looks like a DPDK ethdev.
Packets are sent and received in L2 (Ethernet) format.
The standard DPDK API's to query for information, statistics and send/receive packets
work as expected.


Requirements
------------

The TAP PMD requires kernel support for multiple queues in TAP device
as well as the multi-queue ``multiq`` and incoming ``ingress`` queue disciplines.
These are standard kernel features in most Linux distributions.


Arguments
---------

TAP devices are created with the command line ``--vdev=net_tap0`` option.
This option may be specified more than once by repeating with a different ``net_tapX`` device.

By default, the Linux interfaces are named ``dtap0``, ``dtap1``, etc.
The interface name can be specified by adding the ``iface=foo0``, for example::

   --vdev=net_tap0,iface=foo0 --vdev=net_tap1,iface=foo1, ...

Normally the PMD will generate a random MAC address.
If a static address is desired instead, the ``mac=fixed`` can be used::

   --vdev=net_tap0,mac=fixed

With the fixed option, the MAC address will have the first octets:
as 02:'d':'t':'a':'p':[00-FF] and the last octets are the interface number.

To specify a specific MAC address use the conventional representation.
The string is byte converted to hex, the result is MAC address: ``02:64:74:61:70:11``.

It is possible to specify a remote netdevice to capture packets from by adding
``remote=foo1``, for example::

   --vdev=net_tap,iface=tap0,remote=foo1

If a ``remote`` is set, the tap MAC address will be set to match the remote one
just after netdevice creation. Using TC rules, traffic from the remote netdevice
will be redirected to the tap. If the tap is in promiscuous mode, then all
packets will be redirected. In allmulti mode, all multicast packets will be
redirected.

Using the remote feature is especially useful for capturing traffic from a
netdevice that has no support in the DPDK. It is possible to add explicit
rte_flow rules on the tap PMD to capture specific traffic (see next section for
examples).

Normally, when the DPDK application exits,
the TAP device is marked down and is removed.
But this behavior can be overridden by the use of the persist flag, example::

  --vdev=net_tap0,iface=tap0,persist ...


TUN devices
-----------

The TAP device can be used as an L3 tunnel only device (TUN).
This type of device does not include the Ethernet (L2) header;
all packets are sent and received as IP packets.

TUN devices are created with the command line arguments ``--vdev=net_tunX``,
where X stands for unique id, example::

   --vdev=net_tun0 --vdev=net_tun1,iface=foo1, ...

Unlike TAP PMD, TUN PMD does not support user arguments as ``MAC`` or ``remote`` user
options. Default interface name is ``dtunX``, where X stands for unique id.


Flow API support
----------------

The TAP PMD supports major flow API pattern items and actions.

Requirements
~~~~~~~~~~~~

Flow support in TAP driver requires the Linux kernel support of
flow based traffic control filter ``flower``.
This was added in Linux 4.3 kernel.

The implementation of RSS action uses an eBPF module
that requires additional libraries and tools.
Building the RSS support requires the ``clang`` compiler
to compile the C code to BPF target;
``bpftool`` to convert the compiled BPF object to a header file;
and ``libbpf`` to load the eBPF action into the kernel.

Supported match items:

  - eth: src and dst (with variable masks), and eth_type (0xffff mask).
  - vlan: vid, pcp, but not eid. (requires kernel 4.9)
  - ipv4/6: src and dst (with variable masks), and ip_proto (0xffff mask).
  - udp/tcp: src and dst port (0xffff) mask.

Supported actions:

- DROP
- QUEUE
- PASSTHRU
- RSS

It is generally not possible to provide a "last" item. However, if the "last"
item, once masked, is identical to the masked spec, then it is supported.

Only IPv4/6 and MAC addresses can use a variable mask. All other items need a
full mask (exact match).

As rules are translated to TC, it is possible to show them with something like::

   tc -s filter show dev dtap1 parent 1:

Examples of testpmd flow rules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Drop packets for destination IP 192.0.2.1::

   testpmd> flow create 0 priority 1 ingress pattern eth / ipv4 dst is 192.0.2.1 \
            / end actions drop / end

Ensure packets from a given MAC address are received on a queue 2::

   testpmd> flow create 0 priority 2 ingress pattern eth src is 06:05:04:03:02:01 \
            / end actions queue index 2 / end

Drop UDP packets in vlan 3::

   testpmd> flow create 0 priority 3 ingress pattern eth / vlan vid is 3 / \
            ipv4 proto is 17 / end actions drop / end

Distribute IPv4 TCP packets using RSS to a given MAC address over queues 0-3::

   testpmd> flow create 0 priority 4 ingress pattern eth dst is 0a:0b:0c:0d:0e:0f \
            / ipv4 / tcp / end actions rss queues 0 1 2 3 end / end


Multi-process sharing
---------------------

It is possible to attach an existing TAP device in a secondary process,
by declaring it as a vdev with the same name as in the primary process,
and without any parameter.

The port attached in a secondary process will give access to the
statistics and the queues.
Therefore it can be used for monitoring or Rx/Tx processing.

The IPC synchronization of Rx/Tx queues is currently limited:

  - Maximum 8 queues shared
  - Synchronized on probing, but not on later port update


RSS specifics
-------------

The default packet distribution in TAP without flow rules
is done by the kernel which has a default flow based distribution.
When flow rules are used to distribute packets across a set of queues,
an eBPF program is used to calculate the RSS based on Toeplitz algorithm
with the given key.

The hash is calculated for IPv4 and IPv6,
over src/dst addresses (8 or 32 bytes for IPv4 or IPv6 respectively)
and optionally the src/dst TCP/UDP ports (4 bytes).


Limitations
-----------

- Since TAP device uses a file descriptor to talk to the kernel,
  the same number of queues must be specified for receive and transmit.

- The RSS algorithm only support L3 or L4 functions.
  It does not support finer grain selections
  (for example: only IPV6 packets with extension headers).
