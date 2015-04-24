..  BSD LICENSE
    Copyright(c) 2015 Intel Corporation. All rights reserved.
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

Packet Ordering Application
============================

The Packet Ordering sample app simply shows the impact of reordering a stream.
It's meant to stress the library with different configurations for performance.

Overview
--------

The application uses at least three CPU cores:

* RX core (maser core) receives traffic from the NIC ports and feeds Worker
  cores with traffic through SW queues.

* Worker core (slave core) basically do some light work on the packet.
  Currently it modifies the output port of the packet for configurations with
  more than one port enabled.

* TX Core (slave core) receives traffic from Worker cores through software queues,
  inserts out-of-order packets into reorder buffer, extracts ordered packets
  from the reorder buffer and sends them to the NIC ports for transmission.

Compiling the Application
--------------------------

#.  Go to the example directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/helloworld

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the *DPDK Getting Started* Guide for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        make

Running the Application
-----------------------

Refer to *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Application Command Line
~~~~~~~~~~~~~~~~~~~~~~~~

The application execution command line is:

.. code-block:: console

    ./test-pipeline [EAL options] -- -p PORTMASK [--disable-reorder]

The -c EAL CPU_COREMASK option has to contain at least 3 CPU cores.
The first CPU core in the core mask is the master core and would be assigned to
RX core, the last to TX core and the rest to Worker cores.

The PORTMASK parameter must contain either 1 or even enabled port numbers.
When setting more than 1 port, traffic would be forwarded in pairs.
For example, if we enable 4 ports, traffic from port 0 to 1 and from 1 to 0,
then the other pair from 2 to 3 and from 3 to 2, having [0,1] and [2,3] pairs.

The disable-reorder long option does, as its name implies, disable the reordering
of traffic, which should help evaluate reordering performance impact.
