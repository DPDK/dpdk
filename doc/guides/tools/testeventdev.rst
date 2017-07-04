..  BSD LICENSE
    Copyright(c) 2017 Cavium. All rights reserved.
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
    * Neither the name of Cavium nor the names of its
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

dpdk-test-eventdev Application
==============================

The ``dpdk-test-eventdev`` tool is a Data Plane Development Kit (DPDK)
application that allows exercising various eventdev use cases.
This application has a generic framework to add new eventdev based test cases to
verify functionality and measure the performance parameters of DPDK eventdev
devices.

Compiling the Application
-------------------------

**Build the application**

Execute the ``dpdk-setup.sh`` script to build the DPDK library together with the
``dpdk-test-eventdev`` application.

Initially, the user must select a DPDK target to choose the correct target type
and compiler options to use when building the libraries.
The user must have all libraries, modules, updates and compilers installed
in the system prior to this,
as described in the earlier chapters in this Getting Started Guide.

Running the Application
-----------------------

The application has a number of command line options:

.. code-block:: console

   dpdk-test-eventdev [EAL Options] -- [application options]

EAL Options
~~~~~~~~~~~

The following are the EAL command-line options that can be used in conjunction
with the ``dpdk-test-eventdev`` application.
See the DPDK Getting Started Guides for more information on these options.

*   ``-c <COREMASK>`` or ``-l <CORELIST>``

        Set the hexadecimal bitmask of the cores to run on. The corelist is a
        list of cores to use.

*   ``--vdev <driver><id>``

        Add a virtual eventdev device.

Application Options
~~~~~~~~~~~~~~~~~~~

The following are the application command-line options:

* ``--verbose``

        Set verbose level. Default is 1. Value > 1 displays more details.

* ``--dev <n>``

        Set the device id of the event device.

* ``--test <name>``

        Set test name, where ``name`` is one of the following::

         order_queue
         order_atq
         perf_queue
         perf_atq

* ``--socket_id <n>``

        Set the socket id of the application resources.

* ``--pool-sz <n>``

        Set the number of mbufs to be allocated from the mempool.

* ``--slcore <n>``

        Set the scheduler lcore id.(Valid when eventdev is not RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED capable)

* ``--plcores <CORELIST>``

        Set the list of cores to be used as producers.

* ``--wlcores <CORELIST>``

        Set the list of cores to be used as workers.

* ``--stlist <type_list>``

        Set the scheduled type of each stage where ``type_list`` size
        determines the number of stages used in the test application.
        Each type_list member can be one of the following::

            P or p : Parallel schedule type
            O or o : Ordered schedule type
            A or a : Atomic schedule type

        Application expects the ``type_list`` in comma separated form (i.e. ``--stlist o,a,a,a``)

* ``--nb_flows <n>``

        Set the number of flows to produce.

* ``--nb_pkts <n>``

        Set the number of packets to produce. 0 implies no limit.

* ``--worker_deq_depth <n>``

        Set the dequeue depth of the worker.

* ``--fwd_latency``

        Perform forward latency measurement.

* ``--queue_priority``

        Enable queue priority.

