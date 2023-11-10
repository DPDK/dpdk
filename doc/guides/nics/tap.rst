..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

Tun|Tap Poll Mode Driver
========================

The ``rte_eth_tap.c`` PMD creates a device using TAP interfaces on the
local host. The PMD allows for DPDK and the host to communicate using a raw
device interface on the host and in the DPDK application.

The device created is a TAP device, which sends/receives packet in a raw
format with a L2 header. The usage for a TAP PMD is for connectivity to the
local host using a TAP interface. When the TAP PMD is initialized it will
create a number of tap devices in the host accessed via ``ifconfig -a`` or
``ip`` command. The commands can be used to assign and query the virtual like
device.

These TAP interfaces can be used with Wireshark or tcpdump or Pktgen-DPDK
along with being able to be used as a network connection to the DPDK
application. The method enable one or more interfaces is to use the
``--vdev=net_tap0`` option on the DPDK application command line. Each
``--vdev=net_tap1`` option given will create an interface named dtap0, dtap1,
and so on.

The interface name can be changed by adding the ``iface=foo0``, for example::

   --vdev=net_tap0,iface=foo0 --vdev=net_tap1,iface=foo1, ...

Normally the PMD will generate a random MAC address, but when testing or with
a static configuration the developer may need a fixed MAC address style.
Using the option ``mac=fixed`` you can create a fixed known MAC address::

   --vdev=net_tap0,mac=fixed

The MAC address will have a fixed value with the last octet incrementing by one
for each interface string containing ``mac=fixed``. The MAC address is formatted
as 02:'d':'t':'a':'p':[00-FF]. Convert the characters to hex and you get the
actual MAC address: ``02:64:74:61:70:[00-FF]``.

   --vdev=net_tap0,mac="02:64:74:61:70:11"

The MAC address will have a user value passed as string. The MAC address is in
format with delimiter ``:``. The string is byte converted to hex and you get
the actual MAC address: ``02:64:74:61:70:11``.

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

After the DPDK application is started you can send and receive packets on the
interface using the standard rx_burst/tx_burst APIs in DPDK. From the host
point of view you can use any host tool like tcpdump, Wireshark, ping, Pktgen
and others to communicate with the DPDK application. The DPDK application may
not understand network protocols like IPv4/6, UDP or TCP unless the
application has been written to understand these protocols.

If you need the interface as a real network interface meaning running and has
a valid IP address then you can do this with the following commands::

   sudo ip link set dtap0 up; sudo ip addr add 192.168.0.250/24 dev dtap0
   sudo ip link set dtap1 up; sudo ip addr add 192.168.1.250/24 dev dtap1

Please change the IP addresses as you see fit.

If routing is enabled on the host you can also communicate with the DPDK App
over the internet via a standard socket layer application as long as you
account for the protocol handling in the application.

If you have a Network Stack in your DPDK application or something like it you
can utilize that stack to handle the network protocols. Plus you would be able
to address the interface using an IP address assigned to the internal
interface.

Normally, when the DPDK application exits,
the TAP device is marked down and is removed.
But this behaviour can be overridden by the use of the persist flag, example::

  --vdev=net_tap0,iface=tap0,persist ...

The TUN PMD allows user to create a TUN device on host. The PMD allows user
to transmit and receive packets via DPDK API calls with L3 header and payload.
The devices in host can be accessed via ``ifconfig`` or ``ip`` command. TUN
interfaces are passed to DPDK ``rte_eal_init`` arguments as ``--vdev=net_tunX``,
where X stands for unique id, example::

   --vdev=net_tun0 --vdev=net_tun1,iface=foo1, ...

Unlike TAP PMD, TUN PMD does not support user arguments as ``MAC`` or ``remote`` user
options. Default interface name is ``dtunX``, where X stands for unique id.

Flow API support
----------------

The tap PMD supports major flow API pattern items and actions, when running on
linux kernels above 4.2 ("Flower" classifier required).
The kernel support can be checked with this command::

   zcat /proc/config.gz | ( grep 'CLS_FLOWER=' || echo 'not supported' ) |
   tee -a /dev/stderr | grep -q '=m' &&
   lsmod | ( grep cls_flower || echo 'try modprobe cls_flower' )

Supported items:

- eth: src and dst (with variable masks), and eth_type (0xffff mask).
- vlan: vid, pcp, but not eid. (requires kernel 4.9)
- ipv4/6: src and dst (with variable masks), and ip_proto (0xffff mask).
- udp/tcp: src and dst port (0xffff) mask.

Supported actions:

- DROP
- QUEUE
- PASSTHRU
- RSS (requires kernel 4.9)

It is generally not possible to provide a "last" item. However, if the "last"
item, once masked, is identical to the masked spec, then it is supported.

Only IPv4/6 and MAC addresses can use a variable mask. All other items need a
full mask (exact match).

As rules are translated to TC, it is possible to show them with something like::

   tc -s filter show dev tap1 parent 1:

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

Example
-------

The following is a simple example of using the TAP PMD with the Pktgen
packet generator. It requires that the ``socat`` utility is installed on the
test system.

Build DPDK, then pull down Pktgen and build pktgen using the DPDK SDK/Target
used to build the dpdk you pulled down.

