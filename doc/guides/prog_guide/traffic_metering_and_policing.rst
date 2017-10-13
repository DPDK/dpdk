..  BSD LICENSE
    Copyright(c) 2017 Intel Corporation. All rights reserved.
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


Traffic Metering and Policing API
=================================


Overview
--------

This is the generic API for the Quality of Service (QoS) Traffic Metering and
Policing (MTR) of Ethernet devices. This API is agnostic of the underlying HW,
SW or mixed HW-SW implementation.

The main features are:

* Part of DPDK rte_ethdev API
* Capability query API
* Metering algorithms: RFC 2697 Single Rate Three Color Marker (srTCM), RFC 2698
  and RFC 4115 Two Rate Three Color Marker (trTCM)
* Policer actions (per meter output color): recolor, drop
* Statistics (per policer output color)

Configuration steps
-------------------

The metering and policing stage typically sits on top of flow classification,
which is why the MTR objects are enabled through a special "meter" action.

The MTR objects are created and updated in their own name space (``rte_mtr``)
within the ``librte_ether`` library. Whether an MTR object is private to a
flow or potentially shared by several flows has to be specified at its
creation time.

Once successfully created, an MTR object is hooked into the RX processing path
of the Ethernet device by linking it to one or several flows through the
dedicated "meter" flow action. One or several "meter" actions can be registered
for the same flow. An MTR object can only be destroyed if there are no flows
using it.

Run-time processing
-------------------

Traffic metering determines the color for the current packet (green, yellow,
red) based on the previous history for this flow as maintained by the MTR
object. The policer can do nothing, override the color the packet or drop the
packet. Statistics counters are maintained for MTR object, as configured.

The processing done for each input packet hitting an MTR object is:

* Traffic metering: The packet is assigned a color (the meter output color)
  based on the previous traffic history reflected in the current state of the
  MTR object, according to the specific traffic metering algorithm. The
  traffic metering algorithm can typically work in color aware mode, in which
  case the input packet already has an initial color (the input color), or in
  color blind mode, which is equivalent to considering all input packets
  initially colored as green.

* Policing: There is a separate policer action configured for each meter
  output color, which can:

  * Drop the packet.

  * Keep the same packet color: the policer output color matches the meter
    output color (essentially a no-op action).

  * Recolor the packet: the policer output color is set to a different color
    than the meter output color. The policer output color is the output color
    of the packet, which is set in the packet meta-data (i.e. struct
    ``rte_mbuf::sched::color``).

* Statistics: The set of counters maintained for each MTR object is
  configurable and subject to the implementation support. This set includes
  the number of packets and bytes dropped or passed for each output color.
