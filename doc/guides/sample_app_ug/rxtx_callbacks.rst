..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

RX/TX Callbacks Sample Application
==================================

The RX/TX Callbacks sample application is a packet forwarding application that
demonstrates the use of user defined callbacks on received and transmitted
packets. The application performs a simple latency check, using callbacks, to
determine the time packets spend within the application.

In the sample application a user defined callback is applied to all received
packets to add a timestamp. A separate callback is applied to all packets
prior to transmission to calculate the elapsed time, in CPU cycles.

If hardware timestamping is supported by the NIC, the sample application will
also display the average latency since the packet was timestamped in hardware,
on top of the latency since the packet was received and processed by the RX
callback.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``rxtx_callbacks`` sub-directory.


Running the Application
-----------------------

To run the example in a ``linux`` environment:

.. code-block:: console

    ./<build_dir>/examples/dpdk-rxtx_callbacks -l 1 -n 4 -- [-t]

Use -t to enable hardware timestamping. If not supported by the NIC, an error
will be displayed.

Refer to *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.



Explanation
-----------

The ``rxtx_callbacks`` application is mainly a simple forwarding application
based on the :doc:`skeleton`. See that section of the documentation for more
details of the forwarding part of the application.

The sections below explain the additional RX/TX callback code.


The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function performs the application initialization and calls the
execution threads for each lcore. This function is effectively identical to
the ``main()`` function explained in :doc:`skeleton`.

The ``lcore_main()`` function is also identical.

The main difference is in the user defined ``port_init()`` function where the
callbacks are added. This is explained in the next section:


The Port Initialization  Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The main functional part of the port initialization is shown below with
comments:

.. literalinclude:: ../../../examples/rxtx_callbacks/main.c
    :language: c
    :start-after: Port initialization. 8<
    :end-before: >8 End of port initialization.


The RX and TX callbacks are added to the ports/queues as function pointers:

.. literalinclude:: ../../../examples/rxtx_callbacks/main.c
    :language: c
    :start-after: RX and TX callbacks are added to the ports. 8<
    :end-before: >8 End of RX and TX callbacks.
    :dedent: 1

More than one callback can be added and additional information can be passed
to callback function pointers as a ``void*``. In the examples above ``NULL``
is used.

The ``add_timestamps()`` and ``calc_latency()`` functions are explained below.


The add_timestamps() Callback
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``add_timestamps()`` callback is added to the RX port and is applied to
all packets received:

.. literalinclude:: ../../../examples/rxtx_callbacks/main.c
    :language: c
    :start-after: Callback added to the RX port and applied to packets. 8<
    :end-before: >8 End of callback addition and application.

The DPDK function ``rte_rdtsc()`` is used to add a cycle count timestamp to
each packet (see the *cycles* section of the *DPDK API Documentation* for
details).


The calc_latency() Callback
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``calc_latency()`` callback is added to the TX port and is applied to all
packets prior to transmission:

.. literalinclude:: ../../../examples/rxtx_callbacks/main.c
    :language: c
    :start-after: Callback is added to the TX port. 8<
    :end-before: >8 End of callback addition.

The ``calc_latency()`` function accumulates the total number of packets and
the total number of cycles used. Once more than 100 million packets have been
transmitted the average cycle count per packet is printed out and the counters
are reset.
