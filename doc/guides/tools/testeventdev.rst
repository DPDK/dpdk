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


Eventdev Tests
--------------

ORDER_QUEUE Test
~~~~~~~~~~~~~~~~

This is a functional test case that aims at testing the following:

#. Verify the ingress order maintenance.
#. Verify the exclusive(atomic) access to given atomic flow per eventdev port.

.. _table_eventdev_order_queue_test:

.. table:: Order queue test eventdev configuration.

   +---+--------------+----------------+------------------------+
   | # | Items        | Value          | Comments               |
   |   |              |                |                        |
   +===+==============+================+========================+
   | 1 | nb_queues    | 2              | q0(ordered), q1(atomic)|
   |   |              |                |                        |
   +---+--------------+----------------+------------------------+
   | 2 | nb_producers | 1              |                        |
   |   |              |                |                        |
   +---+--------------+----------------+------------------------+
   | 3 | nb_workers   | >= 1           |                        |
   |   |              |                |                        |
   +---+--------------+----------------+------------------------+
   | 4 | nb_ports     | nb_workers +   | Workers use port 0 to  |
   |   |              | 1              | port n-1. Producer uses|
   |   |              |                | port n                 |
   +---+--------------+----------------+------------------------+

.. _figure_eventdev_order_queue_test:

.. figure:: img/eventdev_order_queue_test.*

   order queue test operation.

The order queue test configures the eventdev with two queues and an event
producer to inject the events to q0(ordered) queue. Both q0(ordered) and
q1(atomic) are linked to all the workers.

The event producer maintains a sequence number per flow and injects the events
to the ordered queue. The worker receives the events from ordered queue and
forwards to atomic queue. Since the events from an ordered queue can be
processed in parallel on the different workers, the ingress order of events
might have changed on the downstream atomic queue enqueue. On enqueue to the
atomic queue, the eventdev PMD driver reorders the event to the original
ingress order(i.e producer ingress order).

When the event is dequeued from the atomic queue by the worker, this test
verifies the expected sequence number of associated event per flow by comparing
the free running expected sequence number per flow.

Application options
^^^^^^^^^^^^^^^^^^^

Supported application command line options are following::

   --verbose
   --dev
   --test
   --socket_id
   --pool_sz
   --plcores
   --wlcores
   --nb_flows
   --nb_pkts
   --worker_deq_depth

Example
^^^^^^^

Example command to run order queue test:

.. code-block:: console

   sudo build/app/dpdk-test-eventdev --vdev=event_sw0 -- \
                --test=order_queue --plcores 1 --wlcores 2,3



