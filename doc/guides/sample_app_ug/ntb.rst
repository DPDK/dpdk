..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

NTB Sample Application
======================

The ntb sample application shows how to use ntb rawdev driver.
This sample provides interactive mode to do packet based processing
between two systems.

This sample supports 4 types of packet forwarding mode.

* ``file-trans``: transmit files between two systems. The sample will
  be polling to receive files from the peer and save the file as
  ``ntb_recv_file[N]``, [N] represents the number of received file.
* ``rxonly``: NTB receives packets but doesn't transmit them.
* ``txonly``: NTB generates and transmits packets without receiving any.
* ``iofwd``: iofwd between NTB device and ethdev.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``ntb`` sub-directory.

Running the Application
-----------------------

The application requires an available core for each port, plus one.
The only available options are the standard ones for the EAL:

.. code-block:: console

    ./build/ntb_fwd -c 0xf -n 6 -- -i

Refer to the *DPDK Getting Started Guide* for general information on
running applications and the Environment Abstraction Layer (EAL)
options.

Command-line Options
--------------------

The application supports the following command-line options.

* ``--buf-size=N``

  Set the data size of the mbufs used to N bytes, where N < 65536.
  The default value is 2048.

* ``--fwd-mode=mode``

  Set the packet forwarding mode as ``file-trans``, ``txonly``,
  ``rxonly`` or ``iofwd``.

* ``--nb-desc=N``

  Set number of descriptors of queue as N, namely queue size,
  where 64 <= N <= 1024. The default value is 1024.

* ``--txfreet=N``

  Set the transmit free threshold of TX rings to N, where 0 <= N <=
  the value of ``--nb-desc``. The default value is 256.

* ``--burst=N``

  Set the number of packets per burst to N, where 1 <= N <= 32.
  The default value is 32.

* ``--qp=N``

  Set the number of queues as N, where qp > 0. The default value is 1.

Using the application
---------------------

The application is console-driven using the cmdline DPDK interface:

.. code-block:: console

        ntb>

From this interface the available commands and descriptions of what
they do as follows:

* ``send [filepath]``: Send file to the peer host. Need to be in
  file-trans forwarding mode first.
* ``start``: Start transmission.
* ``stop``: Stop transmission.
* ``show/clear port stats``: Show/Clear port stats and throughput.
* ``set fwd file-trans/rxonly/txonly/iofwd``: Set packet forwarding
  mode.
* ``quit``: Exit program.
