..  BSD LICENSE
    Copyright(c) 2016 Intel Corporation. All rights reserved.
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

Tun/Tap Poll Mode Driver
========================

The ``rte_eth_tap.c`` PMD creates a device using TUN/TAP interfaces on the
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

Also the speed of the interface can be changed from 10G to whatever number
needed, but the interface does not enforce that speed, for example::

   --vdev=net_tap0,iface=foo0,speed=25000

Normally the PMD will generate a random MAC address, but when testing or with
a static configuration the developer may need a fixed MAC address style.
Using the option ``mac=fixed`` you can create a fixed known MAC address::

   --vdev=net_tap0,mac=fixed

The MAC address will have a fixed value with the last octet incrementing by one
for each interface string containing ``mac=fixed``. The MAC address is formatted
as 00:'d':'t':'a':'p':[00-FF]. Convert the characters to hex and you get the
actual MAC address: ``00:64:74:61:70:[00-FF]``.

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
account for the protocol handing in the application.

If you have a Network Stack in your DPDK application or something like it you
can utilize that stack to handle the network protocols. Plus you would be able
to address the interface using an IP address assigned to the internal
interface.

Flow API support
----------------

The tap PMD supports major flow API pattern items and actions, when running on
linux kernels above 4.2 ("Flower" classifier required). Supported items:

- eth: src and dst (with variable masks), and eth_type (0xffff mask).
- vlan: vid, pcp, tpid, but not eid. (requires kernel 4.9)
- ipv4/6: src and dst (with variable masks), and ip_proto (0xffff mask).
- udp/tcp: src and dst port (0xffff) mask.

Supported actions:

- DROP
- QUEUE
- PASSTHRU

It is generally not possible to provide a "last" item. However, if the "last"
item, once masked, is identical to the masked spec, then it is supported.

Only IPv4/6 and MAC addresses can use a variable mask. All other items need a
full mask (exact match).

As rules are translated to TC, it is possible to show them with something like::

   tc -s filter show dev tap1 parent 1:

Examples of testpmd flow rules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Drop packets for destination IP 192.168.0.1::

   testpmd> flow create 0 priority 1 ingress pattern eth / ipv4 dst is 1.1.1.1 \
            / end actions drop / end

Ensure packets from a given MAC address are received on a queue 2::

   testpmd> flow create 0 priority 2 ingress pattern eth src is 06:05:04:03:02:01 \
            / end actions queue index 2 / end

Drop UDP packets in vlan 3::

   testpmd> flow create 0 priority 3 ingress pattern eth / vlan vid is 3 / \
            ipv4 proto is 17 / end actions drop / end

Example
-------

The following is a simple example of using the TUN/TAP PMD with the Pktgen
packet generator. It requires that the ``socat`` utility is installed on the
test system.

Build DPDK, then pull down Pktgen and build pktgen using the DPDK SDK/Target
used to build the dpdk you pulled down.

Run pktgen from the pktgen directory in a terminal with a commandline like the
following::

    sudo ./app/app/x86_64-native-linuxapp-gcc/app/pktgen -l 1-5 -n 4        \
     --proc-type auto --log-level 8 --socket-mem 512,512 --file-prefix pg   \
     --vdev=net_tap0 --vdev=net_tap1 -b 05:00.0 -b 05:00.1                  \
     -b 04:00.0 -b 04:00.1 -b 04:00.2 -b 04:00.3                            \
     -b 81:00.0 -b 81:00.1 -b 81:00.2 -b 81:00.3                            \
     -b 82:00.0 -b 83:00.0 -- -T -P -m [2:3].0 -m [4:5].1                   \
     -f themes/black-yellow.theme

.. Note:

   Change the ``-b`` options to blacklist all of your physical ports. The
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
