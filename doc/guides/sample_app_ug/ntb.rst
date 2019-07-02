..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

NTB Sample Application
======================

The ntb sample application shows how to use ntb rawdev driver.
This sample provides interactive mode to transmit file between
two hosts.

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

Using the application
---------------------

The application is console-driven using the cmdline DPDK interface:

.. code-block:: console

        ntb>

From this interface the available commands and descriptions of what
they do as as follows:

* ``send [filepath]``: Send file to the peer host.
* ``receive [filepath]``: Receive file to [filepath]. Need the peer
  to send file successfully first.
* ``quit``: Exit program