Run pktgen from the pktgen directory in a terminal with a commandline like the
following::

    sudo ./app/app/x86_64-native-linux-gcc/app/pktgen -l 1-5 -n 4        \
     --proc-type auto --log-level debug --socket-mem 512,512 --file-prefix pg   \
     --vdev=net_tap0 --vdev=net_tap1 -b 05:00.0 -b 05:00.1                  \
     -b 04:00.0 -b 04:00.1 -b 04:00.2 -b 04:00.3                            \
     -b 81:00.0 -b 81:00.1 -b 81:00.2 -b 81:00.3                            \
     -b 82:00.0 -b 83:00.0 -- -T -P -m [2:3].0 -m [4:5].1                   \
     -f themes/black-yellow.theme

.. Note:

   Change the ``-b`` options to exclude all of your physical ports. The
   following command line is all one line.

   Also, ``-f themes/black-yellow.theme`` is optional if the default colors
   work on your system configuration. See the Pktgen docs for more
   information.

Verify with ``ifconfig -a`` command in a different xterm window, should have a
``dtap0`` and ``dtap1`` interfaces created.

Next set the links for the two interfaces to up via the commands below::

    sudo ip link set dtap0 up; sudo ip addr add 192.168.0.250/24 dev dtap0
    sudo ip link set dtap1 up; sudo ip addr add 192.168.1.250/24 dev dtap1

Then use socat to create a loopback for the two interfaces::

    sudo socat interface:dtap0 interface:dtap1

Then on the Pktgen command line interface you can start sending packets using
the commands ``start 0`` and ``start 1`` or you can start both at the same
time with ``start all``. The command ``str`` is an alias for ``start all`` and
``stp`` is an alias for ``stop all``.

While running you should see the 64 byte counters increasing to verify the
traffic is being looped back. You can use ``set all size XXX`` to change the
size of the packets after you stop the traffic. Use pktgen ``help``
command to see a list of all commands. You can also use the ``-f`` option to
load commands at startup in command line or Lua script in pktgen.

RSS specifics
-------------
Packet distribution in TAP is done by the kernel which has a default
distribution. This feature is adding RSS distribution based on eBPF code.
The default eBPF code calculates RSS hash based on Toeplitz algorithm for
a fixed RSS key. It is calculated on fixed packet offsets. For IPv4 and IPv6 it
is calculated over src/dst addresses (8 or 32 bytes for IPv4 or IPv6
respectively) and src/dst TCP/UDP ports (4 bytes).

The RSS algorithm is written in file ``tap_bpf_program.c`` which
does not take part in TAP PMD compilation. Instead this file is compiled
in advance to eBPF object file. The eBPF object file is then parsed and
translated into eBPF byte code in the format of C arrays of eBPF
instructions. The C array of eBPF instructions is part of TAP PMD tree and
is taking part in TAP PMD compilation. At run time the C arrays are uploaded to
the kernel via BPF system calls and the RSS hash is calculated by the
kernel.

It is possible to support different RSS hash algorithms by updating file
``tap_bpf_program.c``  In order to add a new RSS hash algorithm follow these
steps:

#. Write the new RSS implementation in file ``tap_bpf_program.c``

   BPF programs which are uploaded to the kernel correspond to
   C functions under different ELF sections.

#. Install ``LLVM`` library and ``clang`` compiler versions 3.7 and above

#. Use make to compile  `tap_bpf_program.c`` via ``LLVM`` into an object file
   and extract the resulting instructions into ``tap_bpf_insn.h``::

    cd bpf; make

#. Recompile the TAP PMD.

The C arrays are uploaded to the kernel using BPF system calls.

``tc`` (traffic control) is a well known user space utility program used to
configure the Linux kernel packet scheduler. It is usually packaged as
part of the ``iproute2`` package.
Since commit 11c39b5e9 ("tc: add eBPF support to f_bpf") ``tc`` can be used
to uploads eBPF code to the kernel and can be patched in order to print the
C arrays of eBPF instructions just before calling the BPF system call.
Please refer to ``iproute2`` package file ``lib/bpf.c`` function
``bpf_prog_load()``.

An example utility for eBPF instruction generation in the format of C arrays will
be added in next releases

TAP reports on supported RSS functions as part of dev_infos_get callback:
``RTE_ETH_RSS_IP``, ``RTE_ETH_RSS_UDP`` and ``RTE_ETH_RSS_TCP``.
**Known limitation:** TAP supports all of the above hash functions together
and not in partial combinations.

Systems supporting flow API
---------------------------

- "tc flower" classifier requires linux kernel above 4.2
- eBPF/RSS requires linux kernel above 4.9

+--------------------+-----------------------+
| RH7.3              | No flow rule support  |
+--------------------+-----------------------+
| RH7.4              | No RSS action support |
+--------------------+-----------------------+
| RH7.5              | No RSS action support |
+--------------------+-----------------------+
| SLES 15,           | No limitation         |
| kernel 4.12        |                       |
+--------------------+-----------------------+
| Azure Ubuntu 16.04,| No limitation         |
| kernel 4.13        |                       |
+--------------------+-----------------------+

Limitations
-----------

* Rx/Tx must have the same number of queues.
